#include "mqtt_module.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_sntp.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <queue.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h" // 절대 마감 구현시 사용
#include "OCPMessage.h"
#include <esp_netif_sntp.h>
#include "esp_mac.h"
#include "cJSON.h"

extern const uint8_t insator_server_crt_pem_start[] asm("_binary_insator_server_crt_pem_start");
extern const uint8_t insator_server_crt_pem_end[] asm("_binary_insator_server_crt_pem_end");
static const char *TAG = "mqtt_module";
static esp_mqtt_client_handle_t mqtt_client = NULL; // 전역 클라이언트 핸들 저장
static TaskHandle_t s_pub_task_handle = NULL;
static volatile bool s_mqtt_connected = false;
#define MQTT_TEST_TOPIC "ocp/C000000006/9016"
QUEUE *send_queue;
QUEUE *receive_queue;
static SemaphoreHandle_t sem_forConn; // binary semaphore
int thread_all_flag = 1;
int thread_conn_flag = 0;
// 태스크 핸들(필요 시 정지/삭제 용도)
static TaskHandle_t s_conn_task = NULL;
char *auth_payload;
cJSON *root;

unsigned long long OCPUtils_getCurrentTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL); // NTP 동기화 후 Epoch 기반 시간 반환
    unsigned long long millisecondsSinceEpoch =
        (unsigned long long)tv.tv_sec * 1000ULL +
        (unsigned long long)tv.tv_usec / 1000ULL;

    return millisecondsSinceEpoch;
}

// UUID 문자열은 36자 + NULL = 37바이트 필요
char *OCPUtils_generateUUID(void)
{
    uint8_t uuid_bin[16]; // UUID v4는 128bit (16바이트)
    for (int i = 0; i < 16; i += 4)
    {
        uint32_t r = esp_random(); // 32bit 난수
        memcpy(&uuid_bin[i], &r, 4);
    }

    // UUID v4 규격 맞추기
    //  - 4비트 버전 필드(12번째 바이트 상위 4비트)를 0x4로 설정
    uuid_bin[6] = (uuid_bin[6] & 0x0F) | 0x40;
    //  - 2비트 variant 필드(14번째 바이트 상위 2비트)를 0x80~BF 범위로 설정
    uuid_bin[8] = (uuid_bin[8] & 0x3F) | 0x80;

    // 문자열 변환
    char *uuid_str = malloc(37);
    if (!uuid_str)
        return NULL;

    snprintf(uuid_str, 37,
             "%02x%02x%02x%02x-"
             "%02x%02x-"
             "%02x%02x-"
             "%02x%02x-"
             "%02x%02x%02x%02x%02x%02x",
             uuid_bin[0], uuid_bin[1], uuid_bin[2], uuid_bin[3],
             uuid_bin[4], uuid_bin[5],
             uuid_bin[6], uuid_bin[7],
             uuid_bin[8], uuid_bin[9],
             uuid_bin[10], uuid_bin[11], uuid_bin[12], uuid_bin[13], uuid_bin[14], uuid_bin[15]);

    return uuid_str; // 호출자가 free() 필요
}

static bool wait_conn_with_timeout_ms(uint32_t timeout_ms)
{
    BaseType_t ok = xSemaphoreTake(sem_forConn, pdMS_TO_TICKS(timeout_ms));
    return (ok == pdTRUE); // true면 신호 수신(=응답 옴), false면 타임아웃
}

void OCPManager_free_message(OCPMessage *message)
{
    if (message != NULL)
    {
        if (message->data != NULL)
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

    while (thread_all_flag)
    {
        // 여기에 주기적 ping/publish, reconnect 체크 등을 수행
        // 예: 연결 확인 → 미연결이면 재연결 트리거
        //     인가 승인 여부 체크 → 미인가면 authorize 요청 등

        // 예시 로그(지우셔도 됨)
        // ESP_LOGD(TAG, "tick.. conn_flag=%d", thread_conn_flag);
        wait_conn_with_timeout_ms(5000);
        vTaskDelay(pdMS_TO_TICKS(5000)); // 5초 주기
    }

    ESP_LOGI(TAG, "connection keep-alive task exiting");
    vTaskDelete(NULL); // 자기 자신 삭제 (detach에 해당)
}

void thread_sender(void *arg)
{
    while (thread_all_flag)
    {
        if (send_queue->size > 0)
        {
            char *payload = (char *)(send_queue->first->message);
            int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TEST_TOPIC,
                                                 payload, 0 /*len=0면 strlen*/,
                                                 1 /*QoS1*/, 0 /*retain*/);
            if (msg_id >= 0)
            {
                ESP_LOGI(TAG, "Published dummy JSON to %s", MQTT_TEST_TOPIC);
                ESP_LOGI(TAG, "%s", payload);
                free(payload);
                delete_queue(send_queue);
            }
            else
            {
                ESP_LOGW(TAG, "Publish failed (ret=%d)", msg_id);
                free(payload);
                delete_queue(send_queue);
            }
        }
        else
        {
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void thread_receiver(void *arg)
{
    while (thread_all_flag)
    {
        if (receive_queue->size > 0)
        {
            char *message = (char *)(receive_queue->first->message);
            if (message)
            {
                /* 수신 이후 처리 */
            }
        }
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t *event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
    {
        ESP_LOGI(TAG, "MQTT connected");

        if (!auth_payload)
        {
            ESP_LOGE(TAG, "auth_payload is NULL (build it before connecting)");
            break;
        }

        // 길이 명시 권장
        size_t plen = strlen(auth_payload);

        // 1) 복사 전송: outbox에 payload를 복사 -> 즉시 free해도 안전
        //    (QoS1/retain은 필요에 맞춰 조정)
        int msg_id = esp_mqtt_client_enqueue(
            mqtt_client,
            MQTT_TEST_TOPIC,
            auth_payload,
            plen,
            1 /*QoS1*/,
            0 /*retain*/,
            true /*copy*/
        );
        ESP_LOGI(TAG, "enqueued auth msg, id=%d, len=%u", msg_id, (unsigned)plen);

        // 2) 여기서 바로 정리해도 안전 (copy=true 덕분)
        cJSON_free(auth_payload);
        auth_payload = NULL;
        cJSON_Delete(root);
        root = NULL;

        // ❌ vTaskDelay(pdMS_TO_TICKS(5000));  // 핸들러에서 금지
        break;
    }

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT disconnected");
        s_mqtt_connected = false;
        s_pub_task_handle = NULL;
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT data received");
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
    if (xTaskCreate(thread_keep_alive_task, "conn_task", 4096, NULL, 5, &s_pub_task_handle) == pdPASS)
    {
        printf("Task created successfully!\n");
    }

    if (xTaskCreate(thread_sender, "send_task", 4096, NULL, 5, &s_pub_task_handle) == pdPASS)
    {
        printf("Task created successfully!\n");
    }

    // if(xTaskCreate(thread_receiver, "recv_task", 4096, NULL, 5, &s_pub_task_handle)==pdPASS) {
    //     printf("Task created successfully!\n");
    //     }

    // *** 인증 메시지 생성 *** //
    char *auth_data = "{\"authcode\":\"63ed3bdd4c73f00f\"}";
    char *msgId = OCPUtils_generateUUID();
    unsigned long long msgDate = OCPUtils_getCurrentTime();

    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", "2.9.0");
    cJSON_AddStringToObject(root, "msgType", "Q");
    cJSON_AddStringToObject(root, "funcType", "002");
    cJSON_AddStringToObject(root, "siteId", "C000000006");
    cJSON_AddStringToObject(root, "tId", "9016");
    cJSON_AddStringToObject(root, "thingName", "9016");
    cJSON_AddStringToObject(root, "msgCode", "MSGAUTH00002");
    cJSON_AddStringToObject(root, "msgId", msgId);
    cJSON_AddNumberToObject(root, "msgDate", msgDate);
    cJSON_AddStringToObject(root, "resCode", "");
    cJSON_AddStringToObject(root, "resMsg", "");
    cJSON_AddStringToObject(root, "dataFormat", "application/json");
    cJSON_AddStringToObject(root, "severity", "0");
    cJSON_AddStringToObject(root, "encType", "0");
    cJSON_AddStringToObject(root, "authToken", "");
    // data가 JSON이면 파싱해서 붙이기, 아니면 문자열로
    cJSON *data_obj = cJSON_Parse(auth_data);
    if (data_obj)
    {
        cJSON_AddItemToObject(root, "data", data_obj); // 소유권 이동
    }
    else
    {
        cJSON_AddStringToObject(root, "data", auth_data);
    }
    cJSON_AddNumberToObject(root, "datlen", (double)strlen(auth_data));
    auth_payload = cJSON_PrintUnformatted(root); // 동적할당

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL)
    {
        ESP_LOGE("MQTT", "Failed to create MQTT client");
        return;
    }
    else
    {
        ESP_LOGI("MQTT", "MQTT client created successfully");
    }
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_err_t start_result = esp_mqtt_client_start(mqtt_client);
    if (start_result != ESP_OK)
    {
        ESP_LOGE("MQTT", "Failed to start MQTT client: 0x%x", start_result);
    }
    else
    {
        ESP_LOGI("MQTT", "MQTT client started successfully");
    }
}
