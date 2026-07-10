#ifndef BOARD_H
#define BOARD_H

#include "main.h"

#define MOTOR_PWM_PERIOD       3599U
#define MOTOR_PWM_LEFT_CH      TIM_CHANNEL_1
#define MOTOR_PWM_RIGHT_CH     TIM_CHANNEL_2

#define MOTOR_L_IN1_PORT       GPIOB
#define MOTOR_L_IN1_PIN        GPIO_PIN_12
#define MOTOR_L_IN2_PORT       GPIOB
#define MOTOR_L_IN2_PIN        GPIO_PIN_13
#define MOTOR_R_IN1_PORT       GPIOB
#define MOTOR_R_IN1_PIN        GPIO_PIN_14
#define MOTOR_R_IN2_PORT       GPIOB
#define MOTOR_R_IN2_PIN        GPIO_PIN_15
#define MOTOR_STBY_PORT        GPIOA
#define MOTOR_STBY_PIN         GPIO_PIN_12

#define ULTRASONIC_TRIG_PORT   GPIOA
#define ULTRASONIC_TRIG_PIN    GPIO_PIN_3
#define ULTRASONIC_ECHO_PORT   GPIOA
#define ULTRASONIC_ECHO_PIN    GPIO_PIN_4

#define BUZZER_PORT            GPIOA
#define BUZZER_PIN             GPIO_PIN_11
#define BUTTON_PORT            GPIOA
#define BUTTON_PIN             GPIO_PIN_15
#define STATUS_LED_PORT        GPIOC
#define STATUS_LED_PIN         GPIO_PIN_13

#define OLED_SCL_PORT          GPIOB
#define OLED_SCL_PIN           GPIO_PIN_8
#define OLED_SDA_PORT          GPIOB
#define OLED_SDA_PIN           GPIO_PIN_9

#define IR0_PORT GPIOB
#define IR0_PIN  GPIO_PIN_0
#define IR1_PORT GPIOB
#define IR1_PIN  GPIO_PIN_1
#define IR2_PORT GPIOB
#define IR2_PIN  GPIO_PIN_3
#define IR3_PORT GPIOB
#define IR3_PIN  GPIO_PIN_4
#define IR4_PORT GPIOB
#define IR4_PIN  GPIO_PIN_5

/* Runtime calibration values (battery, encoder CPR, wheel geometry, PID)
 * live in App/Src/app_config.c and can be changed through serial commands. */
#define LEFT_ENCODER_SIGN      (1.0f)
#define RIGHT_ENCODER_SIGN     (1.0f)

#endif
