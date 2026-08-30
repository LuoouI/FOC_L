#include "My_LED.h"
#include "My_ADC/My_ADC.h"

static LED_State_t MyLED_LedState = LED_OFF;
static uint32 MyLED_BlinkElapsedMs = 0u;
static uint8 MyLED_LedLevel = 0u;

/***********************************************
 * @brief : 写入保护指示灯 GPIO 电平
 * @param : Level 指示灯输出电平
 * @return: void
 * @date  : 2026-08-15
 * @author: L
 ************************************************/
static void My_LED_WriteLed(uint8 Level)
{
    MyLED_LedLevel = (Level != 0u) ? 1u : 0u;
    gpio_set_level(LED_PIN, (MyLED_LedLevel == 0u) ? GPIO_HIGH : GPIO_LOW);
}

void My_LED_Init(void)
{
    gpio_init(LED_PIN, GPO, GPIO_HIGH, GPO_PUSH_PULL);

    MyLED_LedState = LED_OFF;
    MyLED_BlinkElapsedMs = 0u;
    My_LED_WriteLed(0u);
}

void My_LED_SetLedState(LED_State_t LedState)
{
    if ((LedState != LED_ON) && (LedState != LED_BLINK) && (LedState != LED_OFF))
    {
        LedState = LED_OFF;
    }

    MyLED_LedState = LedState;
    MyLED_BlinkElapsedMs = 0u;

    if (LedState == LED_ON)
    {
        My_LED_WriteLed(1u);
    }
    else if (LedState == LED_OFF)
    {
        My_LED_WriteLed(0u);
    }
    else
    {
        My_LED_WriteLed(1u);
    }
}

void My_LED_Service(uint32 ElapsedMs)
{
    if (MyLED_LedState != LED_BLINK)
    {
        return;
    }

    MyLED_BlinkElapsedMs += ElapsedMs;
    while (MyLED_BlinkElapsedMs >= LED_BLINK_PERIOD_MS)
    {
        MyLED_BlinkElapsedMs -= LED_BLINK_PERIOD_MS;
        My_LED_WriteLed((MyLED_LedLevel == 0u) ? 1u : 0u);
    }
}

LED_State_t My_LED_GetLedState(void)
{
    return MyLED_LedState;
}

void My_LED_CheckVoltage(void)
{
    float Voltage = My_ADC_GetBatteryVoltage();
    LED_State_t LedState;

    if (Voltage < VOLTAGE_LED_OFF_VALUE)
    {
        LedState = LED_OFF;
    }
    else if (Voltage < VOLTAGE_LED_BLINK_VALUE)
    {
        LedState = LED_BLINK;
    }
    else
    {
        LedState = LED_ON;
    }

    if (LedState != MyLED_LedState)
    {
        My_LED_SetLedState(LedState);
    }
}
