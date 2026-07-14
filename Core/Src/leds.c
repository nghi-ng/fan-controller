#include "leds.h"

#include "main.h"

void Leds_SetFanState(FanState_t state)
{
    HAL_GPIO_WritePin(LED_OFF_GPIO_Port, LED_OFF_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_SPEED1_GPIO_Port, LED_SPEED1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_SPEED2_GPIO_Port, LED_SPEED2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_SPEED3_GPIO_Port, LED_SPEED3_Pin, GPIO_PIN_RESET);

    switch (state)
    {
        case FAN_STATE_OFF:
            HAL_GPIO_WritePin(LED_OFF_GPIO_Port, LED_OFF_Pin, GPIO_PIN_SET);
            break;

        case FAN_STATE_SPEED1:
            HAL_GPIO_WritePin(LED_SPEED1_GPIO_Port, LED_SPEED1_Pin, GPIO_PIN_SET);
            break;

        case FAN_STATE_SPEED2:
            HAL_GPIO_WritePin(LED_SPEED2_GPIO_Port, LED_SPEED2_Pin, GPIO_PIN_SET);
            break;

        case FAN_STATE_SPEED3:
            HAL_GPIO_WritePin(LED_SPEED3_GPIO_Port, LED_SPEED3_Pin, GPIO_PIN_SET);
            break;

        default:
            HAL_GPIO_WritePin(LED_OFF_GPIO_Port, LED_OFF_Pin, GPIO_PIN_SET);
            break;
    }
}
