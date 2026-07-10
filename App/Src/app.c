#include "app.h"
#include "app_config.h"
#include "bsp.h"
#include "pid.h"
#include "protocol.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "task.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define HEALTH_CONTROL  (1U << 0)
#define HEALTH_SAFETY   (1U << 1)
#define HEALTH_COMM     (1U << 2)
#define HEALTH_BEHAVIOR (1U << 3)
#define HEALTH_SENSOR   (1U << 4)
#define HEALTH_DISPLAY  (1U << 5)
#define HEALTH_ALL      (HEALTH_CONTROL | HEALTH_SAFETY | HEALTH_COMM | \
                         HEALTH_BEHAVIOR | HEALTH_SENSOR | HEALTH_DISPLAY)
#define CONTROL_PERIOD_MS 10U
#define MANUAL_TIMEOUT_MS 1000U
#define PI_F 3.14159265358979323846f
#define FAULT_LOG_DEPTH 8U
#define ROUTE_DEPTH 8U

typedef enum {
    MOTION_NONE = 0,
    MOTION_MOVE,
    MOTION_TURN
} MotionType;

typedef struct {
    MotionType type;
    float value;
    float start_left_cm;
    float start_right_cm;
    float start_heading_deg;
} MotionGoal;

typedef struct {
    uint32_t timestamp_ms;
    uint32_t faults;
} FaultLogEntry;

typedef struct {
    MotionType type;
    float value;
} RouteStep;

static CarState state;
static PIDController pid_left;
static PIDController pid_right;
static AppConfig config;
static MotionGoal motion;
static StreamBufferHandle_t uart_stream;
static SemaphoreHandle_t ultrasonic_done;
static SemaphoreHandle_t uart_mutex;
static volatile uint32_t echo_rise_us;
static volatile uint32_t echo_width_us;
static volatile bool echo_rise_seen;
static volatile uint32_t health_bits;
static bool estop_latched;
static volatile bool stall_latched;
static FaultLogEntry fault_log[FAULT_LOG_DEPTH];
static uint8_t fault_log_head;
static uint8_t fault_log_count;
static TaskHandle_t task_handles[7];
static RouteStep route[ROUTE_DEPTH];
static uint8_t route_count;
static uint8_t route_index;
static bool route_running;

static void ControlTask(void *argument);
static void SafetyTask(void *argument);
static void CommunicationTask(void *argument);
static void BehaviorTask(void *argument);
static void SensorTask(void *argument);
static void DisplayTask(void *argument);
static void MonitorTask(void *argument);

static void config_get(AppConfig *destination)
{
    taskENTER_CRITICAL();
    *destination = config;
    taskEXIT_CRITICAL();
}

static float approach(float current, float target, float maximum_step)
{
    if (current < target) return fminf(current + maximum_step, target);
    if (current > target) return fmaxf(current - maximum_step, target);
    return current;
}

static void fault_log_add(uint32_t faults)
{
    const uint32_t timestamp = HAL_GetTick();
    taskENTER_CRITICAL();
    fault_log[fault_log_head].timestamp_ms = timestamp;
    fault_log[fault_log_head].faults = faults;
    fault_log_head = (uint8_t)((fault_log_head + 1U) % FAULT_LOG_DEPTH);
    if (fault_log_count < FAULT_LOG_DEPTH) ++fault_log_count;
    taskEXIT_CRITICAL();
}

static void health_mark(uint32_t bit)
{
    taskENTER_CRITICAL();
    health_bits |= bit;
    taskEXIT_CRITICAL();
}

static void uart_write(const char *text)
{
    if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(100U)) == pdTRUE) {
        BSP_UartWrite(text);
        xSemaphoreGive(uart_mutex);
    }
}

void App_GetState(CarState *destination)
{
    taskENTER_CRITICAL();
    *destination = state;
    taskEXIT_CRITICAL();
}

static void set_targets(float left, float right)
{
    taskENTER_CRITICAL();
    state.target_left_rpm = left;
    state.target_right_rpm = right;
    taskEXIT_CRITICAL();
}

static void set_distance(float distance_cm)
{
    taskENTER_CRITICAL();
    state.distance_cm = distance_cm;
    taskEXIT_CRITICAL();
}

static float ultrasonic_measure(void)
{
    echo_rise_seen = false;
    echo_width_us = 0U;
    (void)xSemaphoreTake(ultrasonic_done, 0U);
    BSP_UltrasonicTrigger();
    if (xSemaphoreTake(ultrasonic_done, pdMS_TO_TICKS(40U)) != pdTRUE) return 400.0f;
    if (echo_width_us < 100U || echo_width_us > 30000U) return 400.0f;
    return (float)echo_width_us * 0.01715f;
}

void App_NotifyUartByteFromISR(uint8_t byte)
{
    BaseType_t wake = pdFALSE;
    if (uart_stream != NULL) {
        (void)xStreamBufferSendFromISR(uart_stream, &byte, 1U, &wake);
        portYIELD_FROM_ISR(wake);
    }
}

void App_NotifyUltrasonicEdgeFromISR(bool high_level, uint32_t timestamp_us)
{
    BaseType_t wake = pdFALSE;
    if (high_level) {
        echo_rise_us = timestamp_us;
        echo_rise_seen = true;
    } else if (echo_rise_seen) {
        echo_width_us = timestamp_us - echo_rise_us;
        echo_rise_seen = false;
        if (ultrasonic_done != NULL) {
            xSemaphoreGiveFromISR(ultrasonic_done, &wake);
            portYIELD_FROM_ISR(wake);
        }
    }
}

void App_Init(void)
{
    (void)AppConfig_Load(&config);
    memset(&state, 0, sizeof(state));
    memset(&motion, 0, sizeof(motion));
    state.mode = MODE_MANUAL;
    state.distance_cm = 400.0f;
    state.battery_v = 8.0f;
    state.last_command_ms = HAL_GetTick();
    PID_Init(&pid_left, config.pid_kp, config.pid_ki, config.pid_kd, 100.0f, 80.0f);
    PID_Init(&pid_right, config.pid_kp, config.pid_ki, config.pid_kd, 100.0f, 80.0f);

    uart_stream = xStreamBufferCreate(128U, 1U);
    ultrasonic_done = xSemaphoreCreateBinary();
    uart_mutex = xSemaphoreCreateMutex();
    if (uart_stream == NULL || ultrasonic_done == NULL || uart_mutex == NULL) Error_Handler();
}

void App_StartScheduler(void)
{
    BaseType_t ok = pdPASS;
    ok &= xTaskCreate(ControlTask, "Control", 176U, NULL, 5U, &task_handles[0]);
    ok &= xTaskCreate(SafetyTask, "Safety", 160U, NULL, 6U, &task_handles[1]);
    ok &= xTaskCreate(CommunicationTask, "Comm", 256U, NULL, 4U, &task_handles[2]);
    ok &= xTaskCreate(BehaviorTask, "Behavior", 208U, NULL, 3U, &task_handles[3]);
    ok &= xTaskCreate(SensorTask, "Sensor", 160U, NULL, 2U, &task_handles[4]);
    ok &= xTaskCreate(DisplayTask, "Display", 224U, NULL, 1U, &task_handles[5]);
    ok &= xTaskCreate(MonitorTask, "Monitor", 176U, NULL, 1U, &task_handles[6]);
    if (ok != pdPASS) Error_Handler();
    vTaskStartScheduler();
    Error_Handler();
}

static void ControlTask(void *argument)
{
    /* Highest-rate control path: sample encoders, integrate odometry, apply
     * acceleration limiting, run both PID loops, and detect motor stalls. */
    TickType_t wake = xTaskGetTickCount();
    float filtered_left = 0.0f, filtered_right = 0.0f;
    float ramped_left = 0.0f, ramped_right = 0.0f;
    uint32_t stall_left_ms = 0U, stall_right_ms = 0U;
    (void)argument;
    for (;;) {
        int16_t delta_left, delta_right;
        CarState snapshot;
        AppConfig cfg;
        float rpm_left, rpm_right, output_left = 0.0f, output_right = 0.0f;
        float distance_left, distance_right, center_distance, delta_heading;
        float heading_mid_rad, new_x, new_y, new_heading;

        BSP_EncoderReadDelta(&delta_left, &delta_right);
        config_get(&cfg);
        rpm_left = LEFT_ENCODER_SIGN * (float)delta_left * (60000.0f / CONTROL_PERIOD_MS) /
                   cfg.encoder_counts_per_rev;
        rpm_right = RIGHT_ENCODER_SIGN * (float)delta_right * (60000.0f / CONTROL_PERIOD_MS) /
                    cfg.encoder_counts_per_rev;
        filtered_left += 0.30f * (rpm_left - filtered_left);
        filtered_right += 0.30f * (rpm_right - filtered_right);
        App_GetState(&snapshot);

        /* Differential-drive odometry. Heading remains unwrapped so TURN can
         * measure rotations across the -180/180 degree boundary. */
        distance_left = LEFT_ENCODER_SIGN * (float)delta_left * PI_F * cfg.wheel_diameter_cm /
                        cfg.encoder_counts_per_rev;
        distance_right = RIGHT_ENCODER_SIGN * (float)delta_right * PI_F * cfg.wheel_diameter_cm /
                         cfg.encoder_counts_per_rev;
        center_distance = 0.5f * (distance_left + distance_right);
        delta_heading = (distance_right - distance_left) / cfg.track_width_cm;
        heading_mid_rad = snapshot.heading_deg * PI_F / 180.0f + 0.5f * delta_heading;
        new_x = snapshot.odom_x_cm + center_distance * cosf(heading_mid_rad);
        new_y = snapshot.odom_y_cm + center_distance * sinf(heading_mid_rad);
        new_heading = snapshot.heading_deg + delta_heading * 180.0f / PI_F;

        if (!snapshot.motors_enabled || snapshot.faults != FAULT_NONE) {
            ramped_left = 0.0f;
            ramped_right = 0.0f;
            stall_left_ms = stall_right_ms = 0U;
            PID_Reset(&pid_left);
            PID_Reset(&pid_right);
            BSP_MotorStop();
        } else {
            const float ramp_step = cfg.acceleration_rpm_s * 0.010f;
            ramped_left = approach(ramped_left, snapshot.target_left_rpm, ramp_step);
            ramped_right = approach(ramped_right, snapshot.target_right_rpm, ramp_step);
            output_left = PID_Update(&pid_left, ramped_left, filtered_left, 0.010f);
            output_right = PID_Update(&pid_right, ramped_right, filtered_right, 0.010f);
            BSP_MotorSet(output_left, output_right);

            /* Stall is latched only after sustained high effort and near-zero
             * encoder speed. Short starts and direction changes are ignored. */
            if (fabsf(output_left) >= cfg.stall_pwm_percent &&
                fabsf(ramped_left) >= 30.0f && fabsf(filtered_left) < cfg.stall_rpm) {
                stall_left_ms += CONTROL_PERIOD_MS;
            } else stall_left_ms = 0U;
            if (fabsf(output_right) >= cfg.stall_pwm_percent &&
                fabsf(ramped_right) >= 30.0f && fabsf(filtered_right) < cfg.stall_rpm) {
                stall_right_ms += CONTROL_PERIOD_MS;
            } else stall_right_ms = 0U;
            if (stall_left_ms >= cfg.stall_time_ms || stall_right_ms >= cfg.stall_time_ms) {
                stall_latched = true;
            }
        }
        taskENTER_CRITICAL();
        state.actual_left_rpm = filtered_left;
        state.actual_right_rpm = filtered_right;
        state.odom_x_cm = new_x;
        state.odom_y_cm = new_y;
        state.heading_deg = new_heading;
        state.travel_left_cm = snapshot.travel_left_cm + distance_left;
        state.travel_right_cm = snapshot.travel_right_cm + distance_right;
        taskEXIT_CRITICAL();
        health_mark(HEALTH_CONTROL);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(CONTROL_PERIOD_MS));
    }
}

static void SafetyTask(void *argument)
{
    /* Safety owns the final motor-enable decision. Faults zero setpoints and
     * cancel autonomous motion, so CLEAR never resumes a stale command. */
    TickType_t wake = xTaskGetTickCount();
    bool low_battery = false;
    uint32_t previous_faults = FAULT_NONE;
    (void)argument;
    for (;;) {
        CarState snapshot;
        AppConfig cfg;
        uint32_t faults = FAULT_NONE;
        App_GetState(&snapshot);
        config_get(&cfg);
        if (BSP_EStopActive()) estop_latched = true;
        if (snapshot.battery_v > 1.0f && snapshot.battery_v < cfg.battery_low_v) low_battery = true;
        else if (snapshot.battery_v >= cfg.battery_recover_v) low_battery = false;
        if (low_battery) faults |= FAULT_LOW_BATTERY;
        if (estop_latched) faults |= FAULT_ESTOP;
        if (stall_latched) faults |= FAULT_MOTOR_STALL;
        if (snapshot.mode == MODE_MANUAL &&
            (fabsf(snapshot.target_left_rpm) > 0.1f || fabsf(snapshot.target_right_rpm) > 0.1f) &&
            (HAL_GetTick() - snapshot.last_command_ms) > MANUAL_TIMEOUT_MS) {
            faults |= FAULT_COMMAND_TIMEOUT;
        }
        taskENTER_CRITICAL();
        state.faults = faults;
        if (faults != FAULT_NONE) {
            state.target_left_rpm = 0.0f;
            state.target_right_rpm = 0.0f;
            motion.type = MOTION_NONE;
            route_running = false;
        }
        state.motors_enabled = faults == FAULT_NONE &&
            (fabsf(snapshot.target_left_rpm) > 0.1f || fabsf(snapshot.target_right_rpm) > 0.1f);
        taskEXIT_CRITICAL();
        if (faults != previous_faults) {
            fault_log_add(faults);
            previous_faults = faults;
        }
        BSP_BuzzerSet(faults != FAULT_NONE && ((HAL_GetTick() / 250U) & 1U) != 0U);
        health_mark(HEALTH_SAFETY);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(50U));
    }
}

static void send_status(void)
{
    CarState s;
    char line[192];
    App_GetState(&s);
    (void)snprintf(line, sizeof(line),
        "STAT mode=%u target=%.1f,%.1f rpm=%.1f,%.1f dist=%.1f bat=%.2f "
        "pose=%.1f,%.1f,%.1f ir=%02X fault=%lu\r\n",
        (unsigned)s.mode, (double)s.target_left_rpm, (double)s.target_right_rpm,
        (double)s.actual_left_rpm, (double)s.actual_right_rpm, (double)s.distance_cm,
        (double)s.battery_v, (double)s.odom_x_cm, (double)s.odom_y_cm,
        (double)s.heading_deg, s.line_bits, (unsigned long)s.faults);
    uart_write(line);
}

static bool vehicle_stopped(void)
{
    CarState s;
    App_GetState(&s);
    return s.mode == MODE_MANUAL && !s.motors_enabled && fabsf(s.target_left_rpm) < 0.1f &&
           fabsf(s.target_right_rpm) < 0.1f;
}

static void apply_pid_config(void)
{
    taskENTER_CRITICAL();
    PID_Init(&pid_left, config.pid_kp, config.pid_ki, config.pid_kd, 100.0f, 80.0f);
    PID_Init(&pid_right, config.pid_kp, config.pid_ki, config.pid_kd, 100.0f, 80.0f);
    taskEXIT_CRITICAL();
}

static void send_config(void)
{
    AppConfig cfg;
    char line[192];
    config_get(&cfg);
    (void)snprintf(line, sizeof(line),
        "CFG seq=%lu pid=%.3f,%.3f,%.4f speed=%.1f ramp=%.1f line=%.1f\r\n",
        (unsigned long)cfg.sequence, (double)cfg.pid_kp, (double)cfg.pid_ki,
        (double)cfg.pid_kd, (double)cfg.base_speed_rpm,
        (double)cfg.acceleration_rpm_s, (double)cfg.line_gain);
    uart_write(line);
    (void)snprintf(line, sizeof(line),
        "CFG wheel=%.2f track=%.2f cpr=%.1f bat=%.2f,%.2f,%.4f stall=%.1f,%.1f,%lu\r\n",
        (double)cfg.wheel_diameter_cm, (double)cfg.track_width_cm,
        (double)cfg.encoder_counts_per_rev, (double)cfg.battery_low_v,
        (double)cfg.battery_recover_v, (double)cfg.battery_divider_ratio,
        (double)cfg.stall_pwm_percent, (double)cfg.stall_rpm,
        (unsigned long)cfg.stall_time_ms);
    uart_write(line);
}

static void send_odometry(void)
{
    CarState s;
    char line[160];
    App_GetState(&s);
    (void)snprintf(line, sizeof(line),
        "ODOM x=%.2fcm y=%.2fcm heading=%.2fdeg left=%.2fcm right=%.2fcm\r\n",
        (double)s.odom_x_cm, (double)s.odom_y_cm, (double)s.heading_deg,
        (double)s.travel_left_cm, (double)s.travel_right_cm);
    uart_write(line);
}

static void send_diagnostics(void)
{
    char line[192];
    (void)snprintf(line, sizeof(line),
        "DIAG heap_free=%lu heap_min=%lu reset=0x%02lX\r\n",
        (unsigned long)xPortGetFreeHeapSize(),
        (unsigned long)xPortGetMinimumEverFreeHeapSize(),
        (unsigned long)BSP_ResetCause());
    uart_write(line);
    (void)snprintf(line, sizeof(line),
        "DIAG stack_words control=%u safety=%u comm=%u behavior=%u sensor=%u display=%u monitor=%u\r\n",
        (unsigned)uxTaskGetStackHighWaterMark(task_handles[0]),
        (unsigned)uxTaskGetStackHighWaterMark(task_handles[1]),
        (unsigned)uxTaskGetStackHighWaterMark(task_handles[2]),
        (unsigned)uxTaskGetStackHighWaterMark(task_handles[3]),
        (unsigned)uxTaskGetStackHighWaterMark(task_handles[4]),
        (unsigned)uxTaskGetStackHighWaterMark(task_handles[5]),
        (unsigned)uxTaskGetStackHighWaterMark(task_handles[6]));
    uart_write(line);
}

static void send_fault_log(void)
{
    char line[80];
    FaultLogEntry snapshot[FAULT_LOG_DEPTH];
    uint8_t count;
    taskENTER_CRITICAL();
    count = fault_log_count;
    const uint8_t first = (uint8_t)((fault_log_head + FAULT_LOG_DEPTH - count) %
                                    FAULT_LOG_DEPTH);
    for (uint8_t i = 0U; i < count; ++i) snapshot[i] = fault_log[(first + i) % FAULT_LOG_DEPTH];
    taskEXIT_CRITICAL();
    if (count == 0U) {
        uart_write("LOG EMPTY\r\n");
        return;
    }
    for (uint8_t i = 0U; i < count; ++i) {
        const FaultLogEntry *entry = &snapshot[i];
        (void)snprintf(line, sizeof(line), "LOG t=%lums fault=0x%02lX\r\n",
                       (unsigned long)entry->timestamp_ms, (unsigned long)entry->faults);
        uart_write(line);
    }
}

static void send_route(void)
{
    char line[80];
    if (route_count == 0U) {
        uart_write("ROUTE EMPTY\r\n");
        return;
    }
    for (uint8_t i = 0U; i < route_count; ++i) {
        (void)snprintf(line, sizeof(line), "ROUTE %u %s %.1f%s\r\n",
                       (unsigned)(i + 1U), route[i].type == MOTION_MOVE ? "MOVE" : "TURN",
                       (double)route[i].value,
                       route_running && i == route_index ? " ACTIVE" : "");
        uart_write(line);
    }
}

static void start_motion(MotionType type, float value, const CarState *snapshot)
{
    taskENTER_CRITICAL();
    motion.type = type;
    motion.value = value;
    motion.start_left_cm = snapshot->travel_left_cm;
    motion.start_right_cm = snapshot->travel_right_cm;
    motion.start_heading_deg = snapshot->heading_deg;
    state.mode = MODE_MOTION;
    state.target_left_rpm = state.target_right_rpm = 0.0f;
    taskEXIT_CRITICAL();
}

static void cancel_motion(void)
{
    taskENTER_CRITICAL();
    motion.type = MOTION_NONE;
    route_running = false;
    state.mode = MODE_MANUAL;
    state.target_left_rpm = 0.0f;
    state.target_right_rpm = 0.0f;
    state.last_command_ms = HAL_GetTick();
    taskEXIT_CRITICAL();
}

static void complete_motion(const CarState *snapshot, const char *event)
{
    bool start_next = false;
    RouteStep next = {0};
    taskENTER_CRITICAL();
    if (route_running && route_index + 1U < route_count) {
        ++route_index;
        next = route[route_index];
        start_next = true;
    } else {
        route_running = false;
        motion.type = MOTION_NONE;
        state.mode = MODE_MANUAL;
        state.target_left_rpm = state.target_right_rpm = 0.0f;
    }
    taskEXIT_CRITICAL();
    if (start_next) {
        start_motion(next.type, next.value, snapshot);
        uart_write(event);
        uart_write("EVENT ROUTE NEXT\r\n");
    } else {
        uart_write(event);
        uart_write("EVENT MOTION IDLE\r\n");
    }
}

static void execute_command(const ParsedCommand *command)
{
    /* Configuration commands change RAM first. SAVE is explicit to prevent
     * accidental Flash wear during interactive tuning. */
    switch (command->type) {
    case CMD_MODE:
        taskENTER_CRITICAL();
        motion.type = MOTION_NONE;
        route_running = false;
        state.mode = (CarMode)command->mode;
        state.target_left_rpm = state.target_right_rpm = 0.0f;
        state.last_command_ms = HAL_GetTick();
        taskEXIT_CRITICAL();
        uart_write("OK MODE\r\n");
        break;
    case CMD_DRIVE:
        taskENTER_CRITICAL();
        motion.type = MOTION_NONE;
        route_running = false;
        state.mode = MODE_MANUAL;
        state.target_left_rpm = fmaxf(-180.0f, fminf(180.0f, command->a));
        state.target_right_rpm = fmaxf(-180.0f, fminf(180.0f, command->b));
        state.last_command_ms = HAL_GetTick();
        taskEXIT_CRITICAL();
        uart_write("OK DRIVE\r\n");
        break;
    case CMD_SPEED:
        if (command->a < 20.0f || command->a > 150.0f) uart_write("ERR SPEED RANGE\r\n");
        else {
            taskENTER_CRITICAL();
            config.base_speed_rpm = command->a;
            state.last_command_ms = HAL_GetTick();
            taskEXIT_CRITICAL();
            uart_write("OK SPEED RAM - USE SAVE\r\n");
        }
        break;
    case CMD_PID:
        if (command->a < 0.0f || command->a > 20.0f ||
            command->b < 0.0f || command->b > 20.0f ||
            command->c < 0.0f || command->c > 5.0f) {
            uart_write("ERR PID RANGE\r\n");
        } else {
            taskENTER_CRITICAL();
            config.pid_kp = command->a;
            config.pid_ki = command->b;
            config.pid_kd = command->c;
            PID_Init(&pid_left, command->a, command->b, command->c, 100.0f, 80.0f);
            PID_Init(&pid_right, command->a, command->b, command->c, 100.0f, 80.0f);
            taskEXIT_CRITICAL();
            uart_write("OK PID RAM - USE SAVE\r\n");
        }
        break;
    case CMD_STOP:
        estop_latched = true;
        cancel_motion();
        uart_write("OK ESTOP LATCHED\r\n");
        break;
    case CMD_CLEAR:
        if (BSP_EStopActive()) uart_write("ERR BUTTON ACTIVE\r\n");
        else {
            estop_latched = false;
            stall_latched = false;
            taskENTER_CRITICAL(); state.last_command_ms = HAL_GetTick(); taskEXIT_CRITICAL();
            uart_write("OK CLEAR\r\n");
        }
        break;
    case CMD_STATUS: send_status(); break;
    case CMD_HELP: uart_write(Protocol_Help()); break;
    case CMD_MOVE:
    case CMD_TURN: {
        CarState s;
        if (command->a == 0.0f || fabsf(command->a) > 1000.0f) {
            uart_write("ERR MOTION RANGE\r\n");
            break;
        }
        App_GetState(&s);
        if (s.faults != FAULT_NONE) {
            uart_write("ERR FAULT ACTIVE\r\n");
            break;
        }
        route_running = false;
        start_motion(command->type == CMD_MOVE ? MOTION_MOVE : MOTION_TURN,
                     command->a, &s);
        uart_write(command->type == CMD_MOVE ? "OK MOVE\r\n" : "OK TURN\r\n");
        break;
    }
    case CMD_CANCEL:
        cancel_motion();
        uart_write("OK CANCEL\r\n");
        break;
    case CMD_ODOM: send_odometry(); break;
    case CMD_RESET_ODOM:
        if (!vehicle_stopped()) uart_write("ERR STOP FIRST\r\n");
        else {
            taskENTER_CRITICAL();
            state.odom_x_cm = state.odom_y_cm = state.heading_deg = 0.0f;
            state.travel_left_cm = state.travel_right_cm = 0.0f;
            taskEXIT_CRITICAL();
            uart_write("OK RESETODOM\r\n");
        }
        break;
    case CMD_SAVE:
        if (!vehicle_stopped()) uart_write("ERR STOP FIRST\r\n");
        else uart_write(AppConfig_Save(&config) ? "OK SAVED\r\n" : "ERR FLASH\r\n");
        break;
    case CMD_LOAD:
        if (!vehicle_stopped()) uart_write("ERR STOP FIRST\r\n");
        else {
            AppConfig loaded;
            const bool found = AppConfig_Load(&loaded);
            taskENTER_CRITICAL(); config = loaded; taskEXIT_CRITICAL();
            apply_pid_config();
            uart_write(found ? "OK LOADED\r\n" : "OK DEFAULTS NO RECORD\r\n");
        }
        break;
    case CMD_DEFAULTS:
        if (!vehicle_stopped()) uart_write("ERR STOP FIRST\r\n");
        else {
            const uint32_t sequence = config.sequence;
            taskENTER_CRITICAL();
            AppConfig_Defaults(&config);
            config.sequence = sequence;
            taskEXIT_CRITICAL();
            apply_pid_config();
            uart_write("OK DEFAULTS RAM - USE SAVE\r\n");
        }
        break;
    case CMD_CONFIG: send_config(); break;
    case CMD_WHEEL:
        if (command->a < 2.0f || command->a > 20.0f || command->b < 5.0f ||
            command->b > 50.0f || command->c < 20.0f || command->c > 20000.0f) {
            uart_write("ERR WHEEL RANGE\r\n");
        } else {
            taskENTER_CRITICAL();
            config.wheel_diameter_cm = command->a;
            config.track_width_cm = command->b;
            config.encoder_counts_per_rev = command->c;
            taskEXIT_CRITICAL();
            uart_write("OK WHEEL RAM - USE SAVE\r\n");
        }
        break;
    case CMD_RAMP:
        if (command->a < 20.0f || command->a > 1000.0f) uart_write("ERR RAMP RANGE\r\n");
        else { taskENTER_CRITICAL(); config.acceleration_rpm_s = command->a; taskEXIT_CRITICAL();
               uart_write("OK RAMP RAM - USE SAVE\r\n"); }
        break;
    case CMD_LINE_GAIN:
        if (command->a < 1.0f || command->a > 40.0f) uart_write("ERR LINEGAIN RANGE\r\n");
        else { taskENTER_CRITICAL(); config.line_gain = command->a; taskEXIT_CRITICAL();
               uart_write("OK LINEGAIN RAM - USE SAVE\r\n"); }
        break;
    case CMD_BATTERY:
        if (command->a < 2.5f || command->b < command->a || command->b > 21.0f ||
            command->c < 1.0f || command->c > 10.0f) uart_write("ERR BATTERY RANGE\r\n");
        else {
            taskENTER_CRITICAL();
            config.battery_low_v = command->a;
            config.battery_recover_v = command->b;
            config.battery_divider_ratio = command->c;
            taskEXIT_CRITICAL();
            uart_write("OK BATTERY RAM - USE SAVE\r\n");
        }
        break;
    case CMD_STALL:
        if (command->a < 20.0f || command->a > 100.0f || command->b < 0.5f ||
            command->b > 30.0f || command->c < 200.0f || command->c > 5000.0f) {
            uart_write("ERR STALL RANGE\r\n");
        } else {
            taskENTER_CRITICAL();
            config.stall_pwm_percent = command->a;
            config.stall_rpm = command->b;
            config.stall_time_ms = (uint32_t)command->c;
            taskEXIT_CRITICAL();
            uart_write("OK STALL RAM - USE SAVE\r\n");
        }
        break;
    case CMD_DIAG: send_diagnostics(); break;
    case CMD_LOG: send_fault_log(); break;
    case CMD_VERSION: uart_write("SMARTCAR FW 2.0 C8-SAFE 2026-07-10\r\n"); break;
    case CMD_QUEUE_MOVE:
    case CMD_QUEUE_TURN:
        if (command->a == 0.0f || fabsf(command->a) > 1000.0f) {
            uart_write("ERR ROUTE RANGE\r\n");
        } else if (route_running) {
            uart_write("ERR ROUTE RUNNING\r\n");
        } else if (route_count >= ROUTE_DEPTH) {
            uart_write("ERR ROUTE FULL\r\n");
        } else {
            route[route_count].type = command->type == CMD_QUEUE_MOVE ? MOTION_MOVE : MOTION_TURN;
            route[route_count].value = command->a;
            ++route_count;
            uart_write("OK ROUTE ADDED\r\n");
        }
        break;
    case CMD_RUN_ROUTE: {
        CarState s;
        App_GetState(&s);
        if (route_count == 0U) uart_write("ERR ROUTE EMPTY\r\n");
        else if (!vehicle_stopped()) uart_write("ERR STOP FIRST\r\n");
        else if (s.faults != FAULT_NONE) uart_write("ERR FAULT ACTIVE\r\n");
        else {
            route_index = 0U;
            route_running = true;
            start_motion(route[0].type, route[0].value, &s);
            uart_write("OK ROUTE RUN\r\n");
        }
        break;
    }
    case CMD_CLEAR_ROUTE:
        if (route_running) uart_write("ERR ROUTE RUNNING\r\n");
        else { route_count = route_index = 0U; uart_write("OK ROUTE CLEARED\r\n"); }
        break;
    case CMD_ROUTE: send_route(); break;
    default: uart_write("ERR COMMAND\r\n"); break;
    }
}

static void CommunicationTask(void *argument)
{
    /* USART ISR supplies bytes through a StreamBuffer. Parsing stays in task
     * context so the interrupt remains short and deterministic. */
    char line[96];
    size_t length = 0U;
    (void)argument;
    uart_write("SMARTCAR READY - HELP for commands\r\n");
    for (;;) {
        uint8_t byte;
        if (xStreamBufferReceive(uart_stream, &byte, 1U, pdMS_TO_TICKS(100U)) == 1U) {
            if (byte == '\r' || byte == '\n') {
                if (length != 0U) {
                    ParsedCommand command;
                    line[length] = '\0';
                    if (Protocol_Parse(line, &command)) execute_command(&command);
                    else uart_write("ERR SYNTAX\r\n");
                    length = 0U;
                }
            } else if (length + 1U < sizeof(line)) line[length++] = (char)byte;
            else { length = 0U; uart_write("ERR TOO LONG\r\n"); }
        }
        health_mark(HEALTH_COMM);
    }
}

static void track_step(const CarState *snapshot)
{
    AppConfig cfg;
    config_get(&cfg);
    if (snapshot->line_bits == 0U) {
        const float direction = snapshot->line_error <= 0 ? -1.0f : 1.0f;
        set_targets(direction * 45.0f, -direction * 45.0f);
    } else {
        const float steering = (float)snapshot->line_error * cfg.line_gain;
        set_targets(cfg.base_speed_rpm + steering, cfg.base_speed_rpm - steering);
    }
}

static void avoid_step(void)
{
    AppConfig cfg;
    config_get(&cfg);
    float center = ultrasonic_measure();
    set_distance(center);
    if (center > 35.0f) { set_targets(cfg.base_speed_rpm, cfg.base_speed_rpm); return; }
    set_targets(0.0f, 0.0f);
    BSP_ServoSetAngle(-55); vTaskDelay(pdMS_TO_TICKS(220U));
    float left = ultrasonic_measure();
    BSP_ServoSetAngle(55); vTaskDelay(pdMS_TO_TICKS(220U));
    float right = ultrasonic_measure();
    BSP_ServoSetAngle(0); vTaskDelay(pdMS_TO_TICKS(120U));
    if (left > right && left > 25.0f) set_targets(-55.0f, 55.0f);
    else if (right > 25.0f) set_targets(55.0f, -55.0f);
    else set_targets(-55.0f, -55.0f);
    vTaskDelay(pdMS_TO_TICKS(350U));
    set_targets(0.0f, 0.0f);
}

static void motion_step(const CarState *snapshot)
{
    AppConfig cfg;
    MotionGoal goal;
    float progress, remaining, direction, speed;
    config_get(&cfg);
    taskENTER_CRITICAL(); goal = motion; taskEXIT_CRITICAL();

    if (goal.type == MOTION_MOVE) {
        progress = fabsf(0.5f * ((snapshot->travel_left_cm - goal.start_left_cm) +
                                (snapshot->travel_right_cm - goal.start_right_cm)));
        remaining = fabsf(goal.value) - progress;
        if (remaining <= 0.8f) {
            complete_motion(snapshot, "EVENT MOVE DONE\r\n");
            return;
        }
        direction = goal.value > 0.0f ? 1.0f : -1.0f;
        speed = fmaxf(25.0f, fminf(cfg.base_speed_rpm, remaining * 4.0f));
        set_targets(direction * speed, direction * speed);
    } else if (goal.type == MOTION_TURN) {
        progress = fabsf(snapshot->heading_deg - goal.start_heading_deg);
        remaining = fabsf(goal.value) - progress;
        if (remaining <= 1.5f) {
            complete_motion(snapshot, "EVENT TURN DONE\r\n");
            return;
        }
        direction = goal.value > 0.0f ? 1.0f : -1.0f;
        speed = fmaxf(25.0f, fminf(60.0f, remaining * 1.5f));
        set_targets(-direction * speed, direction * speed);
    } else {
        cancel_motion();
    }
}

static void BehaviorTask(void *argument)
{
    /* Behavior produces wheel-RPM requests only; it never drives PWM directly.
     * SafetyTask and ControlTask remain active during scans and route steps. */
    uint32_t last_range_ms = 0U;
    (void)argument;
    for (;;) {
        CarState snapshot;
        App_GetState(&snapshot);
        if (snapshot.faults == FAULT_NONE) {
            switch (snapshot.mode) {
            case MODE_MANUAL: break;
            case MODE_TRACK: track_step(&snapshot); break;
            case MODE_AVOID: avoid_step(); break;
            case MODE_FUSION:
                if ((HAL_GetTick() - last_range_ms) > 100U) {
                    set_distance(ultrasonic_measure()); last_range_ms = HAL_GetTick();
                }
                App_GetState(&snapshot);
                if (snapshot.distance_cm < 25.0f) avoid_step(); else track_step(&snapshot);
                break;
            case MODE_CRUISE:
                if ((HAL_GetTick() - last_range_ms) > 100U) {
                    set_distance(ultrasonic_measure()); last_range_ms = HAL_GetTick();
                }
                App_GetState(&snapshot);
                { AppConfig cfg; config_get(&cfg);
                set_targets(snapshot.distance_cm < 30.0f ? 0.0f : cfg.base_speed_rpm,
                            snapshot.distance_cm < 30.0f ? 0.0f : cfg.base_speed_rpm); }
                break;
            case MODE_MOTION: motion_step(&snapshot); break;
            default: set_targets(0.0f, 0.0f); break;
            }
        }
        health_mark(HEALTH_BEHAVIOR);
        vTaskDelay(pdMS_TO_TICKS(50U));
    }
}

static void SensorTask(void *argument)
{
    /* IR is sampled at 50 Hz. Battery ADC is intentionally slower (5 Hz) to
     * reduce noise and avoid unnecessary blocking conversions. */
    TickType_t wake = xTaskGetTickCount();
    int8_t last_error = 0;
    uint8_t battery_divider = 0U;
    (void)argument;
    for (;;) {
        const uint8_t bits = BSP_IRRead();
        int16_t weighted = 0, count = 0;
        const int8_t weights[5] = {-4, -2, 0, 2, 4};
        for (uint8_t i = 0U; i < 5U; ++i) if ((bits & (1U << i)) != 0U) {
            weighted += weights[i]; ++count;
        }
        if (count > 0) last_error = (int8_t)(weighted / count);
        taskENTER_CRITICAL(); state.line_bits = bits; state.line_error = last_error; taskEXIT_CRITICAL();
        if (++battery_divider >= 10U) {
            AppConfig cfg;
            config_get(&cfg);
            const float voltage = BSP_BatteryRead(cfg.battery_divider_ratio);
            taskENTER_CRITICAL(); state.battery_v = voltage; taskEXIT_CRITICAL();
            battery_divider = 0U;
        }
        health_mark(HEALTH_SENSOR);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(20U));
    }
}

static const char *mode_name(CarMode mode)
{
    static const char *const names[] = {"MANUAL", "TRACK", "AVOID", "FUSION", "CRUISE", "MOTION"};
    return mode <= MODE_MOTION ? names[mode] : "UNKNOWN";
}

static void DisplayTask(void *argument)
{
    /* Alternate status and odometry pages every two seconds without adding a
     * framebuffer, which keeps C8T6 RAM use low. */
    TickType_t wake = xTaskGetTickCount();
    uint8_t page_counter = 0U;
    (void)argument;
    for (;;) {
        CarState s;
        char l0[22], l1[22], l2[22], l3[22];
        App_GetState(&s);
        if ((page_counter / 8U) % 2U == 0U) {
            (void)snprintf(l0, sizeof(l0), "MODE:%s", mode_name(s.mode));
            (void)snprintf(l1, sizeof(l1), "L:%3.0f R:%3.0f", (double)s.actual_left_rpm, (double)s.actual_right_rpm);
            (void)snprintf(l2, sizeof(l2), "D:%3.0fCM B:%.1fV", (double)s.distance_cm, (double)s.battery_v);
            (void)snprintf(l3, sizeof(l3), "IR:%02X F:%02lX", s.line_bits, (unsigned long)s.faults);
        } else {
            (void)snprintf(l0, sizeof(l0), "ODOMETRY");
            (void)snprintf(l1, sizeof(l1), "X:%5.1f Y:%5.1f", (double)s.odom_x_cm, (double)s.odom_y_cm);
            (void)snprintf(l2, sizeof(l2), "HEAD:%6.1f", (double)s.heading_deg);
            (void)snprintf(l3, sizeof(l3), "HEAP:%4lu", (unsigned long)xPortGetFreeHeapSize());
        }
        BSP_OledShowLines(l0, l1, l2, l3);
        ++page_counter;
        health_mark(HEALTH_DISPLAY);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(250U));
    }
}

static void MonitorTask(void *argument)
{
    /* The watchdog is refreshed only when every monitored task reported health
     * during the window. A stalled task therefore causes a hardware reset. */
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(1200U));
    for (;;) {
        uint32_t observed;
        taskENTER_CRITICAL();
        observed = health_bits;
        health_bits = 0U;
        if ((observed & HEALTH_ALL) != HEALTH_ALL) state.faults |= FAULT_TASK_STALL;
        taskEXIT_CRITICAL();
        if ((observed & HEALTH_ALL) == HEALTH_ALL) { BSP_WatchdogRefresh(); BSP_LedSet(true); }
        else { BSP_LedSet(false); fault_log_add(FAULT_TASK_STALL); }
        send_status();
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}
