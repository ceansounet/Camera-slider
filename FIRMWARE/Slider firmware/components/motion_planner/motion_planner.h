#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// ============================================================
//  Motion Planner
//  Manages saved positions, sequences, acceleration curves
//  and effects like hyperlapse.
// ============================================================

// -------------------------------------------------------
//  Saved Position (keyframe)
// -------------------------------------------------------
typedef struct {
    char    name[32];
    int64_t pos_m1;     // steps motor 1
    int64_t pos_m2;     // steps motor 2
    bool    valid;
} saved_point_t;

// -------------------------------------------------------
//  Acceleration curve type between two keyframes
// -------------------------------------------------------
typedef enum {
    CURVE_LINEAR    = 0,
    CURVE_EASE_IN   = 1,
    CURVE_EASE_OUT  = 2,
    CURVE_EASE_BOTH = 3,   // smooth S-curve
    CURVE_BEZIER    = 4,
} curve_type_t;

// -------------------------------------------------------
//  Sequence step
// -------------------------------------------------------
typedef struct {
    uint8_t     point_idx;       // index in saved_points[]
    float       speed_mm_s;      // travel speed to this point
    float       hold_time_s;     // dwell at this position
    curve_type_t curve;          // accel curve arriving at this point
    float       curve_param;     // e.g. bezier tension
} seq_step_t;

// -------------------------------------------------------
//  Effect types
// -------------------------------------------------------
typedef enum {
    EFFECT_NONE       = 0,
    EFFECT_HYPERLAPSE = 1,   // move X steps, pause, move, pause...
    EFFECT_OSCILLATE  = 2,   // bounce between two points
    EFFECT_PENDULUM   = 3,   // sinusoidal back-and-forth
} effect_type_t;

typedef struct {
    effect_type_t type;
    uint8_t   point_a;       // start point index
    uint8_t   point_b;       // end point index
    float     speed_mm_s;
    float     interval_s;    // pause between moves (hyperlapse)
    uint32_t  repeat;        // 0 = infinite
    float     param1;
    float     param2;
} effect_config_t;

// -------------------------------------------------------
//  Planner state
// -------------------------------------------------------
typedef enum {
    PLANNER_IDLE    = 0,
    PLANNER_RUNNING = 1,
    PLANNER_PAUSED  = 2,
    PLANNER_HOMING  = 3,
} planner_state_t;

// -------------------------------------------------------
//  API
// -------------------------------------------------------
void planner_init(void);

// Saved points CRUD
int     planner_save_point(const char *name, int64_t m1, int64_t m2);  // returns index
bool    planner_delete_point(uint8_t idx);
bool    planner_get_point(uint8_t idx, saved_point_t *out);
int     planner_get_point_count(void);
bool    planner_save_points_nvs(void);
bool    planner_load_points_nvs(void);

// Sequence management
bool    planner_seq_clear(void);
bool    planner_seq_add_step(seq_step_t *step);
bool    planner_seq_remove_step(uint8_t idx);
bool    planner_seq_move_step(uint8_t from, uint8_t to);
int     planner_seq_get_count(void);
bool    planner_seq_get_step(uint8_t idx, seq_step_t *out);
bool    planner_save_sequence_nvs(void);
bool    planner_load_sequence_nvs(void);

// Run
bool    planner_run_sequence(void);
bool    planner_run_effect(effect_config_t *cfg);
void    planner_pause(void);
void    planner_resume(void);
void    planner_stop(void);
planner_state_t planner_get_state(void);

// Direct jog
void    planner_jog(int motor, int64_t steps, float speed_mm_s);
void    planner_goto_point(uint8_t idx, float speed_mm_s);

// Populate JSON status (for WebSocket broadcast)
int     planner_get_status_json(char *buf, int max_len);
int     planner_get_points_json(char *buf, int max_len);
int     planner_get_sequence_json(char *buf, int max_len);
