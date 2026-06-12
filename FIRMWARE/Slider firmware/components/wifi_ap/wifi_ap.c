#include "wifi_ap.h"
#include "config.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "WIFI_AP";

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *e = data;
        ESP_LOGI(TAG, "Station connected, AID=%d", e->aid);
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "Station disconnected");
    }
}

esp_err_t wifi_ap_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {
        .ap = {
            .ssid          = WIFI_AP_SSID,
            .ssid_len      = strlen(WIFI_AP_SSID),
            .channel       = WIFI_AP_CHANNEL,
            .password      = WIFI_AP_PASS,
            .max_connection = WIFI_AP_MAX_STA,
            .authmode      = WIFI_AUTH_WPA2_PSK,
        },
    };
    if (strlen(WIFI_AP_PASS) == 0) wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started: SSID=%s IP=192.168.4.1", WIFI_AP_SSID);
    return ESP_OK;
}

void wifi_ap_get_ip(char *buf, int len)
{
    snprintf(buf, len, "192.168.4.1");
}
