#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MODE_MANUAL = 0,
    MODE_TRACK,
    MODE_AVOID,
    MODE_FUSION,
    MODE_CRUISE,
    MODE_MOTION
} CarMode;

typedef enum {
    FAULT_NONE = 0,
    FAULT_LOW_BATTERY = 1U << 0,
    FAULT_ESTOP = 1U << 1,
    FAULT_COMMAND_TIMEOUT = 1U << 2,
    FAULT_TASK_STALL = 1U << 3,
    FAULT_SENSOR = 1U << 4,
    FAULT_MOTOR_STALL = 1U << 5
} FaultFlags;

typedef struct {
    /* Requested wheel speed. ControlTask applies an acceleration ramp before PID. */
    CarMode mode;
    float target_left_rpm;
    float target_right_rpm;
    float actual_left_rpm;
    float actual_right_rpm;
    float distance_cm;
    float battery_v;
    float odom_x_cm;
    float odom_y_cm;
    float heading_deg;
    float travel_left_cm;
    float travel_right_cm;
    int8_t line_error;
    uint8_t line_bits;
    uint32_t faults;
    uint32_t last_command_ms;
    /* SafetyTask is the only task that authorizes motor output. */
    bool motors_enabled;
} CarState;

typedef struct {
    float kp;
    float ki;
    float kd;
    float output_limit;
    float integral_limit;
    float integral;
    float previous_error;
} PIDController;

#endif
