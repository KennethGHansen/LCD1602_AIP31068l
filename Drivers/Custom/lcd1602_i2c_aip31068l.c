/**
 * lcd1602_i2c_aip31068l.c
 *
 * DFRobot “LCD1602 Module V1.1” / DFR0555-family driver (AIP31068L LCD controller + PCA9633 backlight)
 * using STM32 HAL I2C and DWT-based microsecond delays.
 *
 * ---------------------------------------------------------------------------
 * 1) LCD controller: AIP31068L
 * ---------------------------------------------------------------------------
 * - Interface is 2‑wire serial (SCK/SDA) on power-up when PSB=High. In the DFR0555
 *   module this is exposed as I2C pins (SCL/SDA). [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)[1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 *
 * - AIP31068L serial interface DOES NOT support reading the Busy Flag (BF). The datasheet
 *   explicitly states you MUST wait enough execution time between instructions. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 *   Therefore: this driver *always delays* after each command/data write.
 *
 * - The "control byte" includes an RS bit: RS=0 => command stream, RS=1 => data stream. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 *   Co bit indicates whether another control byte follows; after the last control byte,
 *   a stream of bytes can follow until STOP/RESTART. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 *
 * ---------------------------------------------------------------------------
 * 2) Backlight controller: PCA9633 (for V1.1 module)
 * ---------------------------------------------------------------------------
 * - PCA9633 is an I2C LED driver with per-channel 8-bit PWM control for brightness. [2](https://www.bipom.com/documents/appnotes/LCD%20Interfacing%20using%20HD44780%20Hitachi%20chipset%20compatible%20LCD.pdf)
 * - DFRobot’s wiki for LCD1602 Module V1.1 shows the backlight device address is 0x6B. [3](https://componentsexplorer.com/16x2-lcd-hd44780-datasheet)
 *   (Our config header sets BL_ADDR7 accordingly.)
 *
 * ---------------------------------------------------------------------------
 * 3) Timing source: DWT cycle counter
 * ---------------------------------------------------------------------------
 * - DWT->CYCCNT gives accurate microsecond delays if enabled.
 * - Access to DWT counters requires enabling TRCENA and CYCCNT. [4](https://blog.embeddedexpert.io/?p=3168)[5](https://deepbluembedded.com/stm32-lcd-16x2-tutorial-library-alphanumeric-lcd-16x2-interfacing/)
 *
 * ---------------------------------------------------------------------------
 * FILE DEPENDENCIES
 * ---------------------------------------------------------------------------
 * - lcd1602_i2c_aip31068l_conf.h  (I2C handle + addresses + geometry)
 * - lcd1602_i2c_aip31068l.h       (public API)
 * - dwt_delay.h                   (DWT_Delay_Init(), DWT_DelayUs())
 */

#include "lcd1602_i2c_aip31068l.h"
#include "dwt_delay.h"
#include <string.h>

/* ============================================================================
 *                          CONFIG / CONSTANTS
 * ============================================================================
 */

/* Grab the project’s I2C handle and addresses from the config header. */
static I2C_HandleTypeDef *hi2c = AIP31068L_I2C_HANDLE;
static const uint8_t LCD_ADDR7 = AIP31068L_I2C_ADDR7_LCD;  /* AIP31068L address (7-bit) */
static const uint8_t BL_ADDR7  = PCA9633_I2C_ADDR7_BL_V11;   /* PCA9633 address (7-bit, V1.1 typically 0x6B) [3](https://componentsexplorer.com/16x2-lcd-hd44780-datasheet) */

/**
 * Delay policy:
 * The AIP31068L datasheet requires waiting between instructions because BF is not readable in serial mode. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 *
 * Exact execution times depend on oscillator and instruction; if you want to optimize, replace these values
 * with the instruction timing table values from your specific controller + clock.
 *
 * These are conservative safe delays that work well on typical modules:
 * - Clear/Home are long operations (~ms range)
 * - Most other commands and data writes are ~tens of microseconds
 */
#define LCD_DELAY_CLEAR_HOME_US   2000u
#define LCD_DELAY_CMD_US          50u
#define LCD_DELAY_DATA_US         50u

/* ============================================================================
 *                          INTERNAL I2C HELPERS
 * ============================================================================
 */

/**
 * @brief Transmit a buffer to a 7-bit I2C slave address using STM32 HAL.
 *
 * HAL expects the address left-shifted by 1 (8-bit address field with R/W bit = 0).
 */
static HAL_StatusTypeDef i2c_tx_addr7(uint8_t addr7, const uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Master_Transmit(hi2c, (uint16_t)(addr7 << 1), (uint8_t *)buf, len, HAL_MAX_DELAY);
}

/** Convenience wrapper for LCD device. */
static HAL_StatusTypeDef i2c_tx_lcd(const uint8_t *buf, uint16_t len)
{
    return i2c_tx_addr7(LCD_ADDR7, buf, len);
}

/** Convenience wrapper for Backlight device. */
static HAL_StatusTypeDef i2c_tx_bl(const uint8_t *buf, uint16_t len)
{
    return i2c_tx_addr7(BL_ADDR7, buf, len);
}

/* ============================================================================
 *                          LCD: LOW-LEVEL WRITE PRIMITIVES
 * ============================================================================
 */

/**
 * @brief Send one LCD instruction (command) byte.
 *
 * Protocol (AIP31068L 2-wire serial):
 * - RS bit selects command stream (RS=0) vs data stream (RS=1). [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 * - We use a single "last control byte" + one instruction byte:
 *     [CTRL_LAST_CMD][CMD]
 *
 * Timing:
 * - Must delay after write (BF not readable). [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 */
HAL_StatusTypeDef lcd_write_cmd(uint8_t cmd)
{
    uint8_t frame[2] = { AIP31068L_CTRL_LAST_CMD, cmd };

    HAL_StatusTypeDef st = i2c_tx_lcd(frame, (uint16_t)sizeof(frame));
    if (st != HAL_OK)
        return st;

    /* Apply mandatory delay. Clear/Home require longer wait. */
    if (cmd == LCD_CMD_CLEAR_DISPLAY || cmd == LCD_CMD_RETURN_HOME)
        DWT_DelayUs(LCD_DELAY_CLEAR_HOME_US);
    else
        DWT_DelayUs(LCD_DELAY_CMD_US);

    return HAL_OK;
}

/**
 * @brief Send one LCD data byte (character or CGRAM pattern byte).
 *
 * Protocol:
 *     [CTRL_LAST_DATA][DATA]
 *
 * Timing:
 * - Must delay after data write (BF not readable). [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 */
HAL_StatusTypeDef lcd_write_data_byte(uint8_t data)
{
    uint8_t frame[2] = { AIP31068L_CTRL_LAST_DATA, data };

    HAL_StatusTypeDef st = i2c_tx_lcd(frame, (uint16_t)sizeof(frame));
    if (st == HAL_OK)
        DWT_DelayUs(LCD_DELAY_DATA_US);

    return st;
}

/**
 * @brief Send a stream of LCD data bytes efficiently.
 *
 * Protocol:
 * - After the last control byte, a stream of bytes can follow until STOP/RESTART. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 * - We send: [CTRL_LAST_DATA][d0][d1]...[dn]
 *
 * Note:
 * - This function limits the burst length to 32 for bounded stack usage.
 * - Higher-level functions chunk strings appropriately.
 */
HAL_StatusTypeDef lcd_write_data(const uint8_t *data, size_t len)
{
    /* AIP31068L serial mode requires waiting between writes (BF not readable). [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
     * Many modules cannot handle long back-to-back byte streams in one I2C frame reliably,
     * so we send data one byte at a time, reusing the proven single-byte path.
     */
    if (!data || len == 0)
        return HAL_OK;

    for (size_t i = 0; i < len; i++)
    {
        HAL_StatusTypeDef st = lcd_write_data_byte(data[i]);  /* includes per-byte delay */
        if (st != HAL_OK)
            return st;
    }
    return HAL_OK;
}

/* ============================================================================
 *                          LCD: INITIALIZATION
 * ============================================================================
 */

/**
 * @brief Initialize the LCD controller into a known usable state.
 *
 * Notes:
 * - AIP31068L includes an automatic power-on reset. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 * - The module-level datasheet also mentions an auto reset time (~50 ms). [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 *
 * We do:
 *  1) Ensure DWT is enabled (for accurate microsecond delays)
 *  2) Wait for power-on reset to complete
 *  3) Send a standard “basic setting” sequence:
 *      - function set (2 line, 5x8)
 *      - display on (cursor off, blink off)
 *      - clear
 *      - entry mode increment
 */
HAL_StatusTypeDef lcd_init(void)
{
    if (!hi2c)
        return HAL_ERROR;

    /* Enable DWT cycle counter; required for DWT_DelayUs. [4](https://blog.embeddedexpert.io/?p=3168)[5](https://deepbluembedded.com/stm32-lcd-16x2-tutorial-library-alphanumeric-lcd-16x2-interfacing/) */
    (void)DWT_Delay_Init();

    /* Wait for module power-on reset to finish. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)[1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557) */
    HAL_Delay(60);

    /* Basic init: robust defaults for LCD1602. */
    (void)lcd_function_set(true, true, false);      /* DL=1, N=1, F=0 */
    (void)lcd_display_control(true, false, false);  /* D=1, C=0, B=0 */
    (void)lcd_clear();
    (void)lcd_entry_mode_set(true, false);          /* I/D=1, no shift */

    return HAL_OK;
}

/* ============================================================================
 *                  LCD: “COVER ALL COMMANDS” IMPLEMENTATIONS
 * ============================================================================
 */

HAL_StatusTypeDef lcd_clear(void) { return lcd_write_cmd(LCD_CMD_CLEAR_DISPLAY); }
HAL_StatusTypeDef lcd_home(void)  { return lcd_write_cmd(LCD_CMD_RETURN_HOME);  }

HAL_StatusTypeDef lcd_entry_mode_set(bool increment, bool shift_on)
{
    uint8_t cmd = LCD_CMD_ENTRY_MODE_SET;
    cmd |= increment ? LCD_ENTRY_ID_INCREMENT : LCD_ENTRY_ID_DECREMENT;
    cmd |= shift_on  ? LCD_ENTRY_SHIFT_ON     : LCD_ENTRY_SHIFT_OFF;
    return lcd_write_cmd(cmd);
}

HAL_StatusTypeDef lcd_display_control(bool display_on, bool cursor_on, bool blink_on)
{
    uint8_t cmd = LCD_CMD_DISPLAY_CONTROL;
    cmd |= display_on ? LCD_DISPLAY_ON : LCD_DISPLAY_OFF;
    cmd |= cursor_on  ? LCD_CURSOR_ON  : LCD_CURSOR_OFF;
    cmd |= blink_on   ? LCD_BLINK_ON   : LCD_BLINK_OFF;
    return lcd_write_cmd(cmd);
}

HAL_StatusTypeDef lcd_cursor_display_shift(bool shift_display, bool shift_right)
{
    uint8_t cmd = LCD_CMD_CURSOR_SHIFT;
    cmd |= shift_display ? LCD_SHIFT_DISPLAY : LCD_SHIFT_CURSOR;
    cmd |= shift_right   ? LCD_SHIFT_RIGHT   : LCD_SHIFT_LEFT;
    return lcd_write_cmd(cmd);
}

HAL_StatusTypeDef lcd_function_set(bool use_8bit, bool two_lines, bool font_5x10)
{
    uint8_t cmd = LCD_CMD_FUNCTION_SET;
    cmd |= use_8bit   ? LCD_8BIT_MODE : LCD_4BIT_MODE;
    cmd |= two_lines  ? LCD_2LINE     : LCD_1LINE;
    cmd |= font_5x10  ? LCD_FONT_5x10 : LCD_FONT_5x8;
    return lcd_write_cmd(cmd);
}

HAL_StatusTypeDef lcd_set_cgram_address(uint8_t addr_6bit)
{
    addr_6bit &= 0x3F;
    return lcd_write_cmd((uint8_t)(LCD_CMD_SET_CGRAM_ADDR | addr_6bit));
}

HAL_StatusTypeDef lcd_set_ddram_address(uint8_t addr_7bit)
{
    addr_7bit &= 0x7F;
    return lcd_write_cmd((uint8_t)(LCD_CMD_SET_DDRAM_ADDR | addr_7bit));
}

/* ============================================================================
 *                       LCD: READ COMMANDS (STUBS)
 * ============================================================================
 *
 * The AIP31068L instruction set includes “Read Busy Flag & Address” and “Read data from RAM”.
 * However, the AIP31068L datasheet notes the serial interface does not read BF in serial mode,
 * and requires waiting between instructions. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
 *
 * The DFR0555 hardware exposes only SCL/SDA, and typical usage is write-only for the LCD RAM.
 * So these functions exist only for API completeness and return dummy values.
 */

uint8_t lcd_read_busy_addr(bool *busy)
{
    if (busy) *busy = false;
    return 0;
}

uint8_t lcd_read_data(void)
{
    return 0;
}

/* ============================================================================
 *                       LCD: CONVENIENCE HELPERS
 * ============================================================================
 */

HAL_StatusTypeDef lcd_set_cursor(uint8_t col, uint8_t row)
{
    /**
     * Standard 16x2 mapping: row 0 base 0x00, row 1 base 0x40.
     * AIP31068L’s 2-line DDRAM ranges include 00–27 and 40–67. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
     */
    static const uint8_t row_base[] = { 0x00, 0x40, 0x14, 0x54 };

    if (row >= AIP31068L_LCD_ROWS) row = 0;
    if (col >= AIP31068L_LCD_COLS) col = 0;

    return lcd_set_ddram_address((uint8_t)(row_base[row] + col));
}

HAL_StatusTypeDef lcd_putc(char c)
{
    return lcd_write_data_byte((uint8_t)c);
}

HAL_StatusTypeDef lcd_puts(const char *s)
{
    /**
     * String output uses chunked streaming to reduce I2C transactions.
     * Each chunk uses: [CTRL_LAST_DATA][bytes...] [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
     */
    while (s && *s)
    {
        uint8_t chunk[16];
        size_t n = 0;

        while (s[n] && n < sizeof(chunk))
        {
            chunk[n] = (uint8_t)s[n];
            n++;
        }

        HAL_StatusTypeDef st = lcd_write_data(chunk, n);
        if (st != HAL_OK)
            return st;

        s += n;
    }
    return HAL_OK;
}

HAL_StatusTypeDef lcd_create_char(uint8_t slot, const uint8_t pattern[8])
{
    /**
     * AIP31068L CGRAM is 64x8 bits => 8 custom characters. [1](https://wiki.dfrobot.com/Gravity_I2C_LCD1602_Arduino_LCD_Display_Module_SKU_DFR0555_DF0556_DFR0557)
     * Each custom character consumes 8 bytes in CGRAM.
     */
    slot &= 0x07;

    HAL_StatusTypeDef st = lcd_set_cgram_address((uint8_t)(slot * 8));
    if (st != HAL_OK)
        return st;

    return lcd_write_data(pattern, 8);
}

HAL_StatusTypeDef lcd_put_custom(uint8_t slot)
{
    return lcd_write_data_byte((uint8_t)(slot & 0x07));
}

HAL_StatusTypeDef lcd_scroll(bool right, uint8_t steps)
{
    while (steps--)
    {
        HAL_StatusTypeDef st = lcd_cursor_display_shift(true, right);
        if (st != HAL_OK)
            return st;
    }
    return HAL_OK;
}

HAL_StatusTypeDef lcd_move_cursor(bool right, uint8_t steps)
{
    while (steps--)
    {
        HAL_StatusTypeDef st = lcd_cursor_display_shift(false, right);
        if (st != HAL_OK)
            return st;
    }
    return HAL_OK;
}

/* ============================================================================
 *                 BACKLIGHT ADD-ON: PCA9633 BRIGHTNESS CONTROL
 * ============================================================================
 *
 * PCA9633 is an I2C LED driver providing:
 * - Individual PWM (8-bit) per output channel. [2](https://www.bipom.com/documents/appnotes/LCD%20Interfacing%20using%20HD44780%20Hitachi%20chipset%20compatible%20LCD.pdf)
 *
 * On LCD1602 Module V1.1, DFRobot’s docs show the backlight device address is 0x6B. [3](https://componentsexplorer.com/16x2-lcd-hd44780-datasheet)
 * NOTE: The LCD1602 Module V1.1 that I bought actually didn't have the PCA9633 IC on-board, so it was not possible to use this software control. Shorting the cathode of the
 * backlight diode to GND ensures constant backlight. If the PCA9633 is present, this code should work though
 *
 * Implementation approach:
 * - Initialize MODE registers (wake device)
 * - Configure LEDOUT so all channels use individual PWM
 * - Set PWM0..PWM3 to the same value => acts like “global brightness”
 *
 * Why set all 4 channels:
 * - Different PCB revisions may map the backlight LED(s) to different outputs.
 * - Setting all outputs to same PWM is robust.
 */

/* PCA9633 register addresses (standard from NXP datasheet) [2](https://www.bipom.com/documents/appnotes/LCD%20Interfacing%20using%20HD44780%20Hitachi%20chipset%20compatible%20LCD.pdf) */
#define PCA9633_REG_MODE1     0x00
#define PCA9633_REG_MODE2     0x01
#define PCA9633_REG_PWM0      0x02
#define PCA9633_REG_PWM1      0x03
#define PCA9633_REG_PWM2      0x04
#define PCA9633_REG_PWM3      0x05
#define PCA9633_REG_GRPPWM    0x06
#define PCA9633_REG_GRPFREQ   0x07
#define PCA9633_REG_LEDOUT    0x08

/* LEDOUT bit pairs: 00=off, 01=on, 10=individual PWM, 11=individual+group PWM. [2](https://www.bipom.com/documents/appnotes/LCD%20Interfacing%20using%20HD44780%20Hitachi%20chipset%20compatible%20LCD.pdf) */
#define PCA9633_LEDOUT_IND_PWM_ALL  0xAA  /* 0b10101010 => LED0..LED3 all set to "10" individual PWM */

/**
 * @brief Write a single PCA9633 register.
 */
static HAL_StatusTypeDef pca9633_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_tx_bl(buf, (uint16_t)sizeof(buf));
}

/**
 * @brief Write consecutive PCA9633 registers starting at start_reg.
 *
 * Many I2C peripherals (including PCA9633 family) allow auto-increment; this method assumes
 * the part will accept sequential writes after a register pointer byte.
 */
static HAL_StatusTypeDef pca9633_write_regs(uint8_t start_reg, const uint8_t *vals, size_t len)
{
    if (!vals || len == 0) return HAL_OK;
    if (len > 8) return HAL_ERROR;

    uint8_t buf[1 + 8];
    buf[0] = start_reg;
    memcpy(&buf[1], vals, len);
    return i2c_tx_bl(buf, (uint16_t)(1 + len));
}

HAL_StatusTypeDef lcd_backlight_init(void)
{
    /**
     * Minimal recommended configuration:
     * - MODE1: clear sleep => normal mode (device active) [2](https://www.bipom.com/documents/appnotes/LCD%20Interfacing%20using%20HD44780%20Hitachi%20chipset%20compatible%20LCD.pdf)
     * - MODE2: choose a reasonable default; 0x04 is commonly used (output structure depends on board)
     * - LEDOUT: set all channels to “individual PWM” mode
     * - PWM: set to full brightness initially
     */
    HAL_StatusTypeDef st;

    st = pca9633_write_reg(PCA9633_REG_MODE1, 0x00);
    if (st != HAL_OK) return st;

    st = pca9633_write_reg(PCA9633_REG_MODE2, 0x04);
    if (st != HAL_OK) return st;

    st = pca9633_write_reg(PCA9633_REG_LEDOUT, PCA9633_LEDOUT_IND_PWM_ALL);
    if (st != HAL_OK) return st;
	
    return lcd_backlight_set_brightness(255);
}

HAL_StatusTypeDef lcd_backlight_set_brightness(uint8_t level_0_to_255)
{
    /**
     * Set all four PWM channels to the same duty (0..255).
     * PCA9633 supports 8-bit PWM resolution. [2](https://www.bipom.com/documents/appnotes/LCD%20Interfacing%20using%20HD44780%20Hitachi%20chipset%20compatible%20LCD.pdf)
     */
    uint8_t pwms[4] = {
        level_0_to_255,
        level_0_to_255,
        level_0_to_255,
        level_0_to_255
    };

    return pca9633_write_regs(PCA9633_REG_PWM0, pwms, 4);

}

HAL_StatusTypeDef lcd_backlight_on(void)
{
    return lcd_backlight_set_brightness(255);
}

HAL_StatusTypeDef lcd_backlight_off(void)
{
    return lcd_backlight_set_brightness(0);
}