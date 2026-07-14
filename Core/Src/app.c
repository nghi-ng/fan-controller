#include "app.h"

#include "control.h"
#include "display.h"
#include "encoder.h"
#include "fan_control.h"
#include "i2c_bus.h"
#include "load.h"
#include "logger.h"
#include "main.h"
#include "rt_measure.h"
#include "sensor.h"

extern I2C_HandleTypeDef hi2c1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart2;

bool App_CreateTasks(void)
{
    if (!Logger_Init(&huart2))
    {
        return false;
    }

    if (!I2cBus_Init(&hi2c1))
    {
        return false;
    }

    if (!RtMeasure_Init(&htim2))
    {
        return false;
    }

    if (!FanControl_Init(&htim3))
    {
        return false;
    }

    if (!Control_Init())
    {
        return false;
    }

    if (!Encoder_Init())
    {
        return false;
    }

    if (!Sensor_Init())
    {
        return false;
    }

    if (!Display_Init())
    {
        return false;
    }

    if (!Load_Init())
    {
        (void)Logger_Write("WARNING: LoadTask not started\r\n");
    }

    return true;
}

void App_QueueStartupLog(void)
{
    (void)Logger_Write("System startup complete: scheduler running, UART uses DMA\r\n");
}
