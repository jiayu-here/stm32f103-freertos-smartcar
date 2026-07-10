#ifndef PID_H
#define PID_H

#include "app_types.h"

void PID_Init(PIDController *pid, float kp, float ki, float kd,
              float output_limit, float integral_limit);
void PID_Reset(PIDController *pid);
float PID_Update(PIDController *pid, float setpoint, float measurement, float dt_s);

#endif
