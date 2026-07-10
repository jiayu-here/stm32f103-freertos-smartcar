#ifndef APP_H
#define APP_H

#include "app_types.h"

void App_Init(void);
void App_StartScheduler(void);
void App_GetState(CarState *state);
void App_NotifyUartByteFromISR(uint8_t byte);
void App_NotifyUltrasonicEdgeFromISR(bool high_level, uint32_t timestamp_us);

#endif
