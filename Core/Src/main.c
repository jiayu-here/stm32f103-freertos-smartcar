#include "main.h"
#include "app.h"
#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"

int main(void)
{
    BSP_Init();
    App_Init();
    BSP_StartPeripherals();
    App_StartScheduler();
    return 0;
}

void Error_Handler(void)
{
    __disable_irq();
    BSP_MotorStop();
    while (1) {
        HAL_GPIO_TogglePin(STATUS_LED_PORT, STATUS_LED_PIN);
        for (volatile uint32_t i = 0U; i < 500000U; ++i) { }
    }
}

void vApplicationMallocFailedHook(void) { Error_Handler(); }

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    Error_Handler();
}
