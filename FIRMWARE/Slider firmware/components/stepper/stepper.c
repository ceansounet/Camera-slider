#include "stepper.h"
#include "config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "STEPPER";

// -------------------------------------------------------
//  Internal state array
// -------------------------------------------------------
static stepper_t motors[STEPPER_COUNT];

// -------------------------------------------------------
//  Timer ISR – called every step_interval_us
// -------------------------------------------------------
static void IRAM_ATTR stepper_timer_cb(void *arg)
{
    stepper_t *m = (stepper_t *)arg;

    if (!m->running) return;

    // Toggle STEP pin (A4988 triggers on rising edge)
    gpio_set_level(m->step_pin, 1);
    // Short high pulse (~2 µs min for A4988 – timer resolution handles this)
    esp_rom_delay_us(2);
    gpio_set_level(m->step_pin, 0);

    // Update position
    m->position += m->dir;

    int64_t remaining = llabs(m->target_position - m->position);

    if (remaining == 0) {
        m->running = false;
        esp_timer_stop(m->timer);
        return;
    }

    // Acceleration / deceleration ramp (linear ramp in step interval)
    // Acceleration phase: shorten interval
    // Deceleration phase: lengthen interval
    float min_interval = 1000000.0f / m->target_speed;  // µs at full speed
    float max_interval = 1000000.0f / 1.0f;             // 1 step/sec minimum

    // Steps remaining vs steps needed to decel to stop
    // steps_to_decel = v² / (2*a)  in step units
    float v = 1000000.0f / m->step_interval_us;  // current speed in steps/s
    float decel_steps = (v * v) / (2.0f * m->accel);

    if ((float)remaining <= decel_steps + 1.0f) {
        // Decelerate
        m->step_interval_us += (m->step_interval_us * m->step_interval_us * m->accel) / 1000000.0f;
        if (m->step_interval_us > max_interval) m->step_interval_us = max_interval;
    } else if (m->step_interval_us > min_interval) {
        // Accelerate
        m->step_interval_us -= (m->step_interval_us * m->step_interval_us * m->accel) / 1000000.0f;
        if (m->step_interval_us < min_interval) m->step_interval_us = min_interval;
    }

    // Re-arm timer
    esp_timer_stop(m->timer);
    esp_timer_start_once(m->timer, (uint64_t)m->step_interval_us);
}

// -------------------------------------------------------
//  Init
// -------------------------------------------------------
void stepper_init(void)
{
    memset(motors, 0, sizeof(motors));

    // Pin assignments
    motors[MOTOR_1].dir_pin    = MOTOR1_DIR_PIN;
    motors[MOTOR_1].step_pin   = MOTOR1_STEP_PIN;
    motors[MOTOR_1].enable_pin = MOTOR1_ENABLE_PIN;

    motors[MOTOR_2].dir_pin    = MOTOR2_DIR_PIN;
    motors[MOTOR_2].step_pin   = MOTOR2_STEP_PIN;
    motors[MOTOR_2].enable_pin = MOTOR2_ENABLE_PIN;

    for (int i = 0; i < STEPPER_COUNT; i++) {
        stepper_t *m = &motors[i];

        gpio_config_t io = {
            .pin_bit_mask = (1ULL << m->dir_pin) |
                            (1ULL << m->step_pin) |
                            (1ULL << m->enable_pin),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&io);

        // Disable driver (A4988: ENABLE active LOW → HIGH = disabled)
        gpio_set_level(m->enable_pin, 1);
        gpio_set_level(m->step_pin, 0);
        gpio_set_level(m->dir_pin, 0);
        m->enabled = false;

        // Create esp_timer
        esp_timer_create_args_t timer_args = {
            .callback        = stepper_timer_cb,
            .arg             = m,
            .dispatch_method = ESP_TIMER_ISR,
            .name            = (i == 0) ? "stepper0" : "stepper1",
        };
        esp_timer_create(&timer_args, &m->timer);
    }

    // Endstop pin
    gpio_config_t es = {
        .pin_bit_mask = (1ULL << ENDSTOP_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,  // internal pull-up; GND = pressed
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&es);

    ESP_LOGI(TAG, "Stepper init done");
}

// -------------------------------------------------------
//  Enable / Disable
// -------------------------------------------------------
void stepper_enable(motor_id_t id)
{
    motors[id].enabled = true;
    gpio_set_level(motors[id].enable_pin, 0);  // A4988 ENABLE active LOW
}

void stepper_disable(motor_id_t id)
{
    motors[id].enabled = false;
    gpio_set_level(motors[id].enable_pin, 1);
}

// -------------------------------------------------------
//  Internal: start move (common)
// -------------------------------------------------------
static void _start_move(stepper_t *m, int64_t abs_target, float speed, float accel)
{
    if (m->running) {
        esp_timer_stop(m->timer);
        m->running = false;
    }

    int64_t diff = abs_target - m->position;
    if (diff == 0) return;

    m->dir            = (diff > 0) ? 1 : -1;
    m->target_position = abs_target;
    m->target_speed   = speed;
    m->accel          = accel;
    m->running        = true;

    // Start interval = 1 step/sec (very slow, then ramp up)
    float initial_speed = sqrtf(2.0f * accel);  // v after 1st step
    if (initial_speed > speed) initial_speed = speed;
    m->step_interval_us = 1000000.0f / initial_speed;

    // Set direction pin
    gpio_set_level(m->dir_pin, (m->dir > 0) ? 1 : 0);

    esp_timer_start_once(m->timer, (uint64_t)m->step_interval_us);
}

void stepper_move_steps(motor_id_t id, int64_t steps, float speed, float accel)
{
    stepper_t *m = &motors[id];
    _start_move(m, m->position + steps, speed, accel);
}

void stepper_move_to(motor_id_t id, int64_t abs_pos, float speed, float accel)
{
    _start_move(&motors[id], abs_pos, speed, accel);
}

// -------------------------------------------------------
//  Stop
// -------------------------------------------------------
void stepper_stop(motor_id_t id)
{
    stepper_t *m = &motors[id];
    esp_timer_stop(m->timer);
    m->running = false;
    m->target_position = m->position;
}

void stepper_stop_all(void)
{
    for (int i = 0; i < STEPPER_COUNT; i++) stepper_stop(i);
}

void stepper_emergency_stop(void)
{
    for (int i = 0; i < STEPPER_COUNT; i++) {
        esp_timer_stop(motors[i].timer);
        motors[i].running = false;
        gpio_set_level(motors[i].enable_pin, 1);  // Disable
        motors[i].enabled = false;
    }
    ESP_LOGW(TAG, "EMERGENCY STOP");
}

// -------------------------------------------------------
//  Query
// -------------------------------------------------------
bool stepper_is_running(motor_id_t id)        { return motors[id].running; }
int64_t stepper_get_position(motor_id_t id)   { return motors[id].position; }
void stepper_set_position(motor_id_t id, int64_t pos) { motors[id].position = pos; }
stepper_t *stepper_get(motor_id_t id)         { return &motors[id]; }

// -------------------------------------------------------
//  Homing (motor moves toward endstop until triggered)
// -------------------------------------------------------
bool stepper_home(motor_id_t id, float speed)
{
    stepper_t *m = &motors[id];
    ESP_LOGI(TAG, "Homing motor %d", id);

    stepper_enable(id);
    // Move toward endstop (negative direction, adjust if needed)
    m->dir = -1;
    m->target_position = m->position - 1000000LL;  // large move
    m->target_speed    = speed;
    m->accel           = speed * 2;
    m->running         = true;
    m->step_interval_us = 1000000.0f / speed;
    gpio_set_level(m->dir_pin, 0);
    esp_timer_start_once(m->timer, (uint64_t)m->step_interval_us);

    // Wait for endstop (LOW = triggered)
    int64_t timeout = esp_timer_get_time() + 30000000LL;  // 30s timeout
    while (gpio_get_level(ENDSTOP_PIN) != 0) {
        if (esp_timer_get_time() > timeout) {
            stepper_stop(id);
            ESP_LOGE(TAG, "Homing timeout");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    stepper_stop(id);
    m->position = 0;
    ESP_LOGI(TAG, "Motor %d homed", id);
    return true;
}
