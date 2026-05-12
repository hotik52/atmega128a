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

void motor_update(uint16_t timer_seconds)
{
    if(timer_seconds > 0 || motor_fan_request) {
        MOTOR_PORT |= (1 << MOTOR_PIN); // 타이머가 남아 있거나 팬 요청이 있으면 모터 제어 핀을 HIGH로 켭니다.
    } else {
        MOTOR_PORT &= ~(1 << MOTOR_PIN); // 두 조건이 모두 없으면 모터 제어 핀을 LOW로 내려 끕니다.
    }
}
