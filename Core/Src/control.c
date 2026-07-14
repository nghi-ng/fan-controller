#include "control.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "fan_control.h"
#include "logger.h"
#include "main.h"

#define CONTROL_QUEUE_LENGTH       8U
#define CONTROL_TASK_STACK_WORDS   256U
#define CONTROL_TASK_PRIORITY      (tskIDLE_PRIORITY + 3U)
#define CONTROL_BUTTON_SAMPLE_MS   5U
#define CONTROL_BUTTON_DEBOUNCE_MS 30U
#define CONTROL_BUTTON_ACTIVE      GPIO_PIN_SET
#define CONTROL_PIR_ACTIVE         GPIO_PIN_SET
#define CONTROL_PIR_EMPTY_MS       10000U
#define CONTROL_AUTO_OFF_MAX_CC    2600
#define CONTROL_AUTO_SPEED1_MAX_CC 2700
#define CONTROL_AUTO_SPEED2_MAX_CC 2850

typedef struct
{
    bool initialized;
    GPIO_PinState last_raw_state;
    GPIO_PinState stable_state;
    TickType_t last_change_tick;
} ControlButtonDebounce_t;

typedef struct
{
    bool initialized;
    bool motion_active;
    bool occupied;
    TickType_t last_motion_tick;
} ControlPirState_t;

static QueueHandle_t control_event_queue;
static TaskHandle_t control_task_handle;
static volatile int32_t latest_temp_centi_c;
static volatile bool latest_temp_valid;
static volatile ControlMode_t current_mode = CONTROL_MODE_MANUAL;
static volatile FanState_t current_fan_state = FAN_STATE_OFF;
static volatile bool current_pir_motion_active;
static volatile bool current_pir_occupied;

static void Control_Task(void *argument);
static bool Control_ButtonPressed(ControlButtonDebounce_t *button);
static bool Control_UpdatePir(ControlPirState_t *pir);
static void Control_ToggleMode(ControlMode_t *mode, FanState_t *state, bool occupied);
static FanState_t Control_ApplyEvent(FanState_t state, ControlEvent_t event);
static void Control_ApplyAutoState(FanState_t *state, bool occupied);
static FanState_t Control_StateForAuto(bool occupied);
static FanState_t Control_StateFromTemperature(int32_t temp_centi_c);
static const char *Control_OccupancyName(bool occupied);

bool Control_Init(void)
{
    control_event_queue = xQueueCreate(CONTROL_QUEUE_LENGTH, sizeof(ControlEvent_t));

    if (control_event_queue == NULL)
    {
        return false;
    }

    BaseType_t created = xTaskCreate(Control_Task,
                                     "ControlTask",
                                     CONTROL_TASK_STACK_WORDS,
                                     NULL,
                                     CONTROL_TASK_PRIORITY,
                                     &control_task_handle);

    return created == pdPASS;
}

bool Control_PostEvent(ControlEvent_t event)
{
    if (control_event_queue == NULL)
    {
        return false;
    }

    return xQueueSend(control_event_queue, &event, 0U) == pdPASS;
}

void Control_PostEventFromISR(ControlEvent_t event)
{
    if (control_event_queue == NULL)
    {
        return;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;

    (void)xQueueSendFromISR(control_event_queue, &event, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void Control_UpdateTemperatureCentiC(int32_t temp_centi_c)
{
    latest_temp_centi_c = temp_centi_c;
    latest_temp_valid = true;

    (void)Control_PostEvent(CONTROL_EVENT_TEMPERATURE_UPDATED);
}

void Control_GetStatus(ControlStatus_t *status)
{
    if (status == NULL)
    {
        return;
    }

    status->mode = current_mode;
    status->fan_state = current_fan_state;
    status->temp_centi_c = latest_temp_centi_c;
    status->temp_valid = latest_temp_valid;
    status->pir_motion_active = current_pir_motion_active;
    status->pir_occupied = current_pir_occupied;
}

static void Control_Task(void *argument)
{
    (void)argument;

    FanState_t state = FAN_STATE_OFF;
    ControlMode_t mode = CONTROL_MODE_MANUAL;
    ControlButtonDebounce_t button = {0};
    ControlPirState_t pir = {0};

    Logger_Write("ControlTask started: manual encoder mode\r\n");
    Logger_Printf("mode,%s\r\n", Control_ModeName(mode));
    current_mode = mode;
    current_fan_state = state;
    (void)FanControl_RequestState(state);

    for (;;)
    {
        ControlEvent_t event;

        if (Control_UpdatePir(&pir) && (mode == CONTROL_MODE_AUTO))
        {
            Control_ApplyAutoState(&state, pir.occupied);
        }

        if (xQueueReceive(control_event_queue, &event, pdMS_TO_TICKS(CONTROL_BUTTON_SAMPLE_MS)) == pdPASS)
        {
            if (event == CONTROL_EVENT_TOGGLE_MODE)
            {
                Control_ToggleMode(&mode, &state, pir.occupied);
                continue;
            }

            if (event == CONTROL_EVENT_TEMPERATURE_UPDATED)
            {
                if (mode == CONTROL_MODE_AUTO)
                {
                    Control_ApplyAutoState(&state, pir.occupied);
                }

                continue;
            }

            if (mode != CONTROL_MODE_MANUAL)
            {
                Logger_Write("control_event_ignored,auto_mode\r\n");
                continue;
            }

            FanState_t next_state = Control_ApplyEvent(state, event);

            if (next_state != state)
            {
                state = next_state;
                current_fan_state = state;
                (void)FanControl_RequestState(state);
                Logger_Printf("control_state,%s\r\n", FanControl_StateName(state));
            }
        }

        if (Control_ButtonPressed(&button))
        {
            Control_ToggleMode(&mode, &state, pir.occupied);
        }
    }
}

static bool Control_ButtonPressed(ControlButtonDebounce_t *button)
{
    if (button == NULL)
    {
        return false;
    }

    GPIO_PinState raw_state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
    TickType_t now = xTaskGetTickCount();

    if (!button->initialized)
    {
        button->initialized = true;
        button->last_raw_state = raw_state;
        button->stable_state = raw_state;
        button->last_change_tick = now;
        return false;
    }

    if (raw_state != button->last_raw_state)
    {
        button->last_raw_state = raw_state;
        button->last_change_tick = now;
        return false;
    }

    if ((raw_state != button->stable_state) &&
        ((now - button->last_change_tick) >= pdMS_TO_TICKS(CONTROL_BUTTON_DEBOUNCE_MS)))
    {
        button->stable_state = raw_state;
        return raw_state == CONTROL_BUTTON_ACTIVE;
    }

    return false;
}

static bool Control_UpdatePir(ControlPirState_t *pir)
{
    if (pir == NULL)
    {
        return false;
    }

    TickType_t now = xTaskGetTickCount();
    bool motion_active = HAL_GPIO_ReadPin(PIR_GPIO_Port, PIR_Pin) == CONTROL_PIR_ACTIVE;
    bool occupancy_changed = false;

    if (!pir->initialized)
    {
        pir->initialized = true;
        pir->motion_active = motion_active;
        pir->occupied = motion_active;
        pir->last_motion_tick = now;

        current_pir_motion_active = pir->motion_active;
        current_pir_occupied = pir->occupied;

        Logger_Printf("pir_motion,%s\r\n", pir->motion_active ? "ACTIVE" : "INACTIVE");
        Logger_Printf("occupancy,%s\r\n", Control_OccupancyName(pir->occupied));
        return true;
    }

    if (motion_active != pir->motion_active)
    {
        pir->motion_active = motion_active;
        current_pir_motion_active = pir->motion_active;
        Logger_Printf("pir_motion,%s\r\n", pir->motion_active ? "ACTIVE" : "INACTIVE");
    }

    if (motion_active)
    {
        pir->last_motion_tick = now;

        if (!pir->occupied)
        {
            pir->occupied = true;
            occupancy_changed = true;
        }
    }
    else if (pir->occupied &&
             ((now - pir->last_motion_tick) >= pdMS_TO_TICKS(CONTROL_PIR_EMPTY_MS)))
    {
        pir->occupied = false;
        occupancy_changed = true;
    }

    current_pir_occupied = pir->occupied;

    if (occupancy_changed)
    {
        Logger_Printf("occupancy,%s\r\n", Control_OccupancyName(pir->occupied));
    }

    return occupancy_changed;
}

static void Control_ToggleMode(ControlMode_t *mode, FanState_t *state, bool occupied)
{
    if ((mode == NULL) || (state == NULL))
    {
        return;
    }

    *mode = (*mode == CONTROL_MODE_MANUAL) ? CONTROL_MODE_AUTO : CONTROL_MODE_MANUAL;
    current_mode = *mode;
    Logger_Printf("mode,%s\r\n", Control_ModeName(*mode));

    if ((*mode == CONTROL_MODE_AUTO) && latest_temp_valid)
    {
        Control_ApplyAutoState(state, occupied);
    }
    else if (*mode == CONTROL_MODE_AUTO)
    {
        Control_ApplyAutoState(state, occupied);
    }
}

static FanState_t Control_ApplyEvent(FanState_t state, ControlEvent_t event)
{
    switch (event)
    {
        case CONTROL_EVENT_SPEED_UP:
            if (state < FAN_STATE_SPEED3)
            {
                state = (FanState_t)(state + 1);
            }
            break;

        case CONTROL_EVENT_SPEED_DOWN:
            if (state > FAN_STATE_OFF)
            {
                state = (FanState_t)(state - 1);
            }
            break;

        default:
            break;
    }

    return state;
}

static void Control_ApplyAutoState(FanState_t *state, bool occupied)
{
    if (state == NULL)
    {
        return;
    }

    FanState_t next_state = Control_StateForAuto(occupied);

    if (next_state != *state)
    {
        *state = next_state;
        current_fan_state = *state;
        (void)FanControl_RequestState(*state);
    }

    if (!occupied)
    {
        Logger_Printf("auto_state,%s,occupancy,EMPTY\r\n",
                      FanControl_StateName(*state));
        return;
    }

    if (!latest_temp_valid)
    {
        Logger_Printf("auto_state,%s,temp_c,invalid,occupancy,OCCUPIED\r\n",
                      FanControl_StateName(*state));
        return;
    }

    Logger_Printf("auto_state,%s,temp_c,%ld.%02ld,occupancy,OCCUPIED\r\n",
                  FanControl_StateName(*state),
                  latest_temp_centi_c / 100,
                  latest_temp_centi_c % 100);
}

static FanState_t Control_StateForAuto(bool occupied)
{
    if (!occupied || !latest_temp_valid)
    {
        return FAN_STATE_OFF;
    }

    return Control_StateFromTemperature(latest_temp_centi_c);
}

static FanState_t Control_StateFromTemperature(int32_t temp_centi_c)
{
    if (temp_centi_c < CONTROL_AUTO_OFF_MAX_CC)
    {
        return FAN_STATE_OFF;
    }

    if (temp_centi_c < CONTROL_AUTO_SPEED1_MAX_CC)
    {
        return FAN_STATE_SPEED1;
    }

    if (temp_centi_c < CONTROL_AUTO_SPEED2_MAX_CC)
    {
        return FAN_STATE_SPEED2;
    }

    return FAN_STATE_SPEED3;
}

static const char *Control_OccupancyName(bool occupied)
{
    return occupied ? "OCCUPIED" : "EMPTY";
}

const char *Control_ModeName(ControlMode_t mode)
{
    switch (mode)
    {
        case CONTROL_MODE_MANUAL:
            return "MANUAL";

        case CONTROL_MODE_AUTO:
            return "AUTO";

        default:
            return "UNKNOWN";
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    (void)GPIO_Pin;
}
