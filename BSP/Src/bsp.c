#include "bsp.h"
#include "app.h"
#include "FreeRTOS.h"
#include "task.h"

#include <math.h>
#include <string.h>

ADC_HandleTypeDef hadc1;
IWDG_HandleTypeDef hiwdg;
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
UART_HandleTypeDef huart1;

static uint8_t uart_rx_byte;
static uint16_t encoder_left_previous;
static uint16_t encoder_right_previous;
static uint32_t reset_cause;

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void ADC1_Init(void);
static void TIM1_Init(void);
static void TIM2_Init(void);
static void TIM3_Init(void);
static void TIM4_Init(void);
static void USART1_Init(void);
static void IWDG_Init(void);

void BSP_Init(void)
{
    HAL_Init();
    reset_cause = (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) ? (1U << 0) : 0U) |
                  (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) ? (1U << 1) : 0U) |
                  (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) ? (1U << 2) : 0U) |
                  (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) ? (1U << 3) : 0U) |
                  (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) ? (1U << 4) : 0U) |
                  (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) ? (1U << 5) : 0U);
    __HAL_RCC_CLEAR_RESET_FLAGS();
    SystemClock_Config();
    GPIO_Init();
    ADC1_Init();
    TIM1_Init();
    TIM2_Init();
    TIM3_Init();
    TIM4_Init();
    USART1_Init();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void BSP_StartPeripherals(void)
{
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim2, MOTOR_PWM_LEFT_CH) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim2, MOTOR_PWM_RIGHT_CH) != HAL_OK ||
        HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL) != HAL_OK ||
        HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL) != HAL_OK) {
        Error_Handler();
    }

    encoder_left_previous = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
    encoder_right_previous = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
    BSP_MotorStop();
    BSP_ServoSetAngle(0);
    BSP_OledInit();
    if (HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U) != HAL_OK) {
        Error_Handler();
    }
    IWDG_Init();
}

uint32_t BSP_Micros(void)
{
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}

static void set_motor(GPIO_TypeDef *in1_port, uint16_t in1_pin,
                      GPIO_TypeDef *in2_port, uint16_t in2_pin,
                      TIM_HandleTypeDef *timer, uint32_t channel, float percent)
{
    float magnitude = fabsf(percent);
    if (magnitude > 100.0f) {
        magnitude = 100.0f;
    }

    if (percent > 0.05f) {
        HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_RESET);
    } else if (percent < -0.05f) {
        HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(in1_port, in1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(in2_port, in2_pin, GPIO_PIN_RESET);
        magnitude = 0.0f;
    }
    __HAL_TIM_SET_COMPARE(timer, channel,
                          (uint32_t)(magnitude * (float)MOTOR_PWM_PERIOD / 100.0f));
}

void BSP_MotorSet(float left_percent, float right_percent)
{
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_SET);
    set_motor(MOTOR_L_IN1_PORT, MOTOR_L_IN1_PIN, MOTOR_L_IN2_PORT, MOTOR_L_IN2_PIN,
              &htim2, MOTOR_PWM_LEFT_CH, left_percent);
    set_motor(MOTOR_R_IN1_PORT, MOTOR_R_IN1_PIN, MOTOR_R_IN2_PORT, MOTOR_R_IN2_PIN,
              &htim2, MOTOR_PWM_RIGHT_CH, right_percent);
}

void BSP_MotorStop(void)
{
    set_motor(MOTOR_L_IN1_PORT, MOTOR_L_IN1_PIN, MOTOR_L_IN2_PORT, MOTOR_L_IN2_PIN,
              &htim2, MOTOR_PWM_LEFT_CH, 0.0f);
    set_motor(MOTOR_R_IN1_PORT, MOTOR_R_IN1_PIN, MOTOR_R_IN2_PORT, MOTOR_R_IN2_PIN,
              &htim2, MOTOR_PWM_RIGHT_CH, 0.0f);
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_RESET);
}

void BSP_EncoderReadDelta(int16_t *left, int16_t *right)
{
    const uint16_t now_left = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
    const uint16_t now_right = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
    *left = (int16_t)(now_left - encoder_left_previous);
    *right = (int16_t)(now_right - encoder_right_previous);
    encoder_left_previous = now_left;
    encoder_right_previous = now_right;
}

uint8_t BSP_IRRead(void)
{
    uint8_t bits = 0U;
    bits |= HAL_GPIO_ReadPin(IR0_PORT, IR0_PIN) == GPIO_PIN_RESET ? 0x01U : 0U;
    bits |= HAL_GPIO_ReadPin(IR1_PORT, IR1_PIN) == GPIO_PIN_RESET ? 0x02U : 0U;
    bits |= HAL_GPIO_ReadPin(IR2_PORT, IR2_PIN) == GPIO_PIN_RESET ? 0x04U : 0U;
    bits |= HAL_GPIO_ReadPin(IR3_PORT, IR3_PIN) == GPIO_PIN_RESET ? 0x08U : 0U;
    bits |= HAL_GPIO_ReadPin(IR4_PORT, IR4_PIN) == GPIO_PIN_RESET ? 0x10U : 0U;
    return bits;
}

float BSP_BatteryRead(float divider_ratio)
{
    uint32_t total = 0U;
    for (uint32_t i = 0U; i < 8U; ++i) {
        if (HAL_ADC_Start(&hadc1) != HAL_OK ||
            HAL_ADC_PollForConversion(&hadc1, 2U) != HAL_OK) {
            return 0.0f;
        }
        total += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }
    return ((float)total / 8.0f) * (3.3f / 4095.0f) * divider_ratio;
}

uint32_t BSP_ResetCause(void)
{
    return reset_cause;
}

bool BSP_EStopActive(void)
{
    return HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_RESET;
}

void BSP_ServoSetAngle(int8_t angle_deg)
{
    int32_t pulse;
    if (angle_deg > 60) angle_deg = 60;
    if (angle_deg < -60) angle_deg = -60;
    pulse = 1500 + ((int32_t)angle_deg * 500 / 60);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)pulse);
}

void BSP_UltrasonicTrigger(void)
{
    uint32_t start;
    HAL_GPIO_WritePin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_RESET);
    start = BSP_Micros();
    while ((uint32_t)(BSP_Micros() - start) < 3U) { }
    HAL_GPIO_WritePin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_SET);
    start = BSP_Micros();
    while ((uint32_t)(BSP_Micros() - start) < 10U) { }
    HAL_GPIO_WritePin(ULTRASONIC_TRIG_PORT, ULTRASONIC_TRIG_PIN, GPIO_PIN_RESET);
}

void BSP_UartWrite(const char *text)
{
    BSP_UartWriteN(text, strlen(text));
}

void BSP_UartWriteN(const char *data, size_t length)
{
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, (uint16_t)length, 50U);
}

void BSP_BuzzerSet(bool on)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void BSP_LedSet(bool on)
{
    HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void BSP_WatchdogRefresh(void)
{
    (void)HAL_IWDG_Refresh(&hiwdg);
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    (void)higher_priority_task_woken;
    if (gpio_pin == ULTRASONIC_ECHO_PIN) {
        App_NotifyUltrasonicEdgeFromISR(
            HAL_GPIO_ReadPin(ULTRASONIC_ECHO_PORT, ULTRASONIC_ECHO_PIN) == GPIO_PIN_SET,
            BSP_Micros());
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart->Instance == USART1) {
        App_NotifyUartByteFromISR(uart_rx_byte);
        (void)HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U);
    }
}

uint32_t HAL_GetTick(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        return (uint32_t)xTaskGetTickCount();
    }
    return uwTick;
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clock = {0};

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscillator.HSEState = RCC_HSE_ON;
    oscillator.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    oscillator.HSIState = RCC_HSI_ON;
    oscillator.PLL.PLLState = RCC_PLL_ON;
    oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    oscillator.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) Error_Handler();

    clock.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clock.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clock.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clock.APB1CLKDivider = RCC_HCLK_DIV2;
    clock.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clock, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

static void GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    HAL_GPIO_WritePin(GPIOA, ULTRASONIC_TRIG_PIN | BUZZER_PIN | MOTOR_STBY_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, MOTOR_L_IN1_PIN | MOTOR_L_IN2_PIN | MOTOR_R_IN1_PIN |
                            MOTOR_R_IN2_PIN | OLED_SCL_PIN | OLED_SDA_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_SET);

    gpio.Pin = ULTRASONIC_TRIG_PIN | BUZZER_PIN | MOTOR_STBY_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = MOTOR_L_IN1_PIN | MOTOR_L_IN2_PIN | MOTOR_R_IN1_PIN | MOTOR_R_IN2_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = STATUS_LED_PIN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, OLED_SCL_PIN | OLED_SDA_PIN, GPIO_PIN_SET);

    gpio.Pin = IR0_PIN | IR1_PIN | IR2_PIN | IR3_PIN | IR4_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = BUTTON_PIN;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BUTTON_PORT, &gpio);

    gpio.Pin = ULTRASONIC_ECHO_PIN;
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(ULTRASONIC_ECHO_PORT, &gpio);

    HAL_NVIC_SetPriority(EXTI4_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
}

static void ADC1_Init(void)
{
    ADC_ChannelConfTypeDef channel = {0};
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);
    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1U;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
    channel.Channel = ADC_CHANNEL_2;
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK) Error_Handler();
    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) Error_Handler();
}

static void TIM1_Init(void)
{
    TIM_OC_InitTypeDef channel = {0};
    __HAL_RCC_TIM1_CLK_ENABLE();
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 71U;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = 19999U;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0U;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) Error_Handler();
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = 1500U;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    channel.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    channel.OCFastMode = TIM_OCFAST_DISABLE;
    channel.OCIdleState = TIM_OCIDLESTATE_RESET;
    channel.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &channel, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    HAL_TIM_MspPostInit(&htim1);
}

static void TIM2_Init(void)
{
    TIM_OC_InitTypeDef channel = {0};
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0U;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = MOTOR_PWM_PERIOD;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = 0U;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    channel.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &channel, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&htim2, &channel, TIM_CHANNEL_2) != HAL_OK) Error_Handler();
    HAL_TIM_MspPostInit(&htim2);
}

static void encoder_timer_init(TIM_HandleTypeDef *timer, TIM_TypeDef *instance)
{
    TIM_Encoder_InitTypeDef encoder = {0};
    timer->Instance = instance;
    timer->Init.Prescaler = 0U;
    timer->Init.CounterMode = TIM_COUNTERMODE_UP;
    timer->Init.Period = 65535U;
    timer->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    encoder.EncoderMode = TIM_ENCODERMODE_TI12;
    encoder.IC1Polarity = TIM_ICPOLARITY_RISING;
    encoder.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    encoder.IC1Prescaler = TIM_ICPSC_DIV1;
    encoder.IC1Filter = 6U;
    encoder.IC2Polarity = TIM_ICPOLARITY_RISING;
    encoder.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    encoder.IC2Prescaler = TIM_ICPSC_DIV1;
    encoder.IC2Filter = 6U;
    if (HAL_TIM_Encoder_Init(timer, &encoder) != HAL_OK) Error_Handler();
}

static void TIM3_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    encoder_timer_init(&htim3, TIM3);
}

static void TIM4_Init(void)
{
    __HAL_RCC_TIM4_CLK_ENABLE();
    encoder_timer_init(&htim4, TIM4);
}

static void USART1_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200U;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(USART1_IRQn, 6U, 0U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void IWDG_Init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload = 1250U;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) Error_Handler();
}
