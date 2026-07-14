#include "encoder.h"

#include "FreeRTOS.h"
#include "task.h"
#include "control.h"
#include "logger.h"
#include "main.h"

#define ENCODER_TASK_STACK_WORDS   256U
#define ENCODER_TASK_PRIORITY      (tskIDLE_PRIORITY + 3U)
#define ENCODER_POLL_MS            2U

static TaskHandle_t encoder_task_handle;

static void Encoder_Task(void *argument);

bool Encoder_Init(void)
{
    BaseType_t created = xTaskCreate(Encoder_Task,
                                     "EncoderTask",
                                     ENCODER_TASK_STACK_WORDS,
                                     NULL,
                                     ENCODER_TASK_PRIORITY,
                                     &encoder_task_handle);

    return created == pdPASS;
}

static void Encoder_Task(void *argument)
{
    (void)argument;

    GPIO_PinState last_a = HAL_GPIO_ReadPin(ENC_A_GPIO_Port, ENC_A_Pin);
    TickType_t last_wake = xTaskGetTickCount();

    Logger_Write("EncoderTask started: PA0/PA1 polling\r\n");

    for (;;)
    {
        GPIO_PinState current_a = HAL_GPIO_ReadPin(ENC_A_GPIO_Port, ENC_A_Pin);

        if ((last_a == GPIO_PIN_SET) && (current_a == GPIO_PIN_RESET))
        {
            GPIO_PinState current_b = HAL_GPIO_ReadPin(ENC_B_GPIO_Port, ENC_B_Pin);

            if (current_b == GPIO_PIN_SET)
            {
                (void)Control_PostEvent(CONTROL_EVENT_SPEED_UP);
            }
            else
            {
                (void)Control_PostEvent(CONTROL_EVENT_SPEED_DOWN);
            }
        }

        last_a = current_a;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ENCODER_POLL_MS));
    }
}
