#include "motion_planner.h"
#include "stepper.h"
#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static const char *TAG = "PLANNER";

// -------------------------------------------------------
//  State
// -------------------------------------------------------
static saved_point_t saved_points[MAX_SAVED_POINTS];
static int           point_count = 0;

static seq_step_t    sequence[MAX_SEQUENCE_STEPS];
static int           seq_count = 0;

static planner_state_t state = PLANNER_IDLE;
static SemaphoreHandle_t planner_mutex;
static TaskHandle_t      planner_task_handle = NULL;
static effect_config_t   active_effect;

// -------------------------------------------------------
//  Curve easing function  t ∈ [0,1] → position factor
// -------------------------------------------------------
static float ease(float t, curve_type_t curve)
{
    switch (curve) {
        case CURVE_LINEAR:    return t;
        case CURVE_EASE_IN:   return t * t;
        case CURVE_EASE_OUT:  return 1.0f - (1.0f - t) * (1.0f - t);
        case CURVE_EASE_BOTH: return t < 0.5f ? 2*t*t : 1-powf(-2*t+2,2)/2;
        case CURVE_BEZIER:    return t * t * (3 - 2*t);  // smooth step
        default:              return t;
    }
}

// Speed profile: peak speed reached at middle of move, eased at ends
static float speed_at_t(float t, float v_max, curve_type_t curve)
{
    float ramp = (t < 0.5f) ? ease(2*t, curve) : ease(2*(1-t), curve);
    return fmaxf(v_max * ramp, 1.0f);
}

// -------------------------------------------------------
//  Init
// -------------------------------------------------------
void planner_init(void)
{
    memset(saved_points, 0, sizeof(saved_points));
    memset(sequence, 0, sizeof(sequence));
    planner_mutex = xSemaphoreCreateMutex();
    planner_load_points_nvs();
    planner_load_sequence_nvs();
    ESP_LOGI(TAG, "Planner init done, %d points loaded", point_count);
}

// -------------------------------------------------------
//  Saved points
// -------------------------------------------------------
int planner_save_point(const char *name, int64_t m1, int64_t m2)
{
    xSemaphoreTake(planner_mutex, portMAX_DELAY);
    // Find existing by name or free slot
    int slot = -1;
    for (int i = 0; i < MAX_SAVED_POINTS; i++) {
        if (!saved_points[i].valid && slot == -1) slot = i;
        if (saved_points[i].valid && strcmp(saved_points[i].name, name) == 0) {
            slot = i; break;
        }
    }
    if (slot < 0) { xSemaphoreGive(planner_mutex); return -1; }
    strncpy(saved_points[slot].name, name, 31);
    saved_points[slot].pos_m1  = m1;
    saved_points[slot].pos_m2  = m2;
    saved_points[slot].valid   = true;
    if (slot >= point_count) point_count = slot + 1;
    xSemaphoreGive(planner_mutex);
    planner_save_points_nvs();
    return slot;
}

bool planner_delete_point(uint8_t idx)
{
    if (idx >= MAX_SAVED_POINTS) return false;
    xSemaphoreTake(planner_mutex, portMAX_DELAY);
    saved_points[idx].valid = false;
    xSemaphoreGive(planner_mutex);
    planner_save_points_nvs();
    return true;
}

bool planner_get_point(uint8_t idx, saved_point_t *out)
{
    if (idx >= MAX_SAVED_POINTS || !saved_points[idx].valid) return false;
    *out = saved_points[idx];
    return true;
}

int planner_get_point_count(void) { return point_count; }

// -------------------------------------------------------
//  NVS persistence
// -------------------------------------------------------
bool planner_save_points_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_blob(h, "points", saved_points, sizeof(saved_points));
    nvs_set_i32(h, "point_cnt", point_count);
    nvs_commit(h);
    nvs_close(h);
    return true;
}

bool planner_load_points_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sz = sizeof(saved_points);
    nvs_get_blob(h, "points", saved_points, &sz);
    int32_t cnt = 0;
    nvs_get_i32(h, "point_cnt", &cnt);
    point_count = cnt;
    nvs_close(h);
    return true;
}

bool planner_save_sequence_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_blob(h, "sequence", sequence, sizeof(sequence));
    nvs_set_i32(h, "seq_cnt", seq_count);
    nvs_commit(h);
    nvs_close(h);
    return true;
}

bool planner_load_sequence_nvs(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    size_t sz = sizeof(sequence);
    nvs_get_blob(h, "sequence", sequence, &sz);
    int32_t cnt = 0;
    nvs_get_i32(h, "seq_cnt", &cnt);
    seq_count = cnt;
    nvs_close(h);
    return true;
}

// -------------------------------------------------------
//  Sequence management
// -------------------------------------------------------
bool planner_seq_clear(void)
{
    memset(sequence, 0, sizeof(sequence));
    seq_count = 0;
    return true;
}

bool planner_seq_add_step(seq_step_t *step)
{
    if (seq_count >= MAX_SEQUENCE_STEPS) return false;
    sequence[seq_count++] = *step;
    planner_save_sequence_nvs();
    return true;
}

bool planner_seq_remove_step(uint8_t idx)
{
    if (idx >= seq_count) return false;
    memmove(&sequence[idx], &sequence[idx+1], (seq_count-idx-1)*sizeof(seq_step_t));
    seq_count--;
    planner_save_sequence_nvs();
    return true;
}

bool planner_seq_move_step(uint8_t from, uint8_t to)
{
    if (from >= seq_count || to >= seq_count) return false;
    seq_step_t tmp = sequence[from];
    memmove(&sequence[from], &sequence[from+1], (seq_count-from-1)*sizeof(seq_step_t));
    seq_count--;
    memmove(&sequence[to+1], &sequence[to], (seq_count-to)*sizeof(seq_step_t));
    sequence[to] = tmp;
    seq_count++;
    planner_save_sequence_nvs();
    return true;
}

int  planner_seq_get_count(void) { return seq_count; }

bool planner_seq_get_step(uint8_t idx, seq_step_t *out)
{
    if (idx >= seq_count) return false;
    *out = sequence[idx];
    return true;
}

// -------------------------------------------------------
//  Wait helpers
// -------------------------------------------------------
static void wait_motors_done(void)
{
    while ((stepper_is_running(MOTOR_1) || stepper_is_running(MOTOR_2)) &&
           state == PLANNER_RUNNING) {
        // Check for pause
        while (state == PLANNER_PAUSED) vTaskDelay(pdMS_TO_TICKS(50));
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static void wait_s(float sec)
{
    int ms = (int)(sec * 1000);
    while (ms > 0 && state == PLANNER_RUNNING) {
        while (state == PLANNER_PAUSED) vTaskDelay(pdMS_TO_TICKS(50));
        vTaskDelay(pdMS_TO_TICKS(10));
        ms -= 10;
    }
}

// -------------------------------------------------------
//  Sequence runner task
// -------------------------------------------------------
static void sequence_task(void *arg)
{
    ESP_LOGI(TAG, "Sequence started (%d steps)", seq_count);
    for (int i = 0; i < seq_count && state == PLANNER_RUNNING; i++) {
        seq_step_t *s = &sequence[i];
        if (!saved_points[s->point_idx].valid) continue;
        saved_point_t *p = &saved_points[s->point_idx];

        float speed_steps = s->speed_mm_s * DEFAULT_STEPS_PER_MM;
        float accel_steps = DEFAULT_ACCEL_MM_S2 * DEFAULT_STEPS_PER_MM;

        // Ease-based accel: map curve to accel coefficient
        float accel_mult = 1.0f;
        switch (s->curve) {
            case CURVE_EASE_IN:   accel_mult = 0.5f;  break;
            case CURVE_EASE_OUT:  accel_mult = 2.0f;  break;
            case CURVE_EASE_BOTH: accel_mult = 1.0f;  break;
            case CURVE_BEZIER:    accel_mult = 1.5f;  break;
            default: break;
        }

        stepper_move_to(MOTOR_1, p->pos_m1, speed_steps, accel_steps * accel_mult);
        stepper_move_to(MOTOR_2, p->pos_m2, speed_steps, accel_steps * accel_mult);
        wait_motors_done();

        // Dwell
        if (s->hold_time_s > 0) wait_s(s->hold_time_s);
    }
    state = PLANNER_IDLE;
    ESP_LOGI(TAG, "Sequence done");
    vTaskDelete(NULL);
}

// -------------------------------------------------------
//  Effect runner task
// -------------------------------------------------------
static void effect_task(void *arg)
{
    effect_config_t *cfg = (effect_config_t *)arg;
    ESP_LOGI(TAG, "Effect %d started", cfg->type);

    uint32_t repeat = cfg->repeat == 0 ? 0xFFFFFFFF : cfg->repeat;
    float speed_steps = cfg->speed_mm_s * DEFAULT_STEPS_PER_MM;
    float accel_steps = DEFAULT_ACCEL_MM_S2 * DEFAULT_STEPS_PER_MM;

    saved_point_t pa, pb;
    planner_get_point(cfg->point_a, &pa);
    planner_get_point(cfg->point_b, &pb);

    switch (cfg->type) {

        case EFFECT_HYPERLAPSE:
            // Step-by-step between A and B with interval pauses
            for (uint32_t r = 0; r < repeat && state == PLANNER_RUNNING; r++) {
                // Interpolate in N steps
                int n_steps = (int)(cfg->param1 > 0 ? cfg->param1 : 10);
                for (int k = 0; k <= n_steps && state == PLANNER_RUNNING; k++) {
                    float t = (float)k / n_steps;
                    int64_t m1 = pa.pos_m1 + (int64_t)((pb.pos_m1 - pa.pos_m1) * t);
                    int64_t m2 = pa.pos_m2 + (int64_t)((pb.pos_m2 - pa.pos_m2) * t);
                    stepper_move_to(MOTOR_1, m1, speed_steps, accel_steps);
                    stepper_move_to(MOTOR_2, m2, speed_steps, accel_steps);
                    wait_motors_done();
                    wait_s(cfg->interval_s);
                }
            }
            break;

        case EFFECT_OSCILLATE:
            for (uint32_t r = 0; r < repeat && state == PLANNER_RUNNING; r++) {
                stepper_move_to(MOTOR_1, pb.pos_m1, speed_steps, accel_steps);
                stepper_move_to(MOTOR_2, pb.pos_m2, speed_steps, accel_steps);
                wait_motors_done();
                wait_s(cfg->interval_s);
                stepper_move_to(MOTOR_1, pa.pos_m1, speed_steps, accel_steps);
                stepper_move_to(MOTOR_2, pa.pos_m2, speed_steps, accel_steps);
                wait_motors_done();
                wait_s(cfg->interval_s);
            }
            break;

        case EFFECT_PENDULUM:
            // Sinusoidal interpolation
            for (uint32_t r = 0; r < repeat && state == PLANNER_RUNNING; r++) {
                int n = 60;
                for (int k = 0; k < n && state == PLANNER_RUNNING; k++) {
                    float t = (sinf((float)k / n * M_PI * 2) + 1.0f) / 2.0f;
                    int64_t m1 = pa.pos_m1 + (int64_t)((pb.pos_m1 - pa.pos_m1) * t);
                    int64_t m2 = pa.pos_m2 + (int64_t)((pb.pos_m2 - pa.pos_m2) * t);
                    stepper_move_to(MOTOR_1, m1, speed_steps, accel_steps);
                    stepper_move_to(MOTOR_2, m2, speed_steps, accel_steps);
                    wait_motors_done();
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
            break;

        default: break;
    }

    state = PLANNER_IDLE;
    ESP_LOGI(TAG, "Effect done");
    vTaskDelete(NULL);
}

// -------------------------------------------------------
//  Public control
// -------------------------------------------------------
bool planner_run_sequence(void)
{
    if (state != PLANNER_IDLE) return false;
    if (seq_count == 0) return false;
    stepper_enable(MOTOR_1);
    stepper_enable(MOTOR_2);
    state = PLANNER_RUNNING;
    xTaskCreate(sequence_task, "seq_task", 4096, NULL, 5, &planner_task_handle);
    return true;
}

bool planner_run_effect(effect_config_t *cfg)
{
    if (state != PLANNER_IDLE) return false;
    active_effect = *cfg;
    stepper_enable(MOTOR_1);
    stepper_enable(MOTOR_2);
    state = PLANNER_RUNNING;
    xTaskCreate(effect_task, "fx_task", 4096, &active_effect, 5, &planner_task_handle);
    return true;
}

void planner_pause(void)  { if (state == PLANNER_RUNNING) state = PLANNER_PAUSED; }
void planner_resume(void) { if (state == PLANNER_PAUSED)  state = PLANNER_RUNNING; }

void planner_stop(void)
{
    state = PLANNER_IDLE;
    stepper_stop_all();
}

planner_state_t planner_get_state(void) { return state; }

void planner_jog(int motor, int64_t steps, float speed_mm_s)
{
    motor_id_t id = (motor == 0) ? MOTOR_1 : MOTOR_2;
    stepper_enable(id);
    float sp = speed_mm_s * DEFAULT_STEPS_PER_MM;
    float ac = DEFAULT_ACCEL_MM_S2 * DEFAULT_STEPS_PER_MM;
    stepper_move_steps(id, steps, sp, ac);
}

void planner_goto_point(uint8_t idx, float speed_mm_s)
{
    if (!saved_points[idx].valid) return;
    stepper_enable(MOTOR_1);
    stepper_enable(MOTOR_2);
    float sp = speed_mm_s * DEFAULT_STEPS_PER_MM;
    float ac = DEFAULT_ACCEL_MM_S2 * DEFAULT_STEPS_PER_MM;
    stepper_move_to(MOTOR_1, saved_points[idx].pos_m1, sp, ac);
    stepper_move_to(MOTOR_2, saved_points[idx].pos_m2, sp, ac);
}

// -------------------------------------------------------
//  JSON helpers
// -------------------------------------------------------
static const char *state_str[] = {"idle", "running", "paused", "homing"};

int planner_get_status_json(char *buf, int max_len)
{
    bool es = (gpio_get_level(ENDSTOP_PIN) == 0);
    return snprintf(buf, max_len,
        "{\"state\":\"%s\","
        "\"m1_pos\":%lld,\"m1_run\":%s,"
        "\"m2_pos\":%lld,\"m2_run\":%s,"
        "\"endstop\":%s}",
        state_str[state],
        stepper_get_position(MOTOR_1), stepper_is_running(MOTOR_1) ? "true" : "false",
        stepper_get_position(MOTOR_2), stepper_is_running(MOTOR_2) ? "true" : "false",
        es ? "true" : "false"
    );
}

int planner_get_points_json(char *buf, int max_len)
{
    int n = 0;
    n += snprintf(buf+n, max_len-n, "[");
    bool first = true;
    for (int i = 0; i < MAX_SAVED_POINTS && n < max_len-64; i++) {
        if (!saved_points[i].valid) continue;
        if (!first) n += snprintf(buf+n, max_len-n, ",");
        n += snprintf(buf+n, max_len-n,
            "{\"idx\":%d,\"name\":\"%s\",\"m1\":%lld,\"m2\":%lld}",
            i, saved_points[i].name, saved_points[i].pos_m1, saved_points[i].pos_m2);
        first = false;
    }
    n += snprintf(buf+n, max_len-n, "]");
    return n;
}

int planner_get_sequence_json(char *buf, int max_len)
{
    int n = 0;
    n += snprintf(buf+n, max_len-n, "[");
    for (int i = 0; i < seq_count && n < max_len-128; i++) {
        if (i) n += snprintf(buf+n, max_len-n, ",");
        seq_step_t *s = &sequence[i];
        n += snprintf(buf+n, max_len-n,
            "{\"idx\":%d,\"point\":%d,\"speed\":%.1f,\"hold\":%.2f,\"curve\":%d}",
            i, s->point_idx, s->speed_mm_s, s->hold_time_s, (int)s->curve);
    }
    n += snprintf(buf+n, max_len-n, "]");
    return n;
}
