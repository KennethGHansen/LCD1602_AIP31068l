#include "dwt_delay.h"

/**
 * Some CMSIS versions provide DWT_CTRL_NOCYCCNT_Msk to indicate CYCCNT isn't implemented.
 * On Cortex-M4 (STM32F446RE), CYCCNT is normally available.
 */

bool DWT_Delay_Init(void)
{
    /* If CYCCNT is not implemented, DWT_CTRL.NOCYCCNT will be 1 (if present).  */
#ifdef DWT_CTRL_NOCYCCNT_Msk
    if (DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk)
    {
        return false;
    }
#endif

    /**
     * Enable trace/debug blocks globally.
     * ARM notes DWT counter registers are only accessible when DEMCR.TRCENA is set. [2](http://www.dinceraydin.com/lcd/commands.htm)
     */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Reset cycle counter */
    DWT->CYCCNT = 0;

    /* Enable cycle counter. CYCCNT increments each CPU cycle when enabled.  */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* Sanity check: ensure it increments */
    uint32_t before = DWT->CYCCNT;
    __NOP(); __NOP(); __NOP(); __NOP();
    uint32_t after  = DWT->CYCCNT;

    return (after != before);
}

void DWT_DelayCycles(uint32_t cycles)
{
    /**
     * CYCCNT is a free-running 32-bit counter; subtraction handles wrap-around naturally.
     * This is safe for typical short delays used in peripheral drivers.
     */
    uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles)
    {
        /* Busy-wait */
    }
}

void DWT_DelayUs(uint32_t us)
{
    /**
     * Convert microseconds to cycles:
     *   cycles = SystemCoreClock * us / 1,000,000
     *
     * Use 64-bit math to avoid overflow when us is large.
     */
    uint64_t cycles64 = ((uint64_t)SystemCoreClock * (uint64_t)us) / 1000000ULL;

    /* Cap to 32-bit for DelayCycles() (still extremely large in practice) */
    if (cycles64 > 0xFFFFFFFFULL)
        cycles64 = 0xFFFFFFFFULL;

    DWT_DelayCycles((uint32_t)cycles64);
}