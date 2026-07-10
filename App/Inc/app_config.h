#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define APP_CONFIG_MAGIC   0x53434152UL
#define APP_CONFIG_VERSION 2U

typedef struct {
    /* Record header. sequence selects the newest valid append-only record. */
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t sequence;
    float pid_kp;
    float pid_ki;
    float pid_kd;
    float base_speed_rpm;
    float encoder_counts_per_rev;
    float wheel_diameter_cm;
    float track_width_cm;
    float acceleration_rpm_s;
    float line_gain;
    float battery_divider_ratio;
    float battery_low_v;
    float battery_recover_v;
    float stall_pwm_percent;
    float stall_rpm;
    uint32_t stall_time_ms;
    /* CRC covers every byte before this field, including the record header. */
    uint32_t crc32;
} AppConfig;

/* Fill safe defaults without writing Flash. */
void AppConfig_Defaults(AppConfig *config);
/* Load the newest valid record; returns false when defaults were used. */
bool AppConfig_Load(AppConfig *config);
/* Append a record and erase the reserved page only when it becomes full. */
bool AppConfig_Save(AppConfig *config);
/* Check header, CRC, and every calibration range. */
bool AppConfig_IsValid(const AppConfig *config);

#endif
