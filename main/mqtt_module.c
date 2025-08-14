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

extern const uint8_t insator_server_crt_pem_start[] asm("_binary_insator_server_crt_pem_start");
extern const uint8_t insator_server_crt_pem_end[]   asm("_binary_insator_server_crt_pem_end");
static const char *TAG = "mqtt_module";
static esp_mqtt_client_handle_t mqtt_client = NULL;  // 전역 클라이언트 핸들 저장
static TaskHandle_t s_pub_task_handle = NULL;
static volatile bool s_mqtt_connected = false;
#define MQTT_TEST_TOPIC "ocp/dataBus/"
QUEUE* send_queue;
QUEUE* receive_queue;
static SemaphoreHandle_t sem_forConn; // binary semaphore
int thread_all_flag = 1;
int thread_conn_flag = 0;

static bool wait_conn_with_timeout_ms(uint32_t timeout_ms)
{
    BaseType_t ok = xSemaphoreTake(sem_forConn, pdMS_TO_TICKS(timeout_ms));
    return (ok == pdTRUE);  // true면 신호 수신(=응답 옴), false면 타임아웃
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            // s_mqtt_connected = true;
            // if (s_pub_task_handle == NULL) {
            //     xTaskCreate(publisher_task, "pub_task", 4096, NULL, 5, &s_pub_task_handle);
            // }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            s_mqtt_connected = false;
            s_pub_task_handle = NULL;
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
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
