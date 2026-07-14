#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "app_config.h"

#define LOGGER_QUEUE_LENGTH       12U
#define LOGGER_MESSAGE_LENGTH     128U
#define LOGGER_TASK_STACK_WORDS   512U
#define LOGGER_TASK_PRIORITY      (tskIDLE_PRIORITY + 1U)

typedef struct
{
    char text[LOGGER_MESSAGE_LENGTH];
    uint16_t length;
} LoggerMessage_t;

static UART_HandleTypeDef *logger_uart;
static QueueHandle_t logger_queue;
static TaskHandle_t logger_task_handle;
static uint8_t logger_dma_buffer[LOGGER_MESSAGE_LENGTH];

static void Logger_Task(void *argument);
static void Logger_TransmitAndWait(const uint8_t *data, uint16_t length);
static bool Logger_VPrintf(const char *format, va_list args);

bool Logger_Init(UART_HandleTypeDef *uart)
{
    logger_uart = uart;
    logger_queue = xQueueCreate(LOGGER_QUEUE_LENGTH, sizeof(LoggerMessage_t));

    if ((logger_uart == NULL) || (logger_queue == NULL))
    {
        return false;
    }

    BaseType_t created = xTaskCreate(Logger_Task,
                                     "DebugTask",
                                     LOGGER_TASK_STACK_WORDS,
                                     NULL,
                                     LOGGER_TASK_PRIORITY,
                                     &logger_task_handle);

    return created == pdPASS;
}

bool Logger_Write(const char *text)
{
    if ((logger_queue == NULL) || (text == NULL))
    {
        return false;
    }

    LoggerMessage_t message = {0};
    size_t length = strnlen(text, LOGGER_MESSAGE_LENGTH - 1U);

    memcpy(message.text, text, length);
    message.text[length] = '\0';
    message.length = (uint16_t)length;

    return xQueueSend(logger_queue, &message, 0U) == pdPASS;
}

bool Logger_Printf(const char *format, ...)
{
    bool queued;
    va_list args;

    va_start(args, format);
    queued = Logger_VPrintf(format, args);
    va_end(args);

    return queued;
}

static bool Logger_VPrintf(const char *format, va_list args)
{
    if ((logger_queue == NULL) || (format == NULL))
    {
        return false;
    }

    LoggerMessage_t message = {0};
    int written = vsnprintf(message.text, LOGGER_MESSAGE_LENGTH, format, args);

    if (written < 0)
    {
        return false;
    }

    if (written >= (int)LOGGER_MESSAGE_LENGTH)
    {
        message.length = LOGGER_MESSAGE_LENGTH - 1U;
        message.text[LOGGER_MESSAGE_LENGTH - 1U] = '\0';
    }
    else
    {
        message.length = (uint16_t)written;
    }

    return xQueueSend(logger_queue, &message, 0U) == pdPASS;
}

static void Logger_Task(void *argument)
{
    (void)argument;

    static const char startup_message[] =
        "DebugTask started: USART2 DMA logger ready\r\n"
        APP_FIRMWARE_MARKER;
    Logger_TransmitAndWait((const uint8_t *)startup_message,
                           (uint16_t)(sizeof(startup_message) - 1U));

    for (;;)
    {
        LoggerMessage_t message;

        if (xQueueReceive(logger_queue, &message, portMAX_DELAY) == pdPASS)
        {
            if (message.length > 0U)
            {
                memcpy(logger_dma_buffer, message.text, message.length);
                Logger_TransmitAndWait(logger_dma_buffer, message.length);
            }
        }
    }
}

static void Logger_TransmitAndWait(const uint8_t *data, uint16_t length)
{
    if ((logger_uart == NULL) || (data == NULL) || (length == 0U))
    {
        return;
    }

    xTaskNotifyStateClear(NULL);

    if (HAL_UART_Transmit_DMA(logger_uart, (uint8_t *)data, length) == HAL_OK)
    {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if ((logger_task_handle != NULL) && (logger_uart != NULL) &&
        (huart->Instance == logger_uart->Instance))
    {
        BaseType_t higher_priority_task_woken = pdFALSE;

        vTaskNotifyGiveFromISR(logger_task_handle, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((logger_task_handle != NULL) && (logger_uart != NULL) &&
        (huart->Instance == logger_uart->Instance))
    {
        BaseType_t higher_priority_task_woken = pdFALSE;

        vTaskNotifyGiveFromISR(logger_task_handle, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}
