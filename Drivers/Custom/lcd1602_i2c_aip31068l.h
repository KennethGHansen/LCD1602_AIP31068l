#pragma once
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "lcd1602_i2c_aip31068l_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * lcd1602_i2c_aip31068l.h
 *
 * Driver for DFR0555 (Gravity I2C LCD1602) using AIP31068L controller.
 *
 * Protocol notes from AIP31068L datasheet:
 * - RS bit defines whether following bytes are COMMAND (RS=0) or DATA (RS=1). [1](https://deepbluembedded.com/stm32-delay-microsecond-millisecond-utility-dwt-delay-timer-delay/)
 * - Serial interface does NOT read Busy Flag (BF), so firmware must wait enough execution time
 *   between instructions. [1](https://deepbluembedded.com/stm32-delay-microsecond-millisecond-utility-dwt-delay-timer-delay/)
 *
 * Control byte format (common for these controllers):
 *   bit7 = Co (continuation: 1 = another control byte follows, 0 = last control byte)
 *   bit6 = RS (0 = command stream, 1 = data stream)
 *   bit5..0 = 0
 *
 * Thus:
 *   Co=0, RS=0 => 0x00  (last control byte for commands)
 *   Co=0, RS=1 => 0x40  (last control byte for data)
 *   Co=1, RS=0 => 0x80  (more control bytes follow, command)
 *   Co=1, RS=1 => 0xC0  (more control bytes follow, data)
 */
#define AIP31068L_CTRL_LAST_CMD   0x00
#define AIP31068L_CTRL_LAST_DATA  0x40
#define AIP31068L_CTRL_MORE_CMD   0x80
#define AIP31068L_CTRL_MORE_DATA  0xC0

/* Instruction set (AIP31068L instruction table is HD44780-style). [1](https://deepbluembedded.com/stm32-delay-microsecond-millisecond-utility-dwt-delay-timer-delay/) */
#define LCD_CMD_CLEAR_DISPLAY     0x01
#define LCD_CMD_RETURN_HOME       0x02
#define LCD_CMD_ENTRY_MODE_SET    0x04
#define LCD_CMD_DISPLAY_CONTROL   0x08
#define LCD_CMD_CURSOR_SHIFT      0x10
#define LCD_CMD_FUNCTION_SET      0x20
#define LCD_CMD_SET_CGRAM_ADDR    0x40
#define LCD_CMD_SET_DDRAM_ADDR    0x80

/* Bit masks for composing commands */
#define LCD_ENTRY_ID_INCREMENT    0x02
#define LCD_ENTRY_ID_DECREMENT    0x00
#define LCD_ENTRY_SHIFT_ON        0x01
#define LCD_ENTRY_SHIFT_OFF       0x00

#define LCD_DISPLAY_ON            0x04
#define LCD_DISPLAY_OFF           0x00
#define LCD_CURSOR_ON             0x02
#define LCD_CURSOR_OFF            0x00
#define LCD_BLINK_ON              0x01
#define LCD_BLINK_OFF             0x00

#define LCD_SHIFT_DISPLAY         0x08
#define LCD_SHIFT_CURSOR          0x00
#define LCD_SHIFT_RIGHT           0x04
#define LCD_SHIFT_LEFT            0x00

#define LCD_8BIT_MODE             0x10
#define LCD_4BIT_MODE             0x00
#define LCD_2LINE                 0x08
#define LCD_1LINE                 0x00
#define LCD_FONT_5x10             0x04
#define LCD_FONT_5x8              0x00

/* ------------------------------ Core API ------------------------------ */

/**
 * @brief Initialize the LCD.
 * Must be called once after HAL + clocks + I2C init.
 */
HAL_StatusTypeDef lcd_init(void);

/* Low-level primitives */
HAL_StatusTypeDef lcd_write_cmd(uint8_t cmd);
HAL_StatusTypeDef lcd_write_data_byte(uint8_t data);
HAL_StatusTypeDef lcd_write_data(const uint8_t *data, size_t len);

/* ------------------------- Cover-all-commands API ------------------------- */
HAL_StatusTypeDef lcd_clear(void);
HAL_StatusTypeDef lcd_home(void);

HAL_StatusTypeDef lcd_entry_mode_set(bool increment, bool shift_on);
HAL_StatusTypeDef lcd_display_control(bool display_on, bool cursor_on, bool blink_on);
HAL_StatusTypeDef lcd_cursor_display_shift(bool shift_display, bool shift_right);
HAL_StatusTypeDef lcd_function_set(bool use_8bit, bool two_lines, bool font_5x10);

HAL_StatusTypeDef lcd_set_cgram_address(uint8_t addr_6bit);
HAL_StatusTypeDef lcd_set_ddram_address(uint8_t addr_7bit);

/**
 * Read operations exist in the instruction set, but:
 * - AIP31068L serial interface does not read BF in serial mode; you must delay. [1](https://deepbluembedded.com/stm32-delay-microsecond-millisecond-utility-dwt-delay-timer-delay/)
 * - DFR0555 exposes only SDA/SCL; this driver is write-focused.
 *
 * These are therefore provided as stubs for API completeness.
 */
uint8_t lcd_read_busy_addr(bool *busy);
uint8_t lcd_read_data(void);

/* ----------------------------- Convenience ----------------------------- */
HAL_StatusTypeDef lcd_set_cursor(uint8_t col, uint8_t row);
HAL_StatusTypeDef lcd_putc(char c);
HAL_StatusTypeDef lcd_puts(const char *s);

HAL_StatusTypeDef lcd_create_char(uint8_t slot, const uint8_t pattern[8]);
HAL_StatusTypeDef lcd_put_custom(uint8_t slot);

HAL_StatusTypeDef lcd_scroll(bool right, uint8_t steps);
HAL_StatusTypeDef lcd_move_cursor(bool right, uint8_t steps);


/* ---------------- Backlight add-on API (PCA9633) ----------------
 * PCA9633 is an I2C LED driver with 8-bit PWM per output (0..255 brightness). [1](https://www.nxp.com/docs/en/data-sheet/PCA9633.pdf)
 */
HAL_StatusTypeDef lcd_backlight_init(void);
HAL_StatusTypeDef lcd_backlight_set_brightness(uint8_t level_0_to_255);
HAL_StatusTypeDef lcd_backlight_on(void);
HAL_StatusTypeDef lcd_backlight_off(void);


#ifdef __cplusplus
}
#endif