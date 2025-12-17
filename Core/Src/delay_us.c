#include "delay_us.h"

static uint32_t cycles_per_us;

void delay_us_init(void)
{
    /* Włącz dostęp do DWT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Reset licznika cykli */
    DWT->CYCCNT = 0;

    /* Włącz licznik CYCCNT */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 100 MHz -> 100 cykli na 1 µs */
    cycles_per_us = SystemCoreClock / 1000000UL;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * cycles_per_us;

    while ((DWT->CYCCNT - start) < ticks)
    {
        __NOP();
    }
}
