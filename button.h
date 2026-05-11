#include <avr/io.h>

#define LED_DDR     DDRD
#define LED_PORT    PORTD
#define BUTTON_DDR   DDRG
#define BUTTON_PIN   PING

#define BUTTON_ON 2
#define BUTTON_OFF 3
#define BUTTON_TOGGLE 4
#define BUTTON_DEBOUNCE_COUNT 30

enum {PUSHED, RELEASED};
enum
{
    NO_ACT,
    ACT_PUSH,       // = 3으로 선언하면 밑에는 1이 아니라 4로 선언됨
    ACT_RELEASE
};

typedef struct
{
    volatile uint8_t    *ddr;       // 버튼이 연결된 포트의 주소를 저장하는 포인터
    volatile uint8_t    *pin;       // 버튼이 연결된 핀 번호를 저장하는 변수
    uint8_t             btnPin;     // 버튼 핀 번호를 저장하는 변수
    uint8_t             prevState;  // 이전 버튼 상태를 저장하는 변수
    uint8_t             debounceCount;
} BUTTON;

void ButtonInit(BUTTON *button, volatile uint8_t *ddr, volatile uint8_t *pin, uint8_t pinNum);
uint8_t ButtonGetState(BUTTON *button);
