#include "i2c_bus.h"

#include "FreeRTOS.h"
#include "semphr.h"

#define I2C_BUS_USE_MUTEX 1U

static I2C_HandleTypeDef *i2c_bus_handle;
static SemaphoreHandle_t i2c_bus_mutex;
static HAL_StatusTypeDef i2c_bus_last_status = HAL_OK;
static uint32_t i2c_bus_last_error = HAL_I2C_ERROR_NONE;

bool I2cBus_Init(I2C_HandleTypeDef *i2c)
{
    i2c_bus_handle = i2c;

#if I2C_BUS_USE_MUTEX
    i2c_bus_mutex = xSemaphoreCreateMutex();

    return (i2c_bus_handle != NULL) && (i2c_bus_mutex != NULL);
#else
    i2c_bus_mutex = NULL;

    return i2c_bus_handle != NULL;
#endif
}

bool I2cBus_Lock(uint32_t timeout_ticks)
{
#if I2C_BUS_USE_MUTEX
    if (i2c_bus_mutex == NULL)
    {
        return false;
    }

    return xSemaphoreTake(i2c_bus_mutex, timeout_ticks) == pdPASS;
#else
    (void)timeout_ticks;
    return true;
#endif
}

void I2cBus_Unlock(void)
{
#if I2C_BUS_USE_MUTEX
    if (i2c_bus_mutex != NULL)
    {
        (void)xSemaphoreGive(i2c_bus_mutex);
    }
#endif
}

I2C_HandleTypeDef *I2cBus_Handle(void)
{
    return i2c_bus_handle;
}

bool I2cBus_IsDeviceReady(uint8_t address, uint32_t timeout_ms)
{
    if (i2c_bus_handle == NULL)
    {
        return false;
    }

    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(i2c_bus_handle,
                                                     address << 1,
                                                     2U,
                                                     timeout_ms);
    i2c_bus_last_status = status;
    i2c_bus_last_error = HAL_I2C_GetError(i2c_bus_handle);

    return status == HAL_OK;
}

void I2cBus_Recover(void)
{
#if I2C_BUS_USE_MUTEX
    if ((i2c_bus_handle == NULL) || (i2c_bus_mutex == NULL))
    {
        return;
    }

    if (xSemaphoreTake(i2c_bus_mutex, pdMS_TO_TICKS(100U)) != pdPASS)
    {
        return;
    }

    (void)HAL_I2C_DeInit(i2c_bus_handle);
    (void)HAL_I2C_Init(i2c_bus_handle);

    (void)xSemaphoreGive(i2c_bus_mutex);
#else
    if (i2c_bus_handle == NULL)
    {
        return;
    }

    (void)HAL_I2C_DeInit(i2c_bus_handle);
    (void)HAL_I2C_Init(i2c_bus_handle);
#endif
}

uint32_t I2cBus_LastStatus(void)
{
    return i2c_bus_last_status;
}

uint32_t I2cBus_LastError(void)
{
    return i2c_bus_last_error;
}
