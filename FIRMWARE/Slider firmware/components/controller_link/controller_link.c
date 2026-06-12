#include "controller_link.h"
#include "config.h"
#include "motion_planner.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "CTRL_LINK";

#define RX_BUF_SIZE  512
#define TX_BUF_SIZE  512

// -------------------------------------------------------
//  Send framed message
// -------------------------------------------------------
static void send_frame(const char *json)
{
    int len = strlen(json);
    uint8_t header[3] = {
        CTRL_PROTO_STX,
        (len >> 8) & 0xFF,
        len & 0xFF
    };
    uart_write_bytes(CTRL_UART_PORT, (char*)header, 3);
    uart_write_bytes(CTRL_UART_PORT, json, len);
    uint8_t etx = CTRL_PROTO_ETX;
    uart_write_bytes(CTRL_UART_PORT, (char*)&etx, 1);
}

void controller_link_send_status(void)
{
    char buf[256];
    planner_get_status_json(buf, sizeof(buf));
    send_frame(buf);
}

// -------------------------------------------------------
//  Parse and dispatch incoming command
// -------------------------------------------------------
static void dispatch_command(const char *json, int len)
{
    // Simple keyword-based parser (avoids full JSON lib overhead)
    char cmd = 0;
    // {"cmd":"J","motor":0,"steps":100,"speed":10.0}
    const char *c = strstr(json, "\"cmd\":\"");
    if (c) cmd = c[7];

    switch (cmd) {
        case CMD_JOG: {
            int motor = 0; int64_t steps = 0; float speed = 10.0f;
            sscanf(strstr(json,"\"motor\":")+8, "%d", &motor);
            sscanf(strstr(json,"\"steps\":")+8, "%lld", &steps);
            sscanf(strstr(json,"\"speed\":")+8, "%f", &speed);
            planner_jog(motor, steps, speed);
            break;
        }
        case CMD_STOP:   planner_stop(); break;
        case CMD_HOME:   /* trigger homing via planner */ break;
        case CMD_GOTO: {
            int idx = 0; float sp = 10.0f;
            sscanf(strstr(json,"\"idx\":")+6, "%d", &idx);
            sscanf(strstr(json,"\"speed\":")+8, "%f", &sp);
            planner_goto_point(idx, sp);
            break;
        }
        case CMD_RUN_SEQ: planner_run_sequence(); break;
        case CMD_PAUSE:   planner_pause();  break;
        case CMD_RESUME:  planner_resume(); break;
        default:
            ESP_LOGW(TAG, "Unknown cmd: %c", cmd);
    }

    // Always reply with status
    controller_link_send_status();
}

// -------------------------------------------------------
//  RX task
// -------------------------------------------------------
static void ctrl_rx_task(void *arg)
{
    uint8_t  buf[RX_BUF_SIZE];
    uint8_t  msg[RX_BUF_SIZE];
    int      msg_pos   = 0;
    bool     in_frame  = false;
    int      expected  = 0;

    while (1) {
        int bytes = uart_read_bytes(CTRL_UART_PORT, buf, sizeof(buf)-1,
                                    pdMS_TO_TICKS(20));
        for (int i = 0; i < bytes; i++) {
            uint8_t b = buf[i];
            if (!in_frame) {
                if (b == CTRL_PROTO_STX) { in_frame = true; msg_pos = 0; expected = 0; }
            } else {
                if (expected == 0 && msg_pos < 2) {
                    // Collect 2 length bytes
                    expected = (expected << 8) | b;
                    if (msg_pos == 1) {
                        // expected now has full 16-bit length
                    }
                    msg_pos++;
                } else if (b == CTRL_PROTO_ETX) {
                    msg[msg_pos] = 0;
                    dispatch_command((char*)msg, msg_pos);
                    in_frame = false;
                } else {
                    if (msg_pos < RX_BUF_SIZE-1) msg[msg_pos++] = b;
                }
            }
        }
        // Periodic status push every 200ms
        static TickType_t last_push = 0;
        if (xTaskGetTickCount() - last_push > pdMS_TO_TICKS(200)) {
            controller_link_send_status();
            last_push = xTaskGetTickCount();
        }
    }
}

// -------------------------------------------------------
//  Init
// -------------------------------------------------------
void controller_link_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = CTRL_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(CTRL_UART_PORT, &cfg);
    uart_set_pin(CTRL_UART_PORT, CTRL_UART_TX, CTRL_UART_RX,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(CTRL_UART_PORT, RX_BUF_SIZE*2, TX_BUF_SIZE*2, 0, NULL, 0);

    xTaskCreate(ctrl_rx_task, "ctrl_rx", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Controller link init TX=%d RX=%d @%d",
             CTRL_UART_TX, CTRL_UART_RX, CTRL_UART_BAUD);
}
