#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

#define SERVO_0_DEG      125
#define SERVO_90_DEG     375
#define SERVO_180_DEG    625

#define SERVO_AUTO_STEP  1

void Servo_Init(void);
void Servo_UpdateAutoHorizontal(uint8_t isEnabled);
void Servo_UpdateAutoVertical(uint8_t isEnabled);
void Servo_MoveHorizontalUp(uint16_t step);
void Servo_MoveHorizontalDown(uint16_t step);
void Servo_MoveVerticalUp(uint16_t step);
void Servo_MoveVerticalDown(uint16_t step);
void Servo_Apply(void);

#endif
