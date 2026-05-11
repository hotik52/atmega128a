#include "lcd.h"
#include "fndtimer.h"

// --- [1. I2C(TWI) 제어 함수] ---
void I2C_Init(void) {
    TWSR = 0x00; // 프리스케일러 1
    TWBR = ((F_CPU / 100000L) - 16) / 2; // 100kHz 설정
}

void I2C_Start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))); // 시작 신호 완료 대기
}

void I2C_Stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void I2C_Write(unsigned char data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))); // 데이터 전송 완료 대기
}

static void LCD_SetCursor(uint8_t row, uint8_t col)
{
    LCD_Write_4bit((row == 0 ? 0x80 : 0xC0) + col, 0);
}

// --- [2. I2C LCD 1602 제어 함수] ---
// 주소가 0x27인 경우 0x4E, 0x3F인 경우 0x7E를 사용하세요.
#define LCD_ADDR 0x4E 

void LCD_Write_4bit(unsigned char data, unsigned char rs) {
    unsigned char control = rs ? 0x01 : 0x00; // RS=1(Data), RS=0(Command)
    
    I2C_Start();
    I2C_Write(LCD_ADDR);
    
    // 상위 4비트 전송 (Enable Pulse 포함)
    I2C_Write((data & 0xF0) | control | 0x0C); // En=1, Backlight=1
    _delay_us(2);
    I2C_Write((data & 0xF0) | control | 0x08); // En=0, Backlight=1
    
    // 하위 4비트 전송
    I2C_Write(((data << 4) & 0xF0) | control | 0x0C);
    _delay_us(2);
    I2C_Write(((data << 4) & 0xF0) | control | 0x08);
    
    I2C_Stop();

    if (rs == 0 && (data == 0x01 || data == 0x02)) {
        _delay_us(1600);
    } else {
        _delay_us(50);
    }
}

void LCD_Init(void) {
    _delay_ms(50);
    LCD_Write_4bit(0x33, 0); // 초기화
    LCD_Write_4bit(0x32, 0); // 4비트 모드 설정
    LCD_Write_4bit(0x28, 0); // 2줄, 5x8 폰트
    LCD_Write_4bit(0x0C, 0); // Display ON, Cursor OFF
    LCD_Write_4bit(0x01, 0); // 화면 Clear
    _delay_ms(2);
}

void LCD_WriteString(const char *str)
{
    while(*str) LCD_Write_4bit(*str++, 1);
}


static const uint8_t fan_led_bar[FAN_LED_COUNT] = {
    0x80,
    0xC0,
    0xE0,
    0xF0,
    0xF8,
    0xFC,
    0xFE,
    0xFF
};

// PORTA에 연결된 LED bar를 출력으로 초기화한다.
// FAN_LED_PORT를 0으로 초기화해 부팅 직후에는 모든 LED가 꺼진 상태에서 시작한다.
void FanLed_Init(void)
{
    // main_adc.c의 array LED 방식과 동일하게 PORTA 전체를 LED bar 출력으로 사용한다.
    FAN_LED_DDR = 0xFF;
    FAN_LED_PORT = 0x00;
}

// 현재 팬 Duty(%)를 LED bar 점등 개수로 변환해 표시한다.
// fan_led_bar[]는 누적 점등 패턴이므로 led_count가 3이면 0xE0 패턴을 출력한다.
void FanLed_DisplayDuty(uint8_t duty_percent)
{
    uint8_t led_count = 0;

    // 팬이 실제로 돌지 않는 0~24% 구간은 LED도 꺼 둔다.
    // 25~100% 구간을 LED 1~8칸으로 매핑해 팬 파워를 표시한다.
    if (duty_percent >= FAN_START_DUTY_PERCENT) {
        led_count = 1 + ((uint16_t)(duty_percent - FAN_START_DUTY_PERCENT) * (FAN_LED_COUNT - 1))
                      / (100 - FAN_START_DUTY_PERCENT);
    }

    FAN_LED_PORT = (led_count == 0) ? 0x00 : fan_led_bar[led_count - 1];
}

// PB5 모터 출력은 fndtimer.c가 최종 상태를 관리한다.
// LCD 쪽 팬 제어는 온습도 기반 ON/OFF 요청만 전달해서 UART 타이머 요청과 충돌하지 않게 한다.
void FAN_init(void)
{
    DDRB |= (1 << PB5);
    motor_set_fan_request(0);
}

// 기존 PB5 UART 타이머 모터와 같은 출력을 쓰므로, 실제 PB5는 fndtimer.c에서 한 번만 갱신한다.
void FAN_speed(uint16_t duty_cycle)
{
    if (duty_cycle > FAN_PWM_TOP) {
        duty_cycle = FAN_PWM_TOP;
    }

    motor_set_fan_request(FAN_getDutyPercent(duty_cycle) >= FAN_START_DUTY_PERCENT);
}

// PWM 듀티값을 0~100%로 변환한다.
uint8_t FAN_getDutyPercent(uint16_t duty_cycle)
{
    return (uint32_t)duty_cycle * 100 / FAN_PWM_TOP;
}

// DHT DATA 선을 입력 모드로 전환한다.
// 시작 신호 이후 센서가 DATA 선을 구동하므로 MCU 핀은 반드시 입력으로 바뀌어야 한다.
void DHT_SetInput(void)
{
    // DHT 통신은 1-wire 형태라 MCU가 LOW 시작 신호를 보낸 뒤,
    // 같은 DATA 핀을 입력으로 바꿔 센서의 응답 펄스를 읽는다.
    DHT_DDR &= ~(1 << DHT_DATA);

    // 외부 풀업 저항이 없다면 내부 풀업으로 DATA 라인을 HIGH idle 상태에 둔다.
    // DHT 모듈에 이미 풀업이 있다면 이 설정은 보조 역할만 한다.
    DHT_PORT |= (1 << DHT_DATA); // DHT DATA 라인은 idle 상태에서 HIGH가 필요하다.
}

// DHT DATA 선을 출력 LOW로 만든다.
// DHT 읽기 시작 조건을 만들 때만 사용하며, 이 상태를 오래 유지하면 센서 통신이 막힌다.
static void DHT_SetOutputLow(void)
{
    // DHT 읽기를 시작하려면 MCU가 DATA 라인을 최소 18ms 동안 LOW로 잡아야 한다.
    DHT_DDR |= (1 << DHT_DATA);
    DHT_PORT &= ~(1 << DHT_DATA);
}

// DHT DATA 선이 원하는 상태가 될 때까지 기다린다.
// 반환값: 1 = 원하는 상태 감지 성공, 0 = timeout 발생
static uint8_t DHT_WaitForState(uint8_t state, uint16_t timeout_us)
{
    // DATA 핀이 원하는 상태(state: 0=LOW, 1=HIGH)가 될 때까지 1us 단위로 기다린다.
    // 센서가 응답하지 않거나 배선 문제가 있을 때 무한 대기하지 않도록 timeout을 둔다.
    while (((DHT_PIN & (1 << DHT_DATA)) ? 1 : 0) != state) {
        if (timeout_us == 0) {
            return 0;
        }

        timeout_us--;
        _delay_us(1);
    }

    return 1;
}

// DHT 센서에서 온도/습도를 1회 읽는다.
// 반환값: 1 = 읽기 성공 및 checksum 통과, 0 = 응답 없음/타이밍 오류/checksum 오류
uint8_t DHT_Read(uint8_t *temperature, uint8_t *humidity)
{
    // DHT11 기준 데이터 형식:
    // data[0] = 습도 정수부
    // data[1] = 습도 소수부
    // data[2] = 온도 정수부
    // data[3] = 온도 소수부
    // data[4] = checksum(data[0]~data[3] 합의 하위 8비트)
    uint8_t data[5] = { 0, };

    // 1. MCU 시작 신호: DATA를 출력 LOW로 만들고 18ms 유지한다.
    DHT_SetOutputLow();
    _delay_ms(18);

    // 2. MCU가 DATA를 입력으로 전환한다. 이후부터 센서가 라인을 구동한다.
    DHT_SetInput();
    _delay_us(30);

    // 3. 센서 응답 확인:
    //    센서가 LOW 약 80us -> HIGH 약 80us -> LOW 순서로 응답하면 통신 시작 가능.
    if (!DHT_WaitForState(0, 100)) return 0;
    if (!DHT_WaitForState(1, 100)) return 0;
    if (!DHT_WaitForState(0, 100)) return 0;

    // 4. 총 40비트 수신.
    //    각 비트는 LOW 구간 뒤 HIGH 구간 길이로 0/1을 구분한다.
    //    이 구현은 HIGH 시작 후 40us 지점에서 DATA가 HIGH이면 1, LOW이면 0으로 판정한다.
    for (uint8_t i = 0; i < 40; i++) {
        // 각 비트의 HIGH 구간이 시작될 때까지 기다린다.
        if (!DHT_WaitForState(1, 100)) return 0;

        // DHT11은 0이면 HIGH가 약 26~28us, 1이면 약 70us 유지된다.
        // 40us 뒤에도 HIGH라면 1로 판단한다.
        _delay_us(40);

        if (DHT_PIN & (1 << DHT_DATA)) {
            data[i / 8] |= (1 << (7 - (i % 8)));
        }

        // 다음 비트로 넘어가기 위해 현재 HIGH 구간이 끝나 LOW가 될 때까지 기다린다.
        if (!DHT_WaitForState(0, 100)) return 0;
    }

    // 5. 체크섬 검증. 합이 맞지 않으면 노이즈/타이밍 오류로 보고 실패 처리한다.
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) {
        return 0;
    }

    // 6. 현재 코드는 DHT11 정수부 값만 사용한다.
    *humidity = data[0];
    *temperature = data[2];

    return 1;
}

// value가 min_value~max_value 구간에서 어느 정도 위치인지 PWM 듀티로 변환한다.
// 예: 온도 28~40도를 PWM 0~FAN_PWM_TOP으로 선형 매핑한다.
static uint16_t FAN_MapRangeToDuty(uint8_t value, uint8_t min_value, uint8_t max_value)
{
    if (value < min_value) {
        return 0;
    }

    if (value >= max_value) {
        return FAN_PWM_TOP;
    }

    return (uint32_t)(value - min_value) * FAN_PWM_TOP / (max_value - min_value);
}

// 온도와 습도 값을 각각 팬 목표 듀티로 환산한 뒤, 더 큰 값을 최종 목표로 사용한다.
// 둘 중 하나라도 위험 구간에 가까우면 팬이 그 요구량에 맞춰 더 강하게 동작한다.
uint16_t FAN_GetTargetDuty(uint8_t temperature, uint8_t humidity)
{
    uint16_t temp_duty = FAN_MapRangeToDuty(temperature, TEMP_THRESHOLD, TEMP_MAX);
    uint16_t hum_duty = FAN_MapRangeToDuty(humidity, HUM_THRESHOLD, HUM_MAX);

    // 온도와 습도 중 더 강하게 냉각/환기를 요구하는 값을 팬 목표 파워로 사용한다.
    return (temp_duty > hum_duty) ? temp_duty : hum_duty;
}

// 실제 팬 듀티가 목표 듀티를 부드럽게 따라가도록 램프 처리한다.
// 목표값이 갑자기 바뀌어도 팬 속도가 한 번에 튀지 않고 FAN_RAMP_STEP 단위로 변한다.
uint16_t FAN_RampDuty(uint16_t current_duty, uint16_t target_duty)
{
    if (current_duty < target_duty) {
        uint16_t next_duty = current_duty + FAN_RAMP_STEP;
        return (next_duty > target_duty) ? target_duty : next_duty;
    }

    if (current_duty > target_duty) {
        if (current_duty > target_duty + FAN_RAMP_STEP) {
            return current_duty - FAN_RAMP_STEP;
        }

        return target_duty;
    }

    return current_duty;
}

// LCD 1행에는 온습도, 2행에는 팬 현재 파워와 목표 파워를 표시한다.
// Target은 온습도로 계산된 목표 팬 파워이고, Duty는 실제로 팬에 적용 중인 현재 파워다.
void LCD_PrintStatus(uint8_t temperature, uint8_t humidity, uint16_t fan_duty, uint16_t target_duty)
{
    char line[17];
    uint8_t duty_percent = FAN_getDutyPercent(fan_duty);
    uint8_t target_percent = FAN_getDutyPercent(target_duty);

    LCD_SetCursor(0, 0);
    snprintf(line, sizeof(line), "T:%3uC H:%3u%%    ", temperature, humidity);
    LCD_WriteString(line);

    LCD_SetCursor(1, 0);
    snprintf(line, sizeof(line), "F:%3u%% T:%3u%%   ", duty_percent, target_percent);
    LCD_WriteString(line);
}

// DHT 읽기에 실패했을 때 LCD에 오류 상태를 표시한다.
void LCD_PrintDhtError(void)
{
    LCD_SetCursor(0, 0);
    LCD_WriteString("DHT read failed  ");
    LCD_SetCursor(1, 0);
    LCD_WriteString("Check sensor     ");
}
