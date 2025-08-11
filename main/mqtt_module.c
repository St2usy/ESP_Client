#include "mqtt_module.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>

extern const uint8_t insator_server_crt_pem_start[] asm("_binary_insator_server_crt_pem_start");
extern const uint8_t insator_server_crt_pem_end[]   asm("_binary_insator_server_crt_pem_end");
static const char *TAG = "mqtt_module";
static esp_mqtt_client_handle_t mqtt_client = NULL;  // 전역 클라이언트 핸들 저장

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;

        default:
            break;
    }
}

static void


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
