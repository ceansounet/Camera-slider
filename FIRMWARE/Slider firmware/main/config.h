#pragma once

// ============================================================
//  MotionCtrl – Global Pin & Config Definitions
//  Target: ESP32-S3, ESP-IDF v5.x
// ============================================================

// --- Motor 1 (A4988) ---
#define MOTOR1_DIR_PIN      GPIO_NUM_1
#define MOTOR1_STEP_PIN     GPIO_NUM_2
#define MOTOR1_ENABLE_PIN   GPIO_NUM_3

// --- Motor 2 (A4988) ---
#define MOTOR2_DIR_PIN      GPIO_NUM_4
#define MOTOR2_STEP_PIN     GPIO_NUM_5
#define MOTOR2_ENABLE_PIN   GPIO_NUM_6

// --- End Stop ---
#define ENDSTOP_PIN         GPIO_NUM_13   // LOW = pressed (connects to GND)

// --- UART for Controller Link (ESP32-S3 manette) ---
#define CTRL_UART_PORT      UART_NUM_1
#define CTRL_UART_TX        GPIO_NUM_17
#define CTRL_UART_RX        GPIO_NUM_18
#define CTRL_UART_BAUD      115200

// --- WiFi AP ---
#define WIFI_AP_SSID        "MotionCtrl"
#define WIFI_AP_PASS        "motionctrl123"
#define WIFI_AP_CHANNEL     6
#define WIFI_AP_MAX_STA     4

// --- Motion defaults ---
#define DEFAULT_STEPS_PER_MM    80        // adjust to your mechanics
#define DEFAULT_MAX_SPEED_MM_S  100.0f
#define DEFAULT_ACCEL_MM_S2     200.0f
#define MAX_SAVED_POINTS        32
#define MAX_SEQUENCE_STEPS      64

// --- NVS namespace ---
#define NVS_NAMESPACE           "motionctrl"

// --- WebSocket task ---
#define WS_BROADCAST_PERIOD_MS  100
