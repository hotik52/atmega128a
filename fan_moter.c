#include "fan_moter.h"
#include "fndtimer.h"

static uint8_t motor_fan_request = 0; // 팬 제어 쪽에서 모터를 켜 달라고 요청했는지 저장하는 플래그입니다.

// PB5 모터 출력은 fndtimer.c가 최종 상태를 관리한다.
// LCD 쪽 팬 제어는 온습도 기반 ON/OFF 요청만 전달해서 UART 타이머 요청과 충돌하지 않게 한다.
void FAN_init(void)
{
    MOTOR_DDR |= (1 << MOTOR_PIN);
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

void motor_set_fan_request(uint8_t enabled)
{
    motor_fan_request = enabled ? 1 : 0; // 인자가 0이 아니면 팬 쪽 모터 요청을 켜고, 0이면 요청을 끕니다.
}

void motor_update(uint16_t timer_seconds, uint16_t current_fan_duty)
{
    // 1. 우선순위: UART 타이머가 작동 중인가?
    if (timer_seconds > 0)
    {
        // 타이머가 작동 중이면 무조건 최대 속도로 회전
        OCR1A = FAN_PWM_TOP; 
    }
    // 2. 타이머는 없지만, 온습도 센서에 의한 팬 가동 요청이 있는가?
    else if (motor_fan_request)
    {
        // 온습도 로직에서 계산된 듀티값(0~FAN_PWM_TOP)을 적용
        OCR1A = current_fan_duty;
    }
    // 3. 둘 다 해당 없으면 정지
    else
    {
        OCR1A = 0;
    }
}