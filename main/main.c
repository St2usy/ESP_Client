#include "wifi_module.h"
#include "mqtt_module.h"
#include "esp_log.h"

void app_main(void) {
    ESP_LOGI("MAIN", "Starting Wi-Fi");
    wifi_init_sta();

    ESP_LOGI("MAIN", "Connecting to MQTT Broker");
    mqtt_app_start();
}
