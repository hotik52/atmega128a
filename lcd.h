#ifndef LCD_H
#define LCD_H

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>

// DHT 센서 DATA 핀 설정.
// DHT는 하나의 DATA 선을 MCU와 센서가 번갈아 구동하므로 DDR/PORT/PIN 레지스터를 모두 사용한다.
#define DHT_DDR DDRB      // DHT_DATA 핀의 입출력 방향 설정
#define DHT_PORT PORTB    // DHT_DATA 핀 출력값 또는 내부 풀업 설정
#define DHT_PIN PINB      // DHT_DATA 핀의 실제 입력 상태 읽기
#define DHT_DATA PB0      // 센서 DATA 선이 연결된 핀

// 팬 파워 표시용 array LED 설정.
// main_adc.c에서 사용하던 방식처럼 PORTA 전체를 LED bar 출력으로 사용한다.
#define FAN_LED_DDR DDRA
#define FAN_LED_PORT PORTA
#define FAN_LED_COUNT 8

// 팬 제어 매핑 기준.
// 임계값 미만이면 팬 목표값은 0이고, 임계값~MAX 구간은 0~100%로 선형 매핑한다.
#define FAN_PWM_TOP 639
#define FAN_START_DUTY_PERCENT 25
#define TEMP_THRESHOLD 28 // 28도부터 온도 기준 팬 파워 증가 시작
#define HUM_THRESHOLD 50  // 습도 50%부터 습도 기준 팬 파워 증가 시작
#define TEMP_MAX 30       // 40도 이상이면 온도 기준 팬 목표 100%
#define HUM_MAX 90        // 습도 90% 이상이면 습도 기준 팬 목표 100%

// 주기 제어 설정.
// DHT는 너무 자주 읽으면 불안정할 수 있어 2초마다 읽고, 팬 PWM은 100ms마다 목표값을 따라간다.
#define SENSOR_READ_INTERVAL_MS 2000
#define FAN_RAMP_INTERVAL_MS 100
#define FAN_RAMP_STEP 16

// --- [1. I2C(TWI) 제어 함수] ---

void I2C_Init(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_Write(unsigned char data);

// --- [2. I2C LCD 1602 제어 함수] ---

void LCD_Write_4bit(unsigned char data, unsigned char rs);
void LCD_Init(void);
void LCD_WriteString(const char *str);
void FanLed_Init(void);
void FanLed_DisplayDuty(uint8_t duty_percent);
void FAN_init(void);
void FAN_speed(uint16_t duty);
uint8_t FAN_getDutyPercent(uint16_t duty_cycle);
void DHT_SetInput(void);
uint8_t DHT_Read(uint8_t *temperature, uint8_t *humidity);
uint16_t FAN_GetTargetDuty(uint8_t temperature, uint8_t humidity);
uint16_t FAN_RampDuty(uint16_t current_duty, uint16_t target_duty);
void LCD_PrintStatus(uint8_t temperature, uint8_t humidity, uint16_t fan_duty, uint16_t target_duty);
void LCD_PrintDhtError(void);

#endif
