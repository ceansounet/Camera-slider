#include "webserver.h"
#include "web_index.h"
#include "motion_planner.h"
#include "stepper.h"
#include "config.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "WEBSERVER";
static httpd_handle_t server = NULL;

// -------------------------------------------------------
//  WebSocket client list
// -------------------------------------------------------
#define MAX_WS_CLIENTS 4
static int ws_fds[MAX_WS_CLIENTS];
static int ws_fd_count = 0;

static void ws_add_fd(int fd)
{
    for (int i = 0; i < ws_fd_count; i++) if (ws_fds[i] == fd) return;
    if (ws_fd_count < MAX_WS_CLIENTS) ws_fds[ws_fd_count++] = fd;
}

static void ws_remove_fd(int fd)
{
    for (int i = 0; i < ws_fd_count; i++) {
        if (ws_fds[i] == fd) {
            ws_fds[i] = ws_fds[--ws_fd_count];
            return;
        }
    }
}

// -------------------------------------------------------
//  Broadcast to all WS clients
// -------------------------------------------------------
static void ws_broadcast(const char *json)
{
    httpd_ws_frame_t pkt = {
        .type    = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json,
        .len     = strlen(json),
    };
    for (int i = ws_fd_count - 1; i >= 0; i--) {
        esp_err_t r = httpd_ws_send_frame_async(server, ws_fds[i], &pkt);
        if (r != ESP_OK) ws_remove_fd(ws_fds[i]);
    }
}

// Wrap payload in typed envelope
static void ws_broadcast_type(const char *type, const char *data)
{
    char buf[2048];
    snprintf(buf, sizeof(buf), "{\"type\":\"%s\",\"data\":%s}", type, data);
    ws_broadcast(buf);
}

// -------------------------------------------------------
//  Command dispatcher (from WS message)
// -------------------------------------------------------
static void handle_ws_command(const char *json)
{
    // Extract cmd field
    char cmd[32] = {0};
    const char *p = strstr(json, "\"cmd\":\"");
    if (!p) return;
    sscanf(p + 7, "%31[^\"]", cmd);

    if (strcmp(cmd, "JOG") == 0) {
        int motor = 0; int64_t steps = 0; float speed = 10;
        const char *pm = strstr(json, "\"motor\":");
        const char *ps = strstr(json, "\"steps\":");
        const char *pv = strstr(json, "\"speed\":");
        if (pm) sscanf(pm+8, "%d", &motor);
        if (ps) sscanf(ps+8, "%lld", &steps);
        if (pv) sscanf(pv+8, "%f", &speed);
        planner_jog(motor, steps, speed);

    } else if (strcmp(cmd, "STOP") == 0) {
        planner_stop();

    } else if (strcmp(cmd, "HOME") == 0) {
        int motor = 0;
        const char *pm = strstr(json, "\"motor\":");
        if (pm) sscanf(pm+8, "%d", &motor);
        stepper_home(motor == 0 ? MOTOR_1 : MOTOR_2,
                     DEFAULT_STEPS_PER_MM * 5.0f);

    } else if (strcmp(cmd, "ZERO") == 0) {
        stepper_set_position(MOTOR_1, 0);
        stepper_set_position(MOTOR_2, 0);

    } else if (strcmp(cmd, "ENABLE") == 0) {
        stepper_enable(MOTOR_1); stepper_enable(MOTOR_2);

    } else if (strcmp(cmd, "DISABLE") == 0) {
        stepper_disable(MOTOR_1); stepper_disable(MOTOR_2);

    } else if (strcmp(cmd, "SAVE_POINT") == 0) {
        char name[32] = "Point";
        const char *pn = strstr(json, "\"name\":\"");
        if (pn) sscanf(pn+8, "%31[^\"]", name);
        int idx = planner_save_point(name,
                    stepper_get_position(MOTOR_1),
                    stepper_get_position(MOTOR_2));
        ESP_LOGI(TAG, "Saved point '%s' at slot %d", name, idx);

    } else if (strcmp(cmd, "GET_POINTS") == 0) {
        char buf[1024];
        planner_get_points_json(buf, sizeof(buf));
        ws_broadcast_type("points", buf);
        return;

    } else if (strcmp(cmd, "DEL_POINT") == 0) {
        int idx = 0;
        const char *pi = strstr(json, "\"idx\":");
        if (pi) sscanf(pi+6, "%d", &idx);
        planner_delete_point(idx);

    } else if (strcmp(cmd, "GOTO") == 0) {
        int idx = 0; float sp = 15;
        const char *pi = strstr(json, "\"idx\":");
        const char *pv = strstr(json, "\"speed\":");
        if (pi) sscanf(pi+6, "%d", &idx);
        if (pv) sscanf(pv+8, "%f", &sp);
        planner_goto_point(idx, sp);

    } else if (strcmp(cmd, "SEQ_ADD") == 0) {
        seq_step_t step = {0};
        const char *pp = strstr(json, "\"point\":");
        const char *pv = strstr(json, "\"speed\":");
        const char *ph = strstr(json, "\"hold\":");
        const char *pc = strstr(json, "\"curve\":");
        if (pp) sscanf(pp+8, "%hhu", &step.point_idx);
        if (pv) sscanf(pv+8, "%f", &step.speed_mm_s);
        if (ph) sscanf(ph+7, "%f", &step.hold_time_s);
        if (pc) { int c=0; sscanf(pc+8, "%d", &c); step.curve = c; }
        planner_seq_add_step(&step);

    } else if (strcmp(cmd, "SEQ_DEL") == 0) {
        int idx = 0;
        const char *pi = strstr(json, "\"idx\":");
        if (pi) sscanf(pi+6, "%d", &idx);
        planner_seq_remove_step(idx);

    } else if (strcmp(cmd, "SEQ_CLEAR") == 0) {
        planner_seq_clear();

    } else if (strcmp(cmd, "GET_SEQ") == 0) {
        char buf[1024];
        planner_get_sequence_json(buf, sizeof(buf));
        ws_broadcast_type("sequence", buf);
        return;

    } else if (strcmp(cmd, "RUN") == 0) {
        planner_run_sequence();

    } else if (strcmp(cmd, "PAUSE") == 0) {
        planner_pause();

    } else if (strcmp(cmd, "RESUME") == 0) {
        planner_resume();

    } else if (strcmp(cmd, "EFFECT") == 0) {
        effect_config_t cfg = {0};
        const char *pt = strstr(json, "\"effect_type\":");
        const char *pa = strstr(json, "\"point_a\":");
        const char *pb = strstr(json, "\"point_b\":");
        const char *pv = strstr(json, "\"speed\":");
        const char *pi = strstr(json, "\"interval\":");
        const char *pp = strstr(json, "\"param1\":");
        const char *pr = strstr(json, "\"repeat\":");
        if (pt) { int t=0; sscanf(pt+14, "%d", &t); cfg.type = t; }
        if (pa) sscanf(pa+10, "%hhu", &cfg.point_a);
        if (pb) sscanf(pb+10, "%hhu", &cfg.point_b);
        if (pv) sscanf(pv+8, "%f", &cfg.speed_mm_s);
        if (pi) sscanf(pi+11, "%f", &cfg.interval_s);
        if (pp) sscanf(pp+9, "%f", &cfg.param1);
        if (pr) { uint32_t r=0; sscanf(pr+9, "%lu", &r); cfg.repeat = r; }
        planner_run_effect(&cfg);
    }
}

// -------------------------------------------------------
//  WebSocket handler
// -------------------------------------------------------
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // New connection
        ws_add_fd(httpd_req_to_sockfd(req));
        ESP_LOGI(TAG, "WS client connected fd=%d", httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    httpd_ws_frame_t pkt;
    uint8_t buf[512] = {0};
    memset(&pkt, 0, sizeof(pkt));
    pkt.payload = buf;
    pkt.type    = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &pkt, sizeof(buf)-1);
    if (ret != ESP_OK) {
        ws_remove_fd(httpd_req_to_sockfd(req));
        return ret;
    }
    buf[pkt.len] = 0;

    if (pkt.type == HTTPD_WS_TYPE_TEXT) {
        handle_ws_command((char*)buf);
    } else if (pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ws_remove_fd(httpd_req_to_sockfd(req));
    }
    return ESP_OK;
}

// -------------------------------------------------------
//  HTTP GET / → serve HTML
// -------------------------------------------------------
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

// -------------------------------------------------------
//  Status broadcast task
// -------------------------------------------------------
static void broadcast_task(void *arg)
{
    char buf[256];
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(WS_BROADCAST_PERIOD_MS));
        if (ws_fd_count == 0) continue;
        planner_get_status_json(buf, sizeof(buf));
        // Wrap in {type:"status",...}
        char msg[320];
        // Build flat status object with type field
        snprintf(msg, sizeof(msg),
            "{\"type\":\"status\",\"state\":\"%s\","
            "\"m1_pos\":%lld,\"m1_run\":%s,"
            "\"m2_pos\":%lld,\"m2_run\":%s,"
            "\"endstop\":%s}",
            planner_get_state() == PLANNER_IDLE    ? "idle"    :
            planner_get_state() == PLANNER_RUNNING ? "running" :
            planner_get_state() == PLANNER_PAUSED  ? "paused"  : "homing",
            stepper_get_position(MOTOR_1),
            stepper_is_running(MOTOR_1) ? "true" : "false",
            stepper_get_position(MOTOR_2),
            stepper_is_running(MOTOR_2) ? "true" : "false",
            gpio_get_level(ENDSTOP_PIN) == 0 ? "true" : "false"
        );
        ws_broadcast(msg);
    }
}

void webserver_broadcast_status(void)
{
    // Can be called externally if needed
    char buf[256];
    planner_get_status_json(buf, sizeof(buf));
    ws_broadcast_type("status", buf);
}

// -------------------------------------------------------
//  Init
// -------------------------------------------------------
esp_err_t webserver_init(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 7;
    cfg.stack_size       = 8192;

    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    // Register routes
    httpd_uri_t uri_index = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = index_handler,
    };
    httpd_register_uri_handler(server, &uri_index);

    httpd_uri_t uri_ws = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(server, &uri_ws);

    // Start broadcast task
    xTaskCreate(broadcast_task, "ws_bcast", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Web server started → http://192.168.4.1/");
    return ESP_OK;
}
