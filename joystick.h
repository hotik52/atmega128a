#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>

#define JOYSTICK_CENTER        512
#define JOYSTICK_DEAD_ZONE     40
#define SERVO_MAX_STEP         5

void Joystick_Init(void);
void Joystick_UpdateServo(void);

#endif
