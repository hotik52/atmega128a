#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>

#define UART_INPUT_MAX 5
#define UART_INPUT_TIMEOUT 500
#define FND_DIGIT_MASK 0x1E
#define MOTOR_PIN PB5

void uart0_init(void);
void fnd_init(void);
uint32_t fnd_millis_get(void);
uint8_t fnd_millis_elapsed(uint32_t *last_ms, uint16_t interval_ms);
uint8_t uart0_transmit_ready(void);
void uart0_transmit(char data);
uint8_t uart0_receive_ready(void);
uint8_t uart0_receive(void);
void fnd_display_digit(uint8_t digit, uint8_t number);
void motor_set_fan_request(uint8_t enabled);
void motor_update(uint16_t timer_seconds);
uint8_t timer_parse_input(char *buffer, uint8_t length, uint16_t *timer_seconds);

// UART 입력을 누적해서 유효한 시간이 완성되면 timer_seconds를 갱신합니다.
uint8_t timer_uart_update(uint16_t *timer_seconds);

// FND 4자리를 한 자리씩 빠르게 갱신하는 다이나믹 구동 함수입니다.
void timer_display_update(uint16_t timer_seconds);
