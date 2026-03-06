#pragma once
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * dwt_delay.h
 *
 * Microsecond delay implementation using the ARM Cortex-M DWT cycle counter (CYCCNT).
 *
 * Why this exists:
 *   - The AIP31068L serial interface does NOT read the Busy Flag (BF), so the host
 *     must wait long enough execution time between instructions. [1](https://deepbluembedded.com/stm32-delay-microsecond-millisecond-utility-dwt-delay-timer-delay/)
 *   - Using DWT->CYCCNT gives accurate sub-millisecond delays without timer setup.
 *
 * Key hardware facts:
 *   - Access to DWT counters requires TRCENA (DEMCR) to be set. [2](http://www.dinceraydin.com/lcd/commands.htm)
 *   - CYCCNT increments each processor clock cycle when enabled. 
 *   - Typical enable sequence on Cortex-M: set TRCENA, reset CYCCNT, enable CYCCNTENA. [3](https://evakw.com/en/products/a1474)
 *
 * Notes / caveats:
 *   - Must call DWT_Delay_Init() once AFTER SystemClock_Config() so SystemCoreClock is correct.
 *   - DWT is not available on Cortex-M0/M0+. STM32F446RE is Cortex-M4F (has DWT).
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize DWT CYCCNT cycle counter.
 *
 * Call once during startup after clocks are configured.
 *
 * @return true if CYCCNT appears to be running, false if not available or not running.
 */
bool DWT_Delay_Init(void);

/**
 * @brief Delay for a number of CPU cycles (busy-wait).
 *
 * @param cycles Number of cycles to wait (wrap-safe for typical short delays).
 */
void DWT_DelayCycles(uint32_t cycles);

/**
 * @brief Delay for N microseconds using DWT->CYCCNT.
 *
 * @param us Microseconds to delay.
 */
void DWT_DelayUs(uint32_t us);

#ifdef __cplusplus
}
#endif