#ifndef CONTROL_H
#define CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "fan_control.h"

typedef enum
{
    CONTROL_MODE_MANUAL = 0,
    CONTROL_MODE_AUTO
} ControlMode_t;

typedef enum
{
    CONTROL_EVENT_SPEED_UP = 0,
    CONTROL_EVENT_SPEED_DOWN,
    CONTROL_EVENT_TOGGLE_MODE,
    CONTROL_EVENT_TEMPERATURE_UPDATED
} ControlEvent_t;

typedef struct
{
    ControlMode_t mode;
    FanState_t fan_state;
    int32_t temp_centi_c;
    bool temp_valid;
    bool pir_motion_active;
    bool pir_occupied;
} ControlStatus_t;

bool Control_Init(void);
bool Control_PostEvent(ControlEvent_t event);
void Control_PostEventFromISR(ControlEvent_t event);
void Control_UpdateTemperatureCentiC(int32_t temp_centi_c);
void Control_GetStatus(ControlStatus_t *status);
const char *Control_ModeName(ControlMode_t mode);

#endif /* CONTROL_H */
