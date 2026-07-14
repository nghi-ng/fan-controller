#include "display.h"

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "control.h"
#include "fan_control.h"
#include "lcd.h"
#include "logger.h"

#define DISPLAY_TASK_STACK_WORDS   512U
#define DISPLAY_TASK_PRIORITY      (tskIDLE_PRIORITY + 1U)
#define DISPLAY_PERIOD_MS          500U
#define DISPLAY_STARTUP_DELAY_MS   650U
#define DISPLAY_INIT_RETRY_MS      1300U

static TaskHandle_t display_task_handle;

static void Display_Task(void *argument);
static void Display_FormatStatusLine(char *buffer, size_t buffer_size, const ControlStatus_t *status);
static const char *Display_ModeShortName(ControlMode_t mode);
static const char *Display_FanShortName(FanState_t state);
static const char *Display_PirShortName(const ControlStatus_t *status);

bool Display_Init(void)
{
    BaseType_t created = xTaskCreate(Display_Task,
                                     "DisplayTask",
                                     DISPLAY_TASK_STACK_WORDS,
                                     NULL,
                                     DISPLAY_TASK_PRIORITY,
                                     &display_task_handle);

    return created == pdPASS;
}

static void Display_Task(void *argument)
{
    (void)argument;

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_STARTUP_DELAY_MS));

    while (!Lcd_Init())
    {
        Logger_Printf("lcd_probe,%s\r\n", Lcd_IsPresent() ? "present" : "missing");
        Logger_Write("lcd_init,failed,retry\r\n");
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_INIT_RETRY_MS));
    }

    Logger_Write("DisplayTask started: LCD dashboard enabled\r\n");

    TickType_t last_wake = xTaskGetTickCount();

    for (;;)
    {
        ControlStatus_t status;
        char line1[17] = {0};
        char line2[17] = {0};

        Control_GetStatus(&status);

        (void)snprintf(line1, sizeof(line1), "%s %s",
                       Display_ModeShortName(status.mode),
                       Display_FanShortName(status.fan_state));
        Display_FormatStatusLine(line2, sizeof(line2), &status);

        (void)Lcd_WriteLines(line1, line2);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(DISPLAY_PERIOD_MS));
    }
}

static void Display_FormatStatusLine(char *buffer, size_t buffer_size, const ControlStatus_t *status)
{
    if ((buffer == NULL) || (buffer_size == 0U) || (status == NULL))
    {
        return;
    }

    if (!status->temp_valid)
    {
        (void)snprintf(buffer, buffer_size, "T:ERR PIR:%s",
                       Display_PirShortName(status));
        return;
    }

    (void)snprintf(buffer, buffer_size, "T:%ld.%02ld PIR:%s",
                   status->temp_centi_c / 100,
                   status->temp_centi_c % 100,
                   Display_PirShortName(status));
}

static const char *Display_ModeShortName(ControlMode_t mode)
{
    switch (mode)
    {
        case CONTROL_MODE_MANUAL:
            return "MAN";

        case CONTROL_MODE_AUTO:
            return "AUTO";

        default:
            return "UNK";
    }
}

static const char *Display_FanShortName(FanState_t state)
{
    switch (state)
    {
        case FAN_STATE_OFF:
            return "OFF";

        case FAN_STATE_SPEED1:
            return "S1";

        case FAN_STATE_SPEED2:
            return "S2";

        case FAN_STATE_SPEED3:
            return "S3";

        default:
            return "?";
    }
}

static const char *Display_PirShortName(const ControlStatus_t *status)
{
    if ((status != NULL) && status->pir_motion_active)
    {
        return "MOT";
    }

    if ((status != NULL) && status->pir_occupied)
    {
        return "OCC";
    }

    return "EMP";
}
