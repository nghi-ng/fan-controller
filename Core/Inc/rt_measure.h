#ifndef RT_MEASURE_H
#define RT_MEASURE_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"

bool RtMeasure_Init(TIM_HandleTypeDef *timer);
void RtMeasure_TimerElapsedFromISR(TIM_HandleTypeDef *timer);

#endif /* RT_MEASURE_H */
