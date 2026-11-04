
/*
Mohan Latchman 816044466
 */

#include <Arduino.h>
#include "q7.h"

extern "C" void serial_print(const char *text)
{
    Serial.print(text);
}

extern "C" int serial_receive_available(void)
{
    return Serial.available();
}

extern "C" int serial_receive_read(void)
{
    return Serial.read();
}

void setup(void)
{
    Serial.begin(115200);

    /* Wait for the serial connection to be ready. */
    while (!Serial)
    {
    }

    q7_init();
}

void loop(void)
{
    q7_run();
}
