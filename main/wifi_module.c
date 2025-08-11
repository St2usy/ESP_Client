#include "wifi_module.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#define WIFI_SSID "SK_WiFiGIGA10F8_2.4G"
#define WIFI_PASS "1612000357"
#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;
static const char *TAG = "wifi_module";

static bool tcp_connect_check(const char *host, uint16_t port, int timeout_ms)
{
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,     // IPv4/IPv6 모두 시도
        .ai_socktype = SOCK_STREAM, // TCP
    };
    struct addrinfo *res = NULL;
    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%u", port);

    int err = getaddrinfo(host, portstr, &hints, &res);
    if (err != 0 || res == NULL)
    {
        return false;
    }

    bool ok = false;
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next)
    {
        int sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0)
            continue;

        struct timeval tv = {
            .tv_sec = timeout_ms / 1000,
            .tv_usec = (timeout_ms % 1000) * 1000};
        // 블로킹 connect의 타임아웃을 위해 송/수신 타임아웃 설정
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0)
        {
            ok = true;
        }
        close(sock);
        if (ok)
            break;
    }

    freeaddrinfo(res);
    return ok;
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WIFI Connected");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Disconnected, retrying...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    wifi_event_group = xEventGroupCreate(); // wifi 연결 상태를 스레드간 공유하기 위해, 이벤트 그룹 생성
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialization completed.");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    // === 연결 검증 추가 ===
    const char *probe_host = "aiot.rda.go.kr"; // 실제 점검할 도메인으로 교체
    uint16_t probe_port = 8001;               // 실제 포트로 교체 (예: MQTT TLS면 8001 등)
    int timeout_ms = 2000;

    if (tcp_connect_check(probe_host, probe_port, timeout_ms))
    {
        ESP_LOGI(TAG, "Connectivity check OK: %s:%u reachable", probe_host, probe_port);
    }
    else
    {
        ESP_LOGW(TAG, "Connectivity check FAILED: %s:%u unreachable", probe_host, probe_port);
    }
}
