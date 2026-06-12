#pragma once
#include "esp_err.h"

// ============================================================
//  WiFi Access Point
// ============================================================
esp_err_t wifi_ap_init(void);
void      wifi_ap_get_ip(char *buf, int len);
