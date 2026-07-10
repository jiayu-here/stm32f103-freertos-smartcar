#include "stm32f1xx_it.h"
#include "bsp.h"

void NMI_Handler(void) { }
void HardFault_Handler(void) { while (1) { } }
void MemManage_Handler(void) { while (1) { } }
void BusFault_Handler(void) { while (1) { } }
void UsageFault_Handler(void) { while (1) { } }
void DebugMon_Handler(void) { }

void EXTI4_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(ULTRASONIC_ECHO_PIN);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
