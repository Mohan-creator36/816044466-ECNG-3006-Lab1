
/*
  Mohan Latchman 816044466
 */

#include <Arduino.h>

constexpr uint16_t SAMPLE_COUNT = 1000;

volatile uint16_t DataBuf[SAMPLE_COUNT];
volatile uint32_t TimeBuf[SAMPLE_COUNT];

volatile uint16_t samplesCollected = 0;


// Wait until the ADC is ready for the next operation.
static void waitADC()
{
    while (ADC->STATUS.bit.SYNCBUSY) {
    }
}


// Start the ADC measurement and wait for it to finish.
static uint16_t readADC()
{
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;

    ADC->SWTRIG.bit.START = 1;
    waitADC();

    while (!ADC->INTFLAG.bit.RESRDY) {
    }

    return ADC->RESULT.reg;
}


// Set up the ADC to read A0 at 12-bit resolution without averaging.
static void configureADC()
{
    // Select A0 and set up the ADC using Arduino functions.
    analogReadResolution(12);
    analogReference(AR_DEFAULT);
    (void)analogRead(A0);

    ADC->CTRLA.bit.ENABLE = 0;
    waitADC();

    // Divide the 48 MHz clock by 64 to get 750 kHz.
    ADC->CTRLB.reg =
        ADC_CTRLB_PRESCALER_DIV64 |
        ADC_CTRLB_RESSEL_12BIT;

    waitADC();

    // Disable hardware averaging by taking one sample at a time.
    ADC->AVGCTRL.reg =
        ADC_AVGCTRL_SAMPLENUM_1 |
        ADC_AVGCTRL_ADJRES(0);

    ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(5);

    ADC->CTRLA.bit.ENABLE = 1;
    waitADC();

    // Ignore the first reading after setting up the ADC.
    (void)readADC();
}


// Take an ADC reading whenever the timer reaches 1 ms.
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


// Set up TC5 to generate 1000 interrupts per second.
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

    // Set the compare value to produce a 1000 Hz timer frequency.
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


// Stop the timer before processing and displaying the measurements.
static void stopTimer()
{
    NVIC_DisableIRQ(TC5_IRQn);

    TC5->COUNT16.INTENCLR.reg = TC_INTENCLR_MC0;
    TC5->COUNT16.CTRLA.bit.ENABLE = 0;

    while (TC5->COUNT16.STATUS.bit.SYNCBUSY) {
    }

    NVIC_ClearPendingIRQ(TC5_IRQn);
}


// Find the square root using integer calculations only.
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


// Print an integer value with three decimal places.
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


// Collect the ADC readings and calculate the required results.
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

    // Calculate the average and keep three decimal places.
    uint64_t meanMilli = sum * 1000 / SAMPLE_COUNT;

    // Use the population variance formula to measure the spread of readings.
    // variance = (N * sum(x*x) - sum(x)^2) / N^2.
    uint64_t numerator =
        SAMPLE_COUNT * sumSquares - sum * sum;

    // Multiply the variance by 1,000,000 to keep decimal precision.
    uint64_t varianceScaled =
        numerator * UINT64_C(1000000) /
        ((uint64_t)SAMPLE_COUNT * SAMPLE_COUNT);

    uint64_t noiseMilli = integerSqrt(varianceScaled);

    Serial1.println();
    Serial1.println("--- Q9 RESULTS ---");

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
    Serial1.println("Send r to collect another block.");
}


void setup()
{
    Serial1.begin(115200);

    // Hardware UART starts without waiting for a USB connection.

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
