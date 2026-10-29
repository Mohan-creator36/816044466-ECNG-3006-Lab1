#ifndef PIO_UNIT_TESTING

#include <Arduino.h>

extern "C" {
    int add(int x, int y);
    bool posnum(int x);
}

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    Serial.print("posnum(5) = ");
    Serial.println(posnum(5));

    Serial.print("posnum(-5) = ");
    Serial.println(posnum(-5));

    Serial.print("posnum(0) = ");
    Serial.println(posnum(0));

    Serial.print("100 + 50 = ");
    Serial.println(add(100, 50));

    delay(2000);
}

#endif