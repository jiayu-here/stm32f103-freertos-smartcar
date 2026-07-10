#include "bsp.h"

#include <ctype.h>
#include <string.h>

#define OLED_ADDRESS 0x3CU

static void delay_short(void)
{
    for (volatile uint32_t i = 0U; i < 10U; ++i) {
        __NOP();
    }
}

static void i2c_start(void)
{
    HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
    delay_short();
    HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_RESET);
    delay_short();
    HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET);
}

static void i2c_stop(void)
{
    HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
    delay_short();
    HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET);
    delay_short();
}

static void i2c_write(uint8_t value)
{
    for (uint8_t i = 0U; i < 8U; ++i) {
        HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN,
                          (value & 0x80U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
        delay_short();
        HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
        delay_short();
        HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET);
        value <<= 1U;
    }
    HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET);
    delay_short();
    HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET);
}

static void oled_packet(uint8_t control, const uint8_t *data, size_t length)
{
    i2c_start();
    i2c_write((uint8_t)(OLED_ADDRESS << 1U));
    i2c_write(control);
    for (size_t i = 0U; i < length; ++i) {
        i2c_write(data[i]);
    }
    i2c_stop();
}

static void oled_command(uint8_t command)
{
    oled_packet(0x00U, &command, 1U);
}

static void glyph(char c, uint8_t out[5])
{
    static const uint8_t digits[10][5] = {
        {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
        {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
        {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
        {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
        {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}
    };
    static const uint8_t letters[26][5] = {
        {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
        {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
        {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
        {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
        {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
        {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
        {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
        {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
        {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
        {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
        {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
    };
    memset(out, 0, 5U);
    c = (char)toupper((unsigned char)c);
    if (c >= '0' && c <= '9') memcpy(out, digits[c - '0'], 5U);
    else if (c >= 'A' && c <= 'Z') memcpy(out, letters[c - 'A'], 5U);
    else if (c == '-') { out[1] = out[2] = out[3] = 0x08U; }
    else if (c == '.') { out[2] = 0x60U; }
    else if (c == ':') { out[2] = 0x36U; }
    else if (c == '/') { out[0]=0x20U; out[1]=0x10U; out[2]=0x08U; out[3]=0x04U; out[4]=0x02U; }
    else if (c == '!') { out[2] = 0x5FU; }
}

static void show_line(uint8_t page, const char *text)
{
    uint8_t row[128] = {0};
    size_t column = 0U;

    while (*text != '\0' && column + 6U <= sizeof(row)) {
        glyph(*text++, &row[column]);
        column += 6U;
    }
    oled_command((uint8_t)(0xB0U | page));
    oled_command(0x00U);
    oled_command(0x10U);
    oled_packet(0x40U, row, sizeof(row));
}

void BSP_OledInit(void)
{
    static const uint8_t init_commands[] = {
        0xAE,0x20,0x02,0xB0,0xC8,0x00,0x10,0x40,0x81,0x7F,0xA1,
        0xA6,0xA8,0x3F,0xA4,0xD3,0x00,0xD5,0x80,0xD9,0xF1,0xDA,
        0x12,0xDB,0x40,0x8D,0x14,0xAF
    };
    for (size_t i = 0U; i < sizeof(init_commands); ++i) {
        oled_command(init_commands[i]);
    }
    for (uint8_t page = 0U; page < 8U; ++page) {
        show_line(page, "");
    }
}

void BSP_OledShowLines(const char *line0, const char *line1,
                       const char *line2, const char *line3)
{
    show_line(0U, line0);
    show_line(2U, line1);
    show_line(4U, line2);
    show_line(6U, line3);
}
