// ============================================================
//  MotionCtrl – Handheld Controller Firmware
//  ESP32-S3 + 3.5" ILI9488 SPI display + joystick/buttons
//  Communicates with main unit via UART (framed JSON)
// ============================================================
//
//  Pin mapping (adjust to your PCB):
//    Display (SPI):
//      MOSI  = GPIO 11
//      SCLK  = GPIO 12
//      CS    = GPIO 10
//      DC    = GPIO 13
//      RST   = GPIO 14
//      BL    = GPIO 15 (backlight PWM)
//    Joystick 1 (Motor 1):
//      X     = GPIO 1  (ADC)
//      Y     = GPIO 2  (ADC, unused for now)
//      BTN   = GPIO 3
//    Joystick 2 (Motor 2):
//      X     = GPIO 4  (ADC)
//      Y     = GPIO 5  (ADC, unused)
//      BTN   = GPIO 6
//    Buttons:
//      STOP  = GPIO 7   (active LOW)
//      HOME  = GPIO 8
//      RUN   = GPIO 9
//      MENU  = GPIO 16
//    UART to main unit:
//      TX    = GPIO 17
//      RX    = GPIO 18
//
//  NOTE: Display driver uses esp_lcd (IDF v5+).
//        Replace ili9488_init() with your display driver if different.
// ============================================================

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

static const char *TAG = "CTRL";

// -------------------------------------------------------
//  Pin definitions
// -------------------------------------------------------
#define DISP_MOSI     GPIO_NUM_11
#define DISP_SCLK     GPIO_NUM_12
#define DISP_CS       GPIO_NUM_10
#define DISP_DC       GPIO_NUM_13
#define DISP_RST      GPIO_NUM_14
#define DISP_BL       GPIO_NUM_15

#define JOY1_X_CH     ADC_CHANNEL_0   // GPIO 1
#define JOY2_X_CH     ADC_CHANNEL_3   // GPIO 4
#define JOY1_BTN      GPIO_NUM_3
#define JOY2_BTN      GPIO_NUM_6

#define BTN_STOP      GPIO_NUM_7
#define BTN_HOME      GPIO_NUM_8
#define BTN_RUN       GPIO_NUM_9
#define BTN_MENU      GPIO_NUM_16

#define UART_TX       GPIO_NUM_17
#define UART_RX       GPIO_NUM_18
#define UART_PORT     UART_NUM_1
#define UART_BAUD     115200

// -------------------------------------------------------
//  Protocol constants
// -------------------------------------------------------
#define STX  0x02
#define ETX  0x03

// -------------------------------------------------------
//  State
// -------------------------------------------------------
typedef enum { SCREEN_JOG=0, SCREEN_STATUS, SCREEN_POINTS, SCREEN_SEQ, SCREEN_COUNT } screen_t;
static screen_t current_screen = SCREEN_JOG;

typedef struct {
    int64_t m1_pos;
    int64_t m2_pos;
    char    state[16];
    bool    endstop;
    bool    m1_run;
    bool    m2_run;
} remote_status_t;

static remote_status_t remote = {0};

// -------------------------------------------------------
//  UART send
// -------------------------------------------------------
static void uart_send_cmd(const char *json)
{
    int len = strlen(json);
    uint8_t hdr[3] = { STX, (len>>8)&0xFF, len&0xFF };
    uart_write_bytes(UART_PORT, (char*)hdr, 3);
    uart_write_bytes(UART_PORT, json, len);
    uint8_t etx = ETX;
    uart_write_bytes(UART_PORT, (char*)&etx, 1);
}

// -------------------------------------------------------
//  Parse incoming status JSON from main unit
// -------------------------------------------------------
static void parse_status(const char *json)
{
    const char *p;
    p = strstr(json, "\"m1_pos\":");
    if (p) sscanf(p+9, "%lld", &remote.m1_pos);
    p = strstr(json, "\"m2_pos\":");
    if (p) sscanf(p+9, "%lld", &remote.m2_pos);
    p = strstr(json, "\"state\":\"");
    if (p) sscanf(p+9, "%15[^\"]", remote.state);
    p = strstr(json, "\"endstop\":");
    if (p) remote.endstop = (p[10] == 't');
    p = strstr(json, "\"m1_run\":");
    if (p) remote.m1_run = (p[9] == 't');
    p = strstr(json, "\"m2_run\":");
    if (p) remote.m2_run = (p[9] == 't');
}

// -------------------------------------------------------
//  UART RX task
// -------------------------------------------------------
static void uart_rx_task(void *arg)
{
    uint8_t buf[512], msg[512];
    int msg_pos=0; bool in_frame=false; int expected=0; int len_bytes=0;

    while (1) {
        int n = uart_read_bytes(UART_PORT, buf, sizeof(buf)-1, pdMS_TO_TICKS(20));
        for (int i=0; i<n; i++) {
            uint8_t b = buf[i];
            if (!in_frame) {
                if (b==STX) { in_frame=true; msg_pos=0; expected=0; len_bytes=0; }
            } else {
                if (len_bytes < 2) {
                    expected = (expected<<8)|b; len_bytes++;
                } else if (b == ETX) {
                    msg[msg_pos]=0;
                    parse_status((char*)msg);
                    in_frame=false;
                } else {
                    if (msg_pos < 511) msg[msg_pos++]=b;
                }
            }
        }
    }
}

// -------------------------------------------------------
//  ADC – joystick read  returns -100..+100
// -------------------------------------------------------
static adc_oneshot_unit_handle_t adc_handle;

static int read_joystick(adc_channel_t ch)
{
    int raw=0;
    adc_oneshot_read(adc_handle, ch, &raw);
    // 12-bit 0..4095, centre ~2048, deadband 200
    int v = raw - 2048;
    if (abs(v) < 200) return 0;
    return (v * 100) / 2048;
}

// -------------------------------------------------------
//  Button debounce
// -------------------------------------------------------
static bool btn_pressed(gpio_num_t pin)
{
    if (gpio_get_level(pin) == 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
        return gpio_get_level(pin) == 0;
    }
    return false;
}

// -------------------------------------------------------
//  Simple display driver placeholder
//  Replace with your actual ILI9488 / ST7796 driver
// -------------------------------------------------------
// For production: use esp_lcd + lvgl or similar.
// This stub writes via bit-banging SPI and outputs to UART for debug.

static void display_init(void)
{
    // Configure backlight PWM
    ledc_timer_config_t lt = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&lt);
    ledc_channel_config_t lc = {
        .gpio_num   = DISP_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 200,
        .hpoint     = 0,
    };
    ledc_channel_config(&lc);
    ESP_LOGI(TAG, "Display backlight on (stub – add ILI9488 driver for real output)");
}

// Print status to UART for debug (replace with real LCD draw calls)
static void display_render(void)
{
    static uint32_t frame = 0;
    if (frame++ % 20 != 0) return;  // ~2Hz refresh log

    ESP_LOGI(TAG, "┌──────────────────────────────┐");
    ESP_LOGI(TAG, "│ MotionCtrl    %-14s│", remote.state);
    ESP_LOGI(TAG, "│ M1: %-10lld  M2: %-10lld│", remote.m1_pos, remote.m2_pos);
    ESP_LOGI(TAG, "│ M1:%s  M2:%s  ES:%s     │",
             remote.m1_run?"RUN":"---",
             remote.m2_run?"RUN":"---",
             remote.endstop?"HIT":"---");
    ESP_LOGI(TAG, "│ Screen: %-21d│", current_screen);
    ESP_LOGI(TAG, "└──────────────────────────────┘");
}

// -------------------------------------------------------
//  Joystick jog task
// -------------------------------------------------------
#define JOG_DEADBAND     10    // percent
#define JOG_BASE_SPEED   5.0f  // mm/s at 100%
#define JOG_TICK_MS      100   // ms between jog commands

static void jog_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(JOG_TICK_MS));

        int j1 = read_joystick(JOY1_X_CH);
        int j2 = read_joystick(JOY2_X_CH);

        if (abs(j1) > JOG_DEADBAND) {
            float speed = JOG_BASE_SPEED * (abs(j1) / 100.0f) * 4;
            int64_t steps = (j1 > 0 ? 1 : -1) * (int64_t)(abs(j1) / 10);
            char cmd[128];
            snprintf(cmd, sizeof(cmd),
                     "{\"cmd\":\"J\",\"motor\":0,\"steps\":%lld,\"speed\":%.1f}",
                     steps, speed);
            uart_send_cmd(cmd);
        }
        if (abs(j2) > JOG_DEADBAND) {
            float speed = JOG_BASE_SPEED * (abs(j2) / 100.0f) * 4;
            int64_t steps = (j2 > 0 ? 1 : -1) * (int64_t)(abs(j2) / 10);
            char cmd[128];
            snprintf(cmd, sizeof(cmd),
                     "{\"cmd\":\"J\",\"motor\":1,\"steps\":%lld,\"speed\":%.1f}",
                     steps, speed);
            uart_send_cmd(cmd);
        }
    }
}

// -------------------------------------------------------
//  Button scan task
// -------------------------------------------------------
static void btn_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50));

        if (btn_pressed(BTN_STOP)) {
            uart_send_cmd("{\"cmd\":\"S\"}");
            ESP_LOGW(TAG, "STOP");
            while (gpio_get_level(BTN_STOP)==0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (btn_pressed(BTN_HOME)) {
            uart_send_cmd("{\"cmd\":\"H\",\"motor\":0}");
            ESP_LOGI(TAG, "HOME");
            while (gpio_get_level(BTN_HOME)==0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (btn_pressed(BTN_RUN)) {
            uart_send_cmd("{\"cmd\":\"R\"}");
            ESP_LOGI(TAG, "RUN sequence");
            while (gpio_get_level(BTN_RUN)==0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (btn_pressed(BTN_MENU)) {
            current_screen = (current_screen + 1) % SCREEN_COUNT;
            ESP_LOGI(TAG, "Screen -> %d", current_screen);
            while (gpio_get_level(BTN_MENU)==0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        if (btn_pressed(JOY1_BTN)) {
            uart_send_cmd("{\"cmd\":\"P\"}");  // Pause/Resume toggle
            ESP_LOGI(TAG, "PAUSE");
            while (gpio_get_level(JOY1_BTN)==0) vTaskDelay(pdMS_TO_TICKS(10));
        }

        display_render();
    }
}

// -------------------------------------------------------
//  app_main
// -------------------------------------------------------
void app_main(void)
{
    ESP_LOGI(TAG, "Controller starting…");

    nvs_flash_init();

    // GPIO button pins
    gpio_num_t btns[] = {BTN_STOP, BTN_HOME, BTN_RUN, BTN_MENU, JOY1_BTN, JOY2_BTN};
    for (int i=0; i<6; i++) {
        gpio_config_t c = {
            .pin_bit_mask = 1ULL<<btns[i],
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
        };
        gpio_config(&c);
    }

    // ADC for joysticks
    adc_oneshot_unit_init_cfg_t adc_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&adc_cfg, &adc_handle);
    adc_oneshot_chan_cfg_t ch_cfg = { .atten=ADC_ATTEN_DB_12, .bitwidth=ADC_BITWIDTH_12 };
    adc_oneshot_config_channel(adc_handle, JOY1_X_CH, &ch_cfg);
    adc_oneshot_config_channel(adc_handle, JOY2_X_CH, &ch_cfg);

    // UART
    uart_config_t uc = {
        .baud_rate = UART_BAUD, .data_bits=UART_DATA_8_BITS,
        .parity=UART_PARITY_DISABLE, .stop_bits=UART_STOP_BITS_1,
        .flow_ctrl=UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_PORT, &uc);
    uart_set_pin(UART_PORT, UART_TX, UART_RX, -1, -1);
    uart_driver_install(UART_PORT, 512, 512, 0, NULL, 0);

    // Display
    display_init();

    // Tasks
    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(jog_task,     "jog",     4096, NULL, 4, NULL);
    xTaskCreate(btn_task,     "btn",     4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Controller ready");
}
