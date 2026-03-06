#pragma once
#include "stm32f4xx_hal.h"

/**
 * lcd1602_i2c_aip31068l_conf.h
 *
 * Project configuration for the DFR0555 module:
 * - LCD controller: AIP31068L
 * - Interface: 2-wire serial (SCK/SDA) used at power on (AIP31068L PSB=High selects 2-wire). [1](https://deepbluembedded.com/stm32-delay-microsecond-millisecond-utility-dwt-delay-timer-delay/)
 *
 * DFR0555 module datasheet provides I2C slave addresses (8-bit form):
 *   - AIP31068L (LCD) slave address: 0x7C
 *   - PCA9633DP2 (backlight) slave address: 0xC0 [1](https://deepbluembedded.com/stm32-delay-microsecond-millisecond-utility-dwt-delay-timer-delay/)
 *
 * STM32 HAL typically expects 7-bit addresses (then shifts left internally).
 */
#define AIP31068L_I2C_ADDR7_LCD   (0x7C >> 1)  /* 0x3E [1](https://deepbluembedded.com/stm32-delay-microsecond-millisecond-utility-dwt-delay-timer-delay/) */
#define AIP31068L_I2C_ADDR7_BL    (0xC0 >> 1)  /* 0x60 [1](https://deepbluembedded.com/stm32-delay-microsecond-millisecond-utility-dwt-delay-timer-delay/) */
/**
 * Backlight controller address:
 * DFRobot wiki table for LCD1602 Module V1.1 shows RGBAddr = 0x6B. [2](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 * That value is typically used as a 7-bit address in Arduino libraries.
 */
#define PCA9633_I2C_ADDR7_BL_V11  (0x6B) 


/* LCD geometry (LCD1602) */
#define AIP31068L_LCD_COLS        16
#define AIP31068L_LCD_ROWS        2

/**
 * Choose which CubeMX-generated I2C handle to use.
 * Edit these two lines to match your project:
 *
 * Example: if you use I2C1 in CubeMX, you will have:
 *   extern I2C_HandleTypeDef hi2c1;
 *   and you set AIP31068L_I2C_HANDLE to &hi2c1.
 */
extern I2C_HandleTypeDef hi2c1;
#define AIP31068L_I2C_HANDLE      (&hi2c1)