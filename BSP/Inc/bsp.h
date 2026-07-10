#ifndef BSP_H
#define BSP_H

#include "board.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern ADC_HandleTypeDef hadc1;
extern IWDG_HandleTypeDef hiwdg;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;
extern UART_HandleTypeDef huart1;

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timer);

void BSP_Init(void);
void BSP_StartPeripherals(void);
/* DWT-based free-running microsecond timestamp; wraps naturally. */
uint32_t BSP_Micros(void);
void BSP_MotorSet(float left_percent, float right_percent);
void BSP_MotorStop(void);
void BSP_EncoderReadDelta(int16_t *left, int16_t *right);
uint8_t BSP_IRRead(void);
/* Return battery voltage after applying the configured resistor-divider ratio. */
float BSP_BatteryRead(float divider_ratio);
/* Bit0 pin reset, bit1 power reset, bit2 software, bit3 IWDG, bit4 WWDG, bit5 low power. */
uint32_t BSP_ResetCause(void);
bool BSP_EStopActive(void);
void BSP_ServoSetAngle(int8_t angle_deg);
void BSP_UltrasonicTrigger(void);
void BSP_UartWrite(const char *text);
void BSP_UartWriteN(const char *data, size_t length);
void BSP_BuzzerSet(bool on);
void BSP_LedSet(bool on);
void BSP_WatchdogRefresh(void);
void BSP_OledInit(void);
void BSP_OledShowLines(const char *line0, const char *line1,
                       const char *line2, const char *line3);

#endif
