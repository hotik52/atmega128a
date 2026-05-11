#define F_CPU 16000000UL

#include <stdint.h>
#include <util/delay.h>
#include "button.h"
#include "joystick.h"
#include "servo.h"
#include "lcd.h"
#include "fndtimer.h"

int main(void)
{
    BUTTON btnHorizontal;
    BUTTON btnVertical;
    uint8_t horizontalAuto = 0;
    uint8_t verticalAuto = 0;
    uint8_t temperature = 0;
    uint8_t humidity = 0;
    uint16_t fan_duty = 0;
    uint16_t target_duty = 0;
    uint16_t timer_seconds = 0;
    uint32_t sensor_last_ms = 0;
    uint32_t ramp_last_ms = 0;
    uint32_t second_last_ms = 0;
    uint32_t servo_last_ms = 0;

    ButtonInit(&btnHorizontal, &BUTTON_DDR, &BUTTON_PIN, BUTTON_ON);
    ButtonInit(&btnVertical, &BUTTON_DDR, &BUTTON_PIN, BUTTON_OFF);

    Servo_Init();
    Joystick_Init();
    fnd_init();
    uart0_init();

    I2C_Init();
    LCD_Init();
    FAN_init();
    FanLed_Init();
    DHT_SetInput();

    FAN_speed(0);
    motor_update(timer_seconds);
    timer_display_update(timer_seconds);
    FanLed_DisplayDuty(0);

    sei();

    LCD_Write_4bit(0x80, 0);
    LCD_WriteString("DHT Fan Control");
    LCD_Write_4bit(0xC0, 0);
    LCD_WriteString("Starting...");
    _delay_ms(1000);

    while (1)
    {
        if(timer_uart_update(&timer_seconds))
        {
            second_last_ms = fnd_millis_get();
            motor_update(timer_seconds);
            timer_display_update(timer_seconds);
        }

        if (fnd_millis_elapsed(&sensor_last_ms, SENSOR_READ_INTERVAL_MS))
        {
            if (DHT_Read(&temperature, &humidity))
            {
                target_duty = FAN_GetTargetDuty(temperature, humidity);
                LCD_PrintStatus(temperature, humidity, fan_duty, target_duty);
            }
            else
            {
                LCD_PrintDhtError();
            }
        }

        if (fnd_millis_elapsed(&ramp_last_ms, FAN_RAMP_INTERVAL_MS))
        {
            fan_duty = FAN_RampDuty(fan_duty, target_duty);
            FAN_speed(fan_duty);
            motor_update(timer_seconds);
            FanLed_DisplayDuty(FAN_getDutyPercent(fan_duty));
        }

        if (ButtonGetState(&btnHorizontal) == ACT_PUSH)
        {
            horizontalAuto = !horizontalAuto;
        }
        if (ButtonGetState(&btnVertical) == ACT_PUSH)
        {
            verticalAuto = !verticalAuto;
        }

        if(fnd_millis_elapsed(&servo_last_ms, 20))
        {
            if (!horizontalAuto && !verticalAuto)
            {
                Joystick_UpdateServo();
            }
            else
            {
                Servo_UpdateAutoHorizontal(horizontalAuto);
                Servo_UpdateAutoVertical(verticalAuto);
            }
        }

        if(fnd_millis_elapsed(&second_last_ms, 1000))
        {
            if(timer_seconds > 0) timer_seconds--;
            motor_update(timer_seconds);
            timer_display_update(timer_seconds);
        }
    }
}
