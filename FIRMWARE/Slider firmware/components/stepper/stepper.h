#pragma once
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================
//  Stepper Driver – A4988 interface (step/dir/enable)
//  Uses esp_timer high-resolution timer for step generation
// ============================================================

#define STEPPER_COUNT 2

typedef enum {
    MOTOR_1 = 0,
    MOTOR_2 = 1
} motor_id_t;

typedef enum {
    DIR_POSITIVE = 1,
    DIR_NEGATIVE = -1
} motor_dir_t;

typedef struct {
    gpio_num_t dir_pin;
    gpio_num_t step_pin;
    gpio_num_t enable_pin;

    // State
    volatile int64_t position;       // absolute steps from home
    volatile int64_t target_position;
    volatile float   current_speed;  // steps/sec
    volatile float   target_speed;
    volatile float   accel;          // steps/sec²
    volatile bool    running;
    volatile bool    enabled;
    volatile bool    homing;

    // Timer handle
    esp_timer_handle_t timer;
    // For acceleration ramp
    volatile int64_t   steps_to_decel;
    volatile float     step_interval_us; // current interval
    volatile int       dir;
} stepper_t;

// Init all steppers from config.h pins
void stepper_init(void);

// Enable / disable driver (active LOW on A4988)
void stepper_enable(motor_id_t id);
void stepper_disable(motor_id_t id);

// Move relative / absolute (non-blocking, timer-driven)
void stepper_move_steps(motor_id_t id, int64_t steps, float speed_steps_s, float accel_steps_s2);
void stepper_move_to(motor_id_t id, int64_t abs_pos, float speed_steps_s, float accel_steps_s2);

// Immediate stop
void stepper_stop(motor_id_t id);
void stepper_stop_all(void);

// Emergency stop (disable + stop)
void stepper_emergency_stop(void);

// Query
bool    stepper_is_running(motor_id_t id);
int64_t stepper_get_position(motor_id_t id);
void    stepper_set_position(motor_id_t id, int64_t pos);  // zero-set

// Home on endstop (motor 1 only, blocking with timeout)
bool stepper_home(motor_id_t id, float speed_steps_s);

// Raw step accessor (used by motion planner)
stepper_t *stepper_get(motor_id_t id);
