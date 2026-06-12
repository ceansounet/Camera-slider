#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

// ============================================================
//  HTTP Server + WebSocket for web UI
// ============================================================
esp_err_t webserver_init(void);
void      webserver_broadcast_status(void);   // call periodically
