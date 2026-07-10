#include "app_config.h"
#include "main.h"

#include <stddef.h>
#include <string.h>

#if defined(STM32F103xE)
/* RCT6 high-density Flash pages are 2 KB; reserve the final page. */
#define CONFIG_PAGE_ADDRESS 0x0803F800UL
#define CONFIG_PAGE_SIZE    2048U
#else
/* C8T6 medium-density Flash pages are 1 KB; reserve 0x0800FC00..0x0800FFFF. */
#define CONFIG_PAGE_ADDRESS 0x0800FC00UL
#define CONFIG_PAGE_SIZE    1024U
#endif

static uint32_t crc32_calculate(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0U; i < length; ++i) {
        crc ^= bytes[i];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1U) ^ (0xEDB88320UL & (uint32_t)-(int32_t)(crc & 1U));
        }
    }
    return ~crc;
}

static bool in_range(float value, float minimum, float maximum)
{
    return value >= minimum && value <= maximum;
}

bool AppConfig_IsValid(const AppConfig *config)
{
    if (config->magic != APP_CONFIG_MAGIC ||
        config->version != APP_CONFIG_VERSION ||
        config->size != sizeof(AppConfig)) return false;
    if (config->crc32 != crc32_calculate(config, offsetof(AppConfig, crc32))) return false;
    return in_range(config->pid_kp, 0.0f, 20.0f) &&
           in_range(config->pid_ki, 0.0f, 20.0f) &&
           in_range(config->pid_kd, 0.0f, 5.0f) &&
           in_range(config->base_speed_rpm, 20.0f, 150.0f) &&
           in_range(config->encoder_counts_per_rev, 20.0f, 20000.0f) &&
           in_range(config->wheel_diameter_cm, 2.0f, 20.0f) &&
           in_range(config->track_width_cm, 5.0f, 50.0f) &&
           in_range(config->acceleration_rpm_s, 20.0f, 1000.0f) &&
           in_range(config->line_gain, 1.0f, 40.0f) &&
           in_range(config->battery_divider_ratio, 1.0f, 10.0f) &&
           in_range(config->battery_low_v, 2.5f, 20.0f) &&
           in_range(config->battery_recover_v, config->battery_low_v, 21.0f) &&
           in_range(config->stall_pwm_percent, 20.0f, 100.0f) &&
           in_range(config->stall_rpm, 0.5f, 30.0f) &&
           config->stall_time_ms >= 200U && config->stall_time_ms <= 5000U;
}

void AppConfig_Defaults(AppConfig *config)
{
    memset(config, 0, sizeof(*config));
    config->magic = APP_CONFIG_MAGIC;
    config->version = APP_CONFIG_VERSION;
    config->size = sizeof(AppConfig);
    config->pid_kp = 0.60f;
    config->pid_ki = 0.35f;
    config->pid_kd = 0.005f;
    config->base_speed_rpm = 80.0f;
    config->encoder_counts_per_rev = 780.0f;
    config->wheel_diameter_cm = 6.50f;
    config->track_width_cm = 14.50f;
    config->acceleration_rpm_s = 180.0f;
    config->line_gain = 14.0f;
    config->battery_divider_ratio = 4.0303f;
    config->battery_low_v = 6.60f;
    config->battery_recover_v = 6.90f;
    config->stall_pwm_percent = 75.0f;
    config->stall_rpm = 4.0f;
    config->stall_time_ms = 1000U;
}

bool AppConfig_Load(AppConfig *config)
{
    const uint32_t slots = CONFIG_PAGE_SIZE / sizeof(AppConfig);
    bool found = false;
    AppConfig best;
    /* Scan all append-only slots. Invalid/partially programmed records are ignored. */
    for (uint32_t i = 0U; i < slots; ++i) {
        AppConfig candidate;
        memcpy(&candidate, (const void *)(CONFIG_PAGE_ADDRESS + i * sizeof(AppConfig)),
               sizeof(candidate));
        if (AppConfig_IsValid(&candidate) && (!found || candidate.sequence > best.sequence)) {
            best = candidate;
            found = true;
        }
    }
    if (found) *config = best;
    else AppConfig_Defaults(config);
    return found;
}

bool AppConfig_Save(AppConfig *config)
{
    const uint32_t slots = CONFIG_PAGE_SIZE / sizeof(AppConfig);
    uint32_t target = slots;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status = HAL_OK;

    config->magic = APP_CONFIG_MAGIC;
    config->version = APP_CONFIG_VERSION;
    config->size = sizeof(AppConfig);
    ++config->sequence;
    config->crc32 = crc32_calculate(config, offsetof(AppConfig, crc32));
    if (!AppConfig_IsValid(config)) return false;

    /* A SAVE normally programs only the next empty slot. This reduces erase wear
     * from one erase per SAVE to one erase per full page. */
    for (uint32_t i = 0U; i < slots; ++i) {
        if (*(const uint32_t *)(CONFIG_PAGE_ADDRESS + i * sizeof(AppConfig)) == 0xFFFFFFFFUL) {
            target = i;
            break;
        }
    }

    if (HAL_FLASH_Unlock() != HAL_OK) return false;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    if (target == slots) {
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.PageAddress = CONFIG_PAGE_ADDRESS;
        erase.NbPages = 1U;
        status = HAL_FLASHEx_Erase(&erase, &page_error);
        target = 0U;
    }
    if (status == HAL_OK) {
        const uint32_t *words = (const uint32_t *)config;
        const uint32_t address = CONFIG_PAGE_ADDRESS + target * sizeof(AppConfig);
        for (uint32_t i = 0U; i < sizeof(AppConfig) / sizeof(uint32_t); ++i) {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i * 4U, words[i]);
            if (status != HAL_OK) break;
        }
    }
    (void)HAL_FLASH_Lock();
    if (status != HAL_OK) return false;

    AppConfig verify;
    memcpy(&verify, (const void *)(CONFIG_PAGE_ADDRESS + target * sizeof(AppConfig)), sizeof(verify));
    return AppConfig_IsValid(&verify) && verify.sequence == config->sequence;
}
