#include "fan_control.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "leds.h"
#include "logger.h"

#define FAN_QUEUE_LENGTH       4U
#define FAN_TASK_STACK_WORDS   256U
#define FAN_TASK_PRIORITY      (tskIDLE_PRIORITY + 2U)

static TIM_HandleTypeDef *fan_pwm_timer;
static QueueHandle_t fan_state_queue;
static TaskHandle_t fan_task_handle;

static void FanControl_Task(void *argument);
static void FanControl_ApplyState(FanState_t state);
static void FanControl_SetPwmPercent(uint8_t percent);
static uint8_t FanControl_StateDutyPercent(FanState_t state);

bool FanControl_Init(TIM_HandleTypeDef *pwm_timer)
{
    fan_pwm_timer = pwm_timer;
    fan_state_queue = xQueueCreate(FAN_QUEUE_LENGTH, sizeof(FanState_t));

    if ((fan_pwm_timer == NULL) || (fan_state_queue == NULL))
    {
        return false;
    }

    BaseType_t created = xTaskCreate(FanControl_Task,
                                     "FanTask",
                                     FAN_TASK_STACK_WORDS,
                                     NULL,
                                     FAN_TASK_PRIORITY,
                                     &fan_task_handle);

    return created == pdPASS;
}

bool FanControl_RequestState(FanState_t state)
{
    if (fan_state_queue == NULL)
    {
        return false;
    }

    return xQueueSend(fan_state_queue, &state, 0U) == pdPASS;
}

const char *FanControl_StateName(FanState_t state)
{
    switch (state)
    {
        case FAN_STATE_OFF:
            return "OFF";

        case FAN_STATE_SPEED1:
            return "SPEED1";

        case FAN_STATE_SPEED2:
            return "SPEED2";

        case FAN_STATE_SPEED3:
            return "SPEED3";

        default:
            return "UNKNOWN";
    }
}

static void FanControl_Task(void *argument)
{
    (void)argument;

    FanState_t state = FAN_STATE_OFF;

    if (HAL_TIM_PWM_Start(fan_pwm_timer, TIM_CHANNEL_1) != HAL_OK)
    {
        Logger_Write("ERROR: TIM3 PWM start failed\r\n");
        vTaskDelete(NULL);
    }

    Logger_Write("FanTask started: TIM3 CH1 PWM enabled\r\n");
    FanControl_ApplyState(state);

    for (;;)
    {
        FanState_t requested_state;

        if (xQueueReceive(fan_state_queue, &requested_state, portMAX_DELAY) == pdPASS)
        {
            if (requested_state != state)
            {
                state = requested_state;
                FanControl_ApplyState(state);
            }
        }
    }
}

static void FanControl_ApplyState(FanState_t state)
{
    uint8_t duty_percent = FanControl_StateDutyPercent(state);

    FanControl_SetPwmPercent(duty_percent);
    Leds_SetFanState(state);

    Logger_Printf("fan_state,%s,pwm_percent,%u\r\n",
                  FanControl_StateName(state),
                  duty_percent);
}

static void FanControl_SetPwmPercent(uint8_t percent)
{
    if (percent > 100U)
    {
        percent = 100U;
    }

    uint32_t period = __HAL_TIM_GET_AUTORELOAD(fan_pwm_timer);
    uint32_t pulse = ((period + 1U) * percent) / 100U;

    if (pulse > period)
    {
        pulse = period;
    }

    __HAL_TIM_SET_COMPARE(fan_pwm_timer, TIM_CHANNEL_1, pulse);
}

static uint8_t FanControl_StateDutyPercent(FanState_t state)
{
    switch (state)
    {
        case FAN_STATE_OFF:
            return 0U;

        case FAN_STATE_SPEED1:
            return 40U;

        case FAN_STATE_SPEED2:
            return 70U;

        case FAN_STATE_SPEED3:
            return 100U;

        default:
            return 0U;
    }
}
