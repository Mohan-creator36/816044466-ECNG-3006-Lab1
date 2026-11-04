// Mohan Latchman 816044466


#include <Arduino.h>

constexpr uint16_t SAMPLE_COUNT = 1000;

volatile uint16_t DataBuf[SAMPLE_COUNT];
volatile uint32_t TimeBuf[SAMPLE_COUNT];

volatile uint16_t samplesCollected = 0;


// Wait until the ADC finishes synchronizing.
static void waitADC()
{
    while (ADC->STATUS.bit.SYNCBUSY) {
    }
}


// Begin an ADC conversion and wait until the reading is available.
static uint16_t readADC()
{
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;

    ADC->SWTRIG.bit.START = 1;
    waitADC();

    while (!ADC->INTFLAG.bit.RESRDY) {
    }

    return ADC->RESULT.reg;
}


// Set the ADC to measure A0 with 12-bit resolution and no averaging.
static void configureADC()
{
    // Initialize the ADC and select A0 using Arduino functions.
    analogReadResolution(12);
    analogReference(AR_DEFAULT);
    (void)analogRead(A0);

    ADC->CTRLA.bit.ENABLE = 0;
    waitADC();

    // Reduce the 48 MHz clock to 750 kHz using a division factor of 64.
    ADC->CTRLB.reg =
        ADC_CTRLB_PRESCALER_DIV64 |
        ADC_CTRLB_RESSEL_12BIT;

    waitADC();

    // Turn off hardware averaging so each result uses a single reading.
    ADC->AVGCTRL.reg =
        ADC_AVGCTRL_SAMPLENUM_1 |
        ADC_AVGCTRL_ADJRES(0);

    ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(5);

    ADC->CTRLA.bit.ENABLE = 1;
    waitADC();

    // Discard the first ADC result after initialization.
    (void)readADC();
}


// Record an ADC sample each time the 1 ms timer interrupt occurs.
extern "C" void TC5_Handler(void)
{
    if (TC5->COUNT16.INTFLAG.bit.MC0) {
        TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;

        uint16_t i = samplesCollected;

        if (i < SAMPLE_COUNT) {
            TimeBuf[i] = micros();
            DataBuf[i] = readADC();

            samplesCollected = i + 1;
        }
    }
}


// Initialize TC5 to produce interrupts at a frequency of 1000 Hz.
static void startTimer()
{
    PM->APBCMASK.reg |= PM_APBCMASK_TC5;

    GCLK->CLKCTRL.reg =
        GCLK_CLKCTRL_ID(GCM_TC4_TC5) |
        GCLK_CLKCTRL_GEN_GCLK0 |
        GCLK_CLKCTRL_CLKEN;

    while (GCLK->STATUS.bit.SYNCBUSY) {
    }

    NVIC_DisableIRQ(TC5_IRQn);

    TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY ||
           TC5->COUNT16.CTRLA.bit.SWRST) {
    }

    TC5->COUNT16.CTRLA.reg =
        TC_CTRLA_MODE_COUNT16 |
        TC_CTRLA_WAVEGEN_MFRQ |
        TC_CTRLA_PRESCALER_DIV64;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY) {
    }

    // A compare value of 749 gives 1000 Hz with a 48 MHz clock divided by 64.
    TC5->COUNT16.CC[0].reg = 749;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY) {
    }

    TC5->COUNT16.INTFLAG.reg = TC_INTFLAG_MC0;
    TC5->COUNT16.INTENSET.reg = TC_INTENSET_MC0;

    NVIC_ClearPendingIRQ(TC5_IRQn);
    NVIC_SetPriority(TC5_IRQn, 2);
    NVIC_EnableIRQ(TC5_IRQn);

    TC5->COUNT16.CTRLA.bit.ENABLE = 1;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY) {
    }
}


// Disable the timer before calculating and displaying the results.
static void stopTimer()
{
    NVIC_DisableIRQ(TC5_IRQn);

    TC5->COUNT16.INTENCLR.reg = TC_INTENCLR_MC0;
    TC5->COUNT16.CTRLA.bit.ENABLE = 0;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY) {
    }

    NVIC_ClearPendingIRQ(TC5_IRQn);
}


// Determine the square root using integer arithmetic.
static uint64_t integerSqrt(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = UINT64_C(1) << 62;

    while (bit > value) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }

        bit >>= 2;
    }

    return result;
}


// Output an integer-based value formatted to three decimal places.
static void printThreeDecimals(uint64_t scaledValue)
{
    Serial1.print((unsigned long)(scaledValue / 1000));
    Serial1.print('.');

    uint16_t fraction = scaledValue % 1000;

    if (fraction < 100) {
        Serial1.print('0');
    }

    if (fraction < 10) {
        Serial1.print('0');
    }

    Serial1.print(fraction);
}


// Gather ADC samples, process the data, and report the measurements.
static void collectAndReport()
{
    Serial1.println("Collecting 1000 samples...");
    Serial1.flush();

    samplesCollected = 0;
    startTimer();

    while (samplesCollected < SAMPLE_COUNT) {
    }

    stopTimer();

    uint64_t sum = 0;
    uint64_t sumSquares = 0;

    for (uint16_t i = 0; i < SAMPLE_COUNT; ++i) {
        uint32_t value = DataBuf[i];

        sum += value;
        sumSquares += (uint64_t)value * value;
    }

    // Compute the average reading while preserving three decimal places.
    uint64_t meanMilli = sum * 1000 / SAMPLE_COUNT;

    // Determine the population variance from the collected readings.
    // variance = (N * sum(x*x) - sum(x)^2) / N^2.
    uint64_t numerator =
        SAMPLE_COUNT * sumSquares - sum * sum;

    // Scale the variance by one million to maintain decimal accuracy.
    uint64_t varianceScaled =
        numerator * UINT64_C(1000000) /
        ((uint64_t)SAMPLE_COUNT * SAMPLE_COUNT);

    uint64_t noiseMilli = integerSqrt(varianceScaled);

    Serial1.println();
    Serial1.println("--- Q10 ADC RESULTS ---");

    Serial1.println("Hardware averaging: OFF (1 sample)");
    Serial1.println("Target sampling rate: 1000 Hz");

    Serial1.print("Samples collected: ");
    Serial1.println((unsigned int)samplesCollected);

    Serial1.print("Signal (mean): ");
    printThreeDecimals(meanMilli);
    Serial1.println(" ADC counts");

    Serial1.print("Noise (standard deviation): ");
    printThreeDecimals(noiseMilli);
    Serial1.println(" ADC counts");

    if (noiseMilli != 0) {
        Serial1.print("SNR (linear): ");

        uint64_t snrMilli = meanMilli * 1000 / noiseMilli;

        printThreeDecimals(snrMilli);
        Serial1.println();

        Serial1.println("Calculate ENOB = log(SNR) / log(2).");
    } else {
        Serial1.println("Noise unresolved in this dataset.");
        Serial1.println("SNR and ENOB cannot be calculated.");
    }

    Serial1.println("--- END RESULTS ---");

    // Q10: Find the time differences between successive ADC samples.
    uint32_t minimumInterval = UINT32_MAX;
    uint32_t maximumInterval = 0;

    for (uint16_t i = 1; i < SAMPLE_COUNT; ++i) {
        uint32_t interval = TimeBuf[i] - TimeBuf[i - 1];

        if (interval < minimumInterval) {
            minimumInterval = interval;
        }

        if (interval > maximumInterval) {
            maximumInterval = interval;
        }
    }

    // Find peak-to-peak jitter by subtracting the shortest interval from the longest.
    uint32_t jitter = maximumInterval - minimumInterval;

    Serial1.println();
    Serial1.println("--- Q10 TIMING RESULTS ---");

    Serial1.println("Target sampling interval: 1000 us");
    Serial1.println("Sampling intervals checked: 999");

    Serial1.print("Minimum sampling interval: ");
    Serial1.print((unsigned long)minimumInterval);
    Serial1.println(" us");

    Serial1.print("Maximum sampling interval: ");
    Serial1.print((unsigned long)maximumInterval);
    Serial1.println(" us");

    Serial1.print("Peak-to-peak sampling jitter: ");
    Serial1.print((unsigned long)jitter);
    Serial1.println(" us");

    Serial1.println("--- END Q10 RESULTS ---");
    
    Serial1.println("Send r through UART to collect another block.");
}


void setup()
{
    Serial1.begin(115200);

    // Initialize hardware UART without waiting for a USB connection.

    configureADC();

    delay(500);

    collectAndReport();
}


void loop()
{
    if (Serial1.available()) {
        char command = Serial1.read();

        if (command == 'r' || command == 'R') {
            collectAndReport();
        }
    }
}
