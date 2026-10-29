#include <Arduino.h>
#include <unity.h>

extern "C" {
    int add(int x, int y);
    bool posnum(int x);
}

void test_add()
{
    TEST_ASSERT_EQUAL_INT(150, add(100, 50));
}

void test_posnum()
{
    TEST_ASSERT_TRUE(posnum(5));
    TEST_ASSERT_FALSE(posnum(-5));
    TEST_ASSERT_FALSE(posnum(0));
}

void setup()
{
    delay(2000);

    UNITY_BEGIN();

    RUN_TEST(test_add);
    RUN_TEST(test_posnum);

    UNITY_END();
}

void loop()
{
}