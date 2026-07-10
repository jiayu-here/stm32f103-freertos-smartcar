#include "pid.h"

static float clampf(float value, float limit)
{
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

void PID_Init(PIDController *pid, float kp, float ki, float kd,
              float output_limit, float integral_limit)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->output_limit = output_limit;
    pid->integral_limit = integral_limit;
    PID_Reset(pid);
}

void PID_Reset(PIDController *pid)
{
    pid->integral = 0.0f;
    pid->previous_error = 0.0f;
}

float PID_Update(PIDController *pid, float setpoint, float measurement, float dt_s)
{
    const float error = setpoint - measurement;
    const float derivative = (error - pid->previous_error) / dt_s;
    float output;

    pid->integral = clampf(pid->integral + error * dt_s, pid->integral_limit);
    output = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
    pid->previous_error = error;

    return clampf(output, pid->output_limit);
}
