#include "fndtimer.h"

#include <util/atomic.h>

static const uint8_t fndNumber[] =
{   0x3F,
    0x06,
    0x5B,
    0x4F,
    0x66,
    0x6D,
    0x7D,
    0x07,
    0x7F,
    0x6F
};

static const uint8_t fnd_sel[] =
{   (uint8_t)~(1 << PB1), 
    (uint8_t)~(1 << PB2),
    (uint8_t)~(1 << PB3),
    (uint8_t)~(1 << PB4)
};

static uint8_t motor_fan_request = 0;
static volatile uint8_t fnd_display_digits[4] = { 0, 0, 0, 0 };
static volatile uint8_t fnd_scan_digit = 0;
static volatile uint32_t fnd_millis = 0;

#define FND_TIMER2_TICK_MS 4

static void fnd_timer2_init(void)
{
    TCCR2 = (1 << WGM21) | (1 << CS22);
    OCR2 = 249;
    TIMSK |= (1 << OCIE2);
}

void fnd_init(void)
{
    DDRC = 0xFF; // 숫자 패턴 출력
    DDRB |= FND_DIGIT_MASK | (1 << MOTOR_PIN); // 자릿수 선택 + 모터 출력
    PORTB = (PORTB | FND_DIGIT_MASK) & ~(1 << MOTOR_PIN); // FND OFF, 모터 OFF로 시작
    fnd_timer2_init();
}

ISR(TIMER2_COMP_vect)
{
    uint8_t portb_keep = PORTB & ~FND_DIGIT_MASK;

    fnd_millis += FND_TIMER2_TICK_MS;

    PORTB = portb_keep | FND_DIGIT_MASK;
    PORTC = fndNumber[fnd_display_digits[fnd_scan_digit]];
    PORTB = portb_keep | (fnd_sel[fnd_scan_digit] & FND_DIGIT_MASK);

    fnd_scan_digit++;
    if(fnd_scan_digit >= 4) {
        fnd_scan_digit = 0;
    }
}

uint32_t fnd_millis_get(void)
{
    uint32_t millis;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        millis = fnd_millis;
    }

    return millis;
}

uint8_t fnd_millis_elapsed(uint32_t *last_ms, uint16_t interval_ms)
{
    uint32_t now = fnd_millis_get();

    if((uint32_t)(now - *last_ms) >= interval_ms) {
        *last_ms = now;
        return 1;
    }

    return 0;
}

void uart0_init(void)
{
    UCSR0A |= (1 << U2X0);                      // 2배속 모드
    UCSR0B |= (1 << RXEN0) | (1 << TXEN0);      // 송수신 활성화
    UCSR0C |= (1 << UCSZ00) | (1 << UCSZ01);    // 8비트 모드, 패리티비트 없음, 스톱비트 1비트
    UBRR0H = 0;                                 // 9600bps
    UBRR0L = 207;
}

uint8_t uart0_transmit_ready(void)
{
    return (UCSR0A & (1 << UDRE0));
}

void uart0_transmit(char data)
{
    while (!(UCSR0A & (1 << UDRE0))); // 송신 버퍼가 비어질 때까지 대기
    UDR0 = data; // 데이터 전송
}

uint8_t uart0_receive_ready(void)
{
    return (UCSR0A & (1 << RXC0));
}

uint8_t uart0_receive(void)
{
    return UDR0;
}

void fnd_display_digit(uint8_t digit, uint8_t number)
{
    uint8_t portb_keep = PORTB & ~FND_DIGIT_MASK;

    PORTB = portb_keep | FND_DIGIT_MASK;                  // 모든 자리 OFF
    PORTC = fndNumber[number];   // 세그먼트 데이터 먼저 설정
    PORTB = portb_keep | (fnd_sel[digit] & FND_DIGIT_MASK); // 해당 자리 ON
}

void motor_set_fan_request(uint8_t enabled)
{
    motor_fan_request = enabled ? 1 : 0;
}

void motor_update(uint16_t timer_seconds)
{
    if(timer_seconds > 0 || motor_fan_request) {
        PORTB |= (1 << MOTOR_PIN);
    } else {
        PORTB &= ~(1 << MOTOR_PIN);
    }
}

uint8_t timer_parse_input(char *buffer, uint8_t length, uint16_t *timer_seconds)
{
    uint16_t minutes = 0;
    uint8_t seconds = 0;
    uint8_t colon = 0;
    uint8_t minute_digits = 0;
    uint8_t second_digits = 0;

    // 입력 형식: "분" 또는 "분:초"입니다. 예) 5, 10, 1:30, 99:59
    for(uint8_t i = 0; i < length; i++) {
        char data = buffer[i];

        if(data >= '0' && data <= '9') {
            if(colon) {
                seconds = seconds * 10 + (data - '0');
                second_digits++;
                if(seconds >= 60 || second_digits > 2) return 0;
            } else {
                minutes = minutes * 10 + (data - '0');
                minute_digits++;
                if(minutes > 99 || minute_digits > 2) return 0;
            }
        } else if(data == ':' && colon == 0) {
            colon = 1;
        } else {
            return 0;
        }
    }

    if(minute_digits == 0 && !colon) return 0;
    if(colon && second_digits == 0) return 0;

    *timer_seconds = (minutes * 60) + seconds;
    return 1;
}

uint8_t timer_uart_update(uint16_t *timer_seconds)
{
    // 한 글자씩 들어오는 UART 값을 여러 루프에 걸쳐 보관합니다.
    static char uart_input[UART_INPUT_MAX];
    static uint8_t uart_input_len = 0;
    static uint8_t uart_input_wait = 0;
    uint8_t updated = 0;

    while(uart0_receive_ready()) {
        char rx_data = uart0_receive();

        uart0_transmit(rx_data); // 입력 확인용 에코

        // Enter가 들어오면 지금까지 받은 문자열을 시간으로 해석합니다.
        if(rx_data == '\r' || rx_data == '\n') {
            if(uart_input_len > 0) {
                if(timer_parse_input(uart_input, uart_input_len, timer_seconds)) {
                    updated = 1;
                }

                uart_input_len = 0;
            }

            uart_input_wait = 0;
        } else if((rx_data >= '0' && rx_data <= '9') || rx_data == ':') {
            if(uart_input_len < UART_INPUT_MAX) {
                uart_input[uart_input_len++] = rx_data;
                uart_input_wait = 0;
            } else {
                uart_input_len = 0;
                uart_input_wait = 0;
            }
        } else {
            uart_input_len = 0;
            uart_input_wait = 0;
        }
    }

    // Enter 없이 입력이 멈춰도 일정 시간이 지나면 현재 입력값을 적용합니다.
    if(uart_input_len > 0) {
        uart_input_wait++;

        if(uart_input_wait >= UART_INPUT_TIMEOUT) {
            if(timer_parse_input(uart_input, uart_input_len, timer_seconds)) {
                updated = 1;
            }

            uart_input_len = 0;
            uart_input_wait = 0;
        }
    }

    return updated;
}

void timer_display_update(uint16_t timer_seconds)
{
    uint8_t min = timer_seconds / 60;
    uint8_t sec = timer_seconds % 60;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        fnd_display_digits[0] = min / 10;
        fnd_display_digits[1] = min % 10;
        fnd_display_digits[2] = sec / 10;
        fnd_display_digits[3] = sec % 10;
    }
}
