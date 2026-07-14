#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx_hal.h"

typedef enum
{
    FAN_STATE_OFF = 0,
    FAN_STATE_SPEED1,
    FAN_STATE_SPEED2,
    FAN_STATE_SPEED3
} FanState_t;

bool FanControl_Init(TIM_HandleTypeDef *pwm_timer);
bool FanControl_RequestState(FanState_t state);
const char *FanControl_StateName(FanState_t state);

#endif /* FAN_CONTROL_H */
