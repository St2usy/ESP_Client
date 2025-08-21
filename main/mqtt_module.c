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
#include <stdint.h>

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
static int s_msgid_sub_auth = -1;

uint64_t OCPUtils_getCurrentTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL); // NTP 동기화 후 Epoch 기반 시간 반환
    uint64_t millisecondsSinceEpoch =
        (uint64_t)tv.tv_sec * 1000ULL +
        (uint64_t)tv.tv_usec / 1000ULL;

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

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t *event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
    {
        ESP_LOGI(TAG, "MQTT connected");
        s_mqtt_connected = true;

        // 1) 인증 응답/결과를 받을 토픽을 먼저 구독
        s_msgid_sub_auth = esp_mqtt_client_subscribe(mqtt_client,
                                                     "ocp/C000000006/9016",
                                                     1);
        if (s_msgid_sub_auth < 0)
        {
            ESP_LOGE(TAG, "subscribe failed: %d", s_msgid_sub_auth);
        }
        else
        {
            ESP_LOGI(TAG, "subscribe sent, submsg_id=%d", s_msgid_sub_auth);
        }
        const char *hello = "{\"authcode\":\"63ed3bdd4c73f00f\"}";
        esp_mqtt_client_publish(mqtt_client, "ocp/C000000006/9016", hello, 0, 1, 0);
        break; // ★ 반드시 필요
    }

    case MQTT_EVENT_DISCONNECTED:
    {
        ESP_LOGI(TAG, "MQTT disconnected");
        s_mqtt_connected = false;
        s_pub_task_handle = NULL;
        break;
    }

    case MQTT_EVENT_SUBSCRIBED:
    {
        if (!auth_payload)
        {
            ESP_LOGE(TAG, "auth_payload is NULL (build it before connecting)");
            break;
        }
        size_t plen = strlen(auth_payload);
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
        cJSON_free(auth_payload);
        auth_payload = NULL;
        cJSON_Delete(root);
        root = NULL;
        break;
    }

    case MQTT_EVENT_ERROR:
    {
        ESP_LOGE(TAG, "MQTT error");
        break;
    }

    case MQTT_EVENT_DATA:
    {
        ESP_LOGI(TAG, "MQTT data received");
        add_queue(receive_queue, event);
        break;
    }

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
        .credentials.username = "C000000006+9016",

        .broker.verification.certificate = (const char *)insator_server_crt_pem_start,
        .session.keepalive = 60,
        .network.timeout_ms = 10000,
        .session = {
            .keepalive = 60,          // 초. 기본 120 근방. 브로커 정책에 맞추기
            .disable_clean_session = false, // true면 Persistent Session
            .last_will = {
                .topic = "ocp/C000000006/9016",
                .msg = "{\"authcode\":\"63ed3bdd4c73f00f\"}",
                .msg_len = 0,  // 0이면 strlen(msg) 사용
                .qos = 1,
                .retain = 1,
            },
        },
    };

    receive_queue = create_queue();
    send_queue = create_queue();

    sem_forConn = xSemaphoreCreateBinary();
    configASSERT(sem_forConn != NULL);
    xSemaphoreTake(sem_forConn, 0);

    // *** 인증 메시지 생성 *** //
    char *auth_data = "{\"authcode\":\"63ed3bdd4c73f00f\"}";
    char *msgId = OCPUtils_generateUUID();
    uint64_t msgDate = OCPUtils_getCurrentTime();

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
