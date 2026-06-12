#pragma once
#include <stdint.h>
#include <stdbool.h>

// ============================================================
//  Controller Link – UART protocol to handheld ESP32-S3
//  Uses simple framed JSON messages:
//  [STX][LEN_HI][LEN_LO][JSON...][ETX]
//  STX=0x02  ETX=0x03
// ============================================================

#define CTRL_PROTO_STX  0x02
#define CTRL_PROTO_ETX  0x03

typedef enum {
    CMD_JOG       = 'J',
    CMD_STOP      = 'S',
    CMD_HOME      = 'H',
    CMD_GOTO      = 'G',
    CMD_STATUS    = '?',
    CMD_RUN_SEQ   = 'R',
    CMD_PAUSE     = 'P',
    CMD_RESUME    = 'r',
    CMD_EFFECT    = 'E',
} ctrl_cmd_t;

void controller_link_init(void);
void controller_link_send_status(void);   // push status to controller
