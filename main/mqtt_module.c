#include "mqtt_module.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <queue.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h" // 절대 마감 구현시 사용
#include "OCPMessage.h"

extern const uint8_t insator_server_crt_pem_start[] asm("_binary_insator_server_crt_pem_start");
extern const uint8_t insator_server_crt_pem_end[]   asm("_binary_insator_server_crt_pem_end");
static const char *TAG = "mqtt_module";
static esp_mqtt_client_handle_t mqtt_client = NULL;  // 전역 클라이언트 핸들 저장
static TaskHandle_t s_pub_task_handle = NULL;
static volatile bool s_mqtt_connected = false;
#define MQTT_TEST_TOPIC "ocp/"
QUEUE* send_queue;
QUEUE* receive_queue;
static SemaphoreHandle_t sem_forConn; // binary semaphore
int thread_all_flag = 1;
int thread_conn_flag = 0;
// 태스크 핸들(필요 시 정지/삭제 용도)
static TaskHandle_t s_conn_task = NULL;
unsigned char 			auth_response[1024];	// for auth data

static bool wait_conn_with_timeout_ms(uint32_t timeout_ms)
{
    BaseType_t ok = xSemaphoreTake(sem_forConn, pdMS_TO_TICKS(timeout_ms));
    return (ok == pdTRUE);  // true면 신호 수신(=응답 옴), false면 타임아웃
}

void OCPManager_free_message(OCPMessage* message)
{
	if(message != NULL) 
	{ 
		if(message->data != NULL) 
		{ 
			free(message->data); 
			message->data = NULL; 
		}

		free(message); 
		message = NULL;
	}
}

void thread_keep_alive_task(void *arg)
{
    ESP_LOGI(TAG, "connection keep-alive task started");
    thread_conn_flag = 0;

    while (thread_all_flag) {
        // 여기에 주기적 ping/publish, reconnect 체크 등을 수행
        // 예: 연결 확인 → 미연결이면 재연결 트리거
        //     인가 승인 여부 체크 → 미인가면 authorize 요청 등

        // 예시 로그(지우셔도 됨)
        // ESP_LOGD(TAG, "tick.. conn_flag=%d", thread_conn_flag);
        wait_conn_with_timeout_ms(5000);
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5초 주기
    }

    ESP_LOGI(TAG, "connection keep-alive task exiting");
    vTaskDelete(NULL); // 자기 자신 삭제 (detach에 해당)
}

void thread_sender(void *arg) { // sendmessage 기능 추가
    while(thread_all_flag) {
        if(send_queue->size >0) {
            OCPMessage* message = (OCPMessage*) (send_queue->first->message);
            int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TEST_TOPIC,
                                                 message, 0 /*len=0면 strlen*/,
                                                 1 /*QoS1*/, 0 /*retain*/);
            if (msg_id >= 0) {
                ESP_LOGI(TAG, "Published dummy JSON to %s", MQTT_TEST_TOPIC);
                ESP_LOGI(TAG, "%s", message);
                OCPManager_free_message(message);
            } else {
                ESP_LOGW(TAG, "Publish failed (ret=%d)", msg_id);
                OCPManager_free_message(message);
            }
        } else {
            
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void thread_receiver(void* arg) {
    while(thread_all_flag) {
        if(receive_queue->size>0) {
            OCPMessage* message = (OCPMessage*) (send_queue->first->message);
            if(message)
			{
				if(!strcmp(message->msgCode, OCPMESSAGE_MSGCODE_AUTHPROCESS_RES))
				{ // AUTH_RESPONSE 는 암호화 안되어 있고, iot client 가 처리하고 끝
					memset(auth_response, 0, 1024);
					memcpy(auth_response, message->data, (int)strlen(message->data));

					int semval = -1;
					sem_getvalue(sem_forConn, &semval);
					if(semval == 0)
						sem_post(sem_forConn);
					
					OCPManager_free_message(message);
					fprintf(stderr, "[<<<] pre-process OCPMESSAGE_MSGCODE_AUTHPROCESS_RES\n");
				}
            }
        }
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            // 인증 메시지 생성 및 송신
            // 인증 메시지 수신, 인증 토큰 파싱
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            s_mqtt_connected = false;
            s_pub_task_handle = NULL;
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;

        case MQTT_EVENT_DATA :
            add_queue(receive_queue, event);
            break;
        default:
            break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://aiot.rda.go.kr:8001",
        .credentials.client_id = "C000000006+9016",
        .credentials.authentication.password = "63ed3bdd4c73f00f",
        .broker.verification.certificate = (const char *)insator_server_crt_pem_start,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1, 
        .session.keepalive = 60,
        .network.timeout_ms = 10000,
    };

    /** QUEUE 생성 시기에 대한 메모리 최적화 필요 */
    receive_queue = create_queue();
    send_queue = create_queue();

    /** semaphore 생성 시기에 대한 메모리 최적화 필요 */
    sem_forConn = xSemaphoreCreateBinary();
    configASSERT(sem_forConn != NULL);
    xSemaphoreTake(sem_forConn, 0);

    /** thread 생성 */
    if(xTaskCreate(thread_keep_alive_task, "conn_task", 4096, NULL, 5, &s_pub_task_handle)==pdPASS) {
        printf("Task created successfully!\n");
        }

    if(xTaskCreate(thread_sender, "send_task", 4096, NULL, 5, &s_pub_task_handle)==pdPASS) {
        printf("Task created successfully!\n");
        }

    if(xTaskCreate(thread_receiver, "recv_task", 4096, NULL, 5, &s_pub_task_handle)==pdPASS) {
        printf("Task created successfully!\n");
        }
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE("MQTT", "Failed to create MQTT client");
        return;
    } else {
        ESP_LOGI("MQTT", "MQTT client created successfully");
    }
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_err_t start_result = esp_mqtt_client_start(mqtt_client);
    if (start_result != ESP_OK) {
        ESP_LOGE("MQTT", "Failed to start MQTT client: 0x%x", start_result);
    } else {
        ESP_LOGI("MQTT", "MQTT client started successfully");
    }   
}

