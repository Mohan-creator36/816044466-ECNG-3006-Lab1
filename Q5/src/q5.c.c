#include <sam.h>
#include <stdint.h>
#include "q5.h"

#define SLOT_COUNT       4U
#define ENTRIES_PER_SLOT 5U
#define SLOT_TIME_MS     5000U
#define TIMER_COMPARE    749U

typedef struct
{
    uint32_t milliseconds;
} system_time_t;

typedef void (*task_function_t)(void);

static volatile system_time_t system_time = {0U};
static uint32_t slot_start_ms = 0U;

static void timer_wait_sync(void)
{
    while (TC5->COUNT16.STATUS.bit.SYNCBUSY != 0U)
    {
    }
}

static void timer_init(void)
{
    /* Turn on the clock for Timer 5. */
    PM->APBCMASK.reg |= PM_APBCMASK_TC5;

    /* Use the 48 MHz clock for TC4 and TC5. */
    GCLK->CLKCTRL.reg =
        GCLK_CLKCTRL_ID_TC4_TC5 |
        GCLK_CLKCTRL_GEN_GCLK0 |
        GCLK_CLKCTRL_CLKEN;

    while (GCLK->STATUS.bit.SYNCBUSY != 0U)
    {
    }

    /* Reset the timer before setting it up. */
    TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;

    while ((TC5->COUNT16.STATUS.bit.SYNCBUSY != 0U) ||
           (TC5->COUNT16.CTRLA.bit.SWRST != 0U))
    {
    }

    /*
     * Set the timer to 16-bit mode with a prescaler of 64.
     * The clock is divided from 48 MHz to 750 kHz.
     * This gives 750 counts in 1 ms, so the compare value is 749.
     */
    TC5->COUNT16.CTRLA.reg =
        TC_CTRLA_MODE_COUNT16 |
        TC_CTRLA_WAVEGEN_MFRQ |
        TC_CTRLA_PRESCALER_DIV64;

    timer_wait_sync();

    TC5->COUNT16.CC[0].reg = TIMER_COMPARE;
    timer_wait_sync();

    /* Clear the old interrupt flag and allow new timer interrupts. */
    TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;
    TC5->COUNT16.INTENSET.reg = TC_INTENSET_MC0;

    NVIC_DisableIRQ(TC5_IRQn);
    NVIC_ClearPendingIRQ(TC5_IRQn);
    NVIC_SetPriority(TC5_IRQn, 2U);
    NVIC_EnableIRQ(TC5_IRQn);

    /* Enable the timer so it begins counting. */
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
    timer_wait_sync();
}

void TC5_Handler(void)
{
    if ((TC5->COUNT16.INTFLAG.reg & TC_INTFLAG_MC0) != 0U)
    {
        /* Acknowledge the timer interrupt. */
        TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

        /* Add 1 ms to the system time. */
        system_time.milliseconds++;
    }
}

static uint32_t time_ms(void)
{
    uint32_t saved_mask;
    uint32_t result;

    /* Temporarily disable interrupts while reading the time. */
    saved_mask = __get_PRIMASK();
    __disable_irq();

    result = system_time.milliseconds;

    __set_PRIMASK(saved_mask);

    return result;
}

static void sleep_seconds(uint32_t seconds)
{
    const uint32_t start_ms = time_ms();
    const uint32_t duration_ms = seconds * 1000U;

    while ((uint32_t)(time_ms() - start_ms) < duration_ms)
    {
        /* Keep waiting until the required time has passed. */
    }
}

static void one(void)
{
    serial_print("task 1 running\r\n");
    sleep_seconds(1U);
}

static void two(void)
{
    serial_print("task 2 running\r\n");
    sleep_seconds(2U);
}

static void three(void)
{
    serial_print("task 3 running\r\n");
    sleep_seconds(3U);
}

static void four(void)
{
    serial_print("task 4 running\r\n");
    sleep_seconds(4U);
}

void five(void)
{
    serial_print("task 5 running\r\n");
    sleep_seconds(5U);
}

static void burn(void)
{
    const uint32_t burn_start_ms = time_ms();

    while ((uint32_t)(time_ms() - slot_start_ms) < SLOT_TIME_MS)
    {
        /* Wait until the current 5-second slot finishes. */
    }

    slot_start_ms = time_ms();

    serial_print("burn time = ");
    serial_print_number(
        (uint32_t)(slot_start_ms - burn_start_ms)
    );
    serial_print(" ms\r\n\r\n");
}

static const task_function_t task_table
    [SLOT_COUNT][ENTRIES_PER_SLOT] =
{
    {one,  two,   burn, burn, burn},
    {one,  three, burn, burn, burn},
    {one,  four,  burn, burn, burn},
    {burn, burn,  burn, burn, burn}
};

void q5_init(void)
{
    serial_print("\r\nQuestion 5: cyclic executive\r\n");
    serial_print("Timer tick = 1 ms\r\n");
    serial_print("Slot duration = 5000 ms\r\n\r\n");

    timer_init();
    slot_start_ms = time_ms();
}

void q5_run(void)
{
    uint32_t slot;
    uint32_t entry;

    for (slot = 0U; slot < SLOT_COUNT; slot++)
    {
        for (entry = 0U; entry < ENTRIES_PER_SLOT; entry++)
        {
            task_table[slot][entry]();

            if (task_table[slot][entry] == burn)
            {
                break;
            }
        }
    }
}