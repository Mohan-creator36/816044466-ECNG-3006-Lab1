
/*
 * Name: Mohan Latchman
 * Student ID: 816044466
 * Question 7: Serial Debouncing FSM
 * Method: If-Else
 * Platform: Arduino MKR Zero
 */

#include <sam.h>
#include <stdint.h>
#include "q7.h"

#define LED_MASK       (1UL << 8)
#define VALID_CHAR     'a'
#define DEBOUNCE_MS    500U
#define TIMER_COMPARE  749U

typedef enum
{
    STATE_OFF = 0,
    STATE_ON = 1
} led_state_t;

typedef struct
{
    uint32_t milliseconds;
    uint16_t debounce_counter;
    uint8_t ready;
} system_time_t;

static volatile system_time_t system_time =
    {0U, 0U, 1U};

static led_state_t current_state = STATE_OFF;
static uint32_t last_processed_tick = 0U;

static void timer_wait_sync(void)
{
    while (TC5->COUNT16.STATUS.bit.SYNCBUSY != 0U)
    {
    }
}

static void timer_init(void)
{
    /* Enable the clock for Timer 5. */
    PM->APBCMASK.reg |= PM_APBCMASK_TC5;

    /* Use the 48 MHz clock for TC4 and TC5. */
    GCLK->CLKCTRL.reg =
        GCLK_CLKCTRL_ID_TC4_TC5 |
        GCLK_CLKCTRL_GEN_GCLK0 |
        GCLK_CLKCTRL_CLKEN;

    while (GCLK->STATUS.bit.SYNCBUSY != 0U)
    {
    }

    /* Reset the timer before configuring it. */
    TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;

    while ((TC5->COUNT16.STATUS.bit.SYNCBUSY != 0U) ||
           (TC5->COUNT16.CTRLA.bit.SWRST != 0U))
    {
    }

    /*
     * Configure the timer in 16-bit mode.
     * 48 MHz divided by 64 gives 750 kHz.
     * A compare value of 749 gives a 1 ms tick.
     */
    TC5->COUNT16.CTRLA.reg =
        TC_CTRLA_MODE_COUNT16 |
        TC_CTRLA_WAVEGEN_MFRQ |
        TC_CTRLA_PRESCALER_DIV64;

    timer_wait_sync();

    TC5->COUNT16.CC[0].reg = TIMER_COMPARE;
    timer_wait_sync();

    /* Clear previous flags and enable interrupts. */
    TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;
    TC5->COUNT16.INTENSET.reg = TC_INTENSET_MC0;

    NVIC_DisableIRQ(TC5_IRQn);
    NVIC_ClearPendingIRQ(TC5_IRQn);
    NVIC_SetPriority(TC5_IRQn, 2U);
    NVIC_EnableIRQ(TC5_IRQn);

    /* Start the hardware timer. */
    TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
    timer_wait_sync();
}

void TC5_Handler(void)
{
    if ((TC5->COUNT16.INTFLAG.reg &
         TC_INTFLAG_MC0) != 0U)
    {
        /* Clear the timer interrupt flag. */
        TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

        /* Increase the system time every millisecond. */
        system_time.milliseconds++;

        /* Track the time since the last valid input. */
        if (system_time.ready == 0U)
        {
            system_time.debounce_counter++;

            if (system_time.debounce_counter >=
                DEBOUNCE_MS)
            {
                system_time.debounce_counter = 0U;
                system_time.ready = 1U;
            }
        }
    }
}

static void process_character(int received)
{
    uint32_t saved_mask;
    uint8_t accepted;

    /* Only the lowercase character 'a' is valid. */
    if (received == VALID_CHAR)
    {
        /*
         * Check whether the debounce period is over.
         * Interrupts are disabled briefly to protect
         * the shared timer variables.
         */
        saved_mask = __get_PRIMASK();
        __disable_irq();

        accepted = system_time.ready;

        if (accepted != 0U)
        {
            system_time.ready = 0U;
            system_time.debounce_counter = 0U;
        }

        __set_PRIMASK(saved_mask);

        if (accepted != 0U)
        {
            /* Use if-else to control the FSM states. */
            if (current_state == STATE_OFF)
            {
                current_state = STATE_ON;

                PORT->Group[1].OUTSET.reg = LED_MASK;

                serial_print("Valid a: LED ON\r\n");
            }
            else
            {
                current_state = STATE_OFF;

                PORT->Group[1].OUTCLR.reg = LED_MASK;

                serial_print("Valid a: LED OFF\r\n");
            }
        }
        else
        {
            /* Ignore a repeated input within 500 ms. */
            serial_print(
                "Ignored a: within 500 ms\r\n"
            );
        }
    }
    else
    {
        /* Ignore characters other than 'a'. */
        serial_print("Invalid character\r\n");
    }
}

void q7_init(void)
{
    /*
     * The MKR Zero built-in LED uses PB08.
     * Configure Port B pin 8 as an output.
     */
    PORT->Group[1].DIRSET.reg = LED_MASK;
    PORT->Group[1].OUTCLR.reg = LED_MASK;

    current_state = STATE_OFF;

    timer_init();

    serial_print("\r\nQuestion 7: Debouncing FSM\r\n");
    serial_print("Method: If-Else\r\n");
    serial_print("Timer tick: 1 ms\r\n");
    serial_print("Debounce period: 500 ms\r\n");
    serial_print("Valid character: a\r\n");
    serial_print("Built-in LED: OFF\r\n\r\n");
}

void q7_run(void)
{
    uint32_t current_tick = system_time.milliseconds;

    /*
     * Process serial inputs only when a new
     * system timer tick has occurred.
     */
    if (current_tick != last_processed_tick)
    {
        last_processed_tick = current_tick;

        while (serial_receive_available() > 0)
        {
            int received = serial_receive_read();

            process_character(received);
        }
    }
}
