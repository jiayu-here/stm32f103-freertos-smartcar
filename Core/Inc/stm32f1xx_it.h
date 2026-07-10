#ifndef STM32F1XX_IT_H
#define STM32F1XX_IT_H

#include "main.h"

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);
void EXTI4_IRQHandler(void);
void USART1_IRQHandler(void);

#endif
