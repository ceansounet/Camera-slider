#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "config.h"
#include "stepper.h"
#include "motion_planner.h"
#include "wifi_ap.h"
#include "webserver.h"
#include "controller_link.h"

static const char *TAG = "MAIN";

// -------------------------------------------------------
//  Endstop interrupt handler
// -------------------------------------------------------
static void IRAM_ATTR endstop_isr(void *arg)
{
    if (gpio_get_level(ENDSTOP_PIN) == 0) {
        // Triggered: emergency stop
        stepper_stop_all();
    }
}

// -------------------------------------------------------
//  Main
// -------------------------------------------------------
void app_main(void)
{
    ESP_LOGI(TAG, "MotionCtrl starting…");

    // ── NVS ──────────────────────────────────────────────
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // ── Steppers ─────────────────────────────────────────
    stepper_init();

    // ── Endstop interrupt ────────────────────────────────
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ENDSTOP_PIN, endstop_isr, NULL);
    gpio_set_intr_type(ENDSTOP_PIN, GPIO_INTR_NEGEDGE);

    // ── Motion planner ───────────────────────────────────
    planner_init();

    // ── WiFi AP ──────────────────────────────────────────
    wifi_ap_init();

    // ── HTTP / WebSocket server ───────────────────────────
    webserver_init();

    // ── Controller link (UART to handheld) ───────────────
    controller_link_init();

    ESP_LOGI(TAG, "All systems up. Connect to WiFi '%s' → http://192.168.4.1",
             WIFI_AP_SSID);

    // ── Main loop (watchdog + status log) ────────────────
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "M1=%lld M2=%lld state=%d ES=%d",
                 stepper_get_position(MOTOR_1),
                 stepper_get_position(MOTOR_2),
                 (int)planner_get_state(),
                 gpio_get_level(ENDSTOP_PIN));
    }
}
