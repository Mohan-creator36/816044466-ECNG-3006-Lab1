#include <Arduino.h>
#include "q5.h"

extern "C" void serial_print(const char *text)
{
    Serial.print(text);
}

extern "C" void serial_print_number(uint32_t value)
{
    Serial.print(static_cast<unsigned long>(value));
}

void setup(void)
{
    Serial.begin(115200);

    /* Wait for the serial connection to be established. */
    while (!Serial)
    {
    }

    q5_init();
}

void loop(void)
{
    q5_run();
}