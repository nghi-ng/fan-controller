#include "sensor.h"

#include "FreeRTOS.h"
#include "task.h"
#include "bmp280.h"
#include "control.h"
#include "i2c_bus.h"
#include "logger.h"

#define SENSOR_TASK_STACK_WORDS   384U
#define SENSOR_TASK_PRIORITY      (tskIDLE_PRIORITY + 1U)
#define SENSOR_I2C_SCAN_TIMEOUT_MS 5U
#define SENSOR_I2C_MUTEX_TIMEOUT_MS 50U
#define SENSOR_STARTUP_DELAY_MS   300U
#define SENSOR_PERIOD_MS          1000U
#define SENSOR_REINIT_PERIOD_MS   5000U

static TaskHandle_t sensor_task_handle;

static void Sensor_Task(void *argument);
static bool Sensor_TryInitBmp280(void);
static void Sensor_LogI2cScan(void);

bool Sensor_Init(void)
{
    BaseType_t created = xTaskCreate(Sensor_Task,
                                     "SensorTask",
                                     SENSOR_TASK_STACK_WORDS,
                                     NULL,
                                     SENSOR_TASK_PRIORITY,
                                     &sensor_task_handle);

    return created == pdPASS;
}

static void Sensor_Task(void *argument)
{
    (void)argument;

    Logger_Write("SensorTask started: BMP280 ID check\r\n");

    vTaskDelay(pdMS_TO_TICKS(SENSOR_STARTUP_DELAY_MS));
    Sensor_LogI2cScan();
    (void)Sensor_TryInitBmp280();

    TickType_t last_wake = xTaskGetTickCount();
    TickType_t last_reinit = last_wake;

    for (;;)
    {
        int32_t temp_centi_c = 0;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));

        if (!Bmp280_IsReady() &&
            ((xTaskGetTickCount() - last_reinit) >= pdMS_TO_TICKS(SENSOR_REINIT_PERIOD_MS)))
        {
            last_reinit = xTaskGetTickCount();
            Sensor_LogI2cScan();
            (void)Sensor_TryInitBmp280();
        }

        if (Bmp280_ReadTemperatureCentiC(&temp_centi_c))
        {
            Control_UpdateTemperatureCentiC(temp_centi_c);
            Logger_Printf("temp_c,%ld.%02ld,raw,%ld\r\n",
                          temp_centi_c / 100,
                          temp_centi_c % 100,
                          Bmp280_LastRawTemperature());
        }
        else
        {
            Bmp280_SetNotReady();
            last_reinit = xTaskGetTickCount() - pdMS_TO_TICKS(SENSOR_REINIT_PERIOD_MS);
            Logger_Write("temp_c,read_failed\r\n");
        }
    }
}

static bool Sensor_TryInitBmp280(void)
{
    uint8_t address = 0U;
    uint8_t chip_id = 0U;

    if (Bmp280_Detect(&address, &chip_id))
    {
        Logger_Printf("bmp280_addr,0x%02X,bmp280_id,0x%02X,%s\r\n",
                      address,
                      chip_id,
                      (chip_id == BMP280_EXPECTED_CHIP_ID) ? "OK" : "UNEXPECTED");

        if (chip_id != BMP280_EXPECTED_CHIP_ID)
        {
            return false;
        }

        if (Bmp280_Init(address))
        {
            Logger_Write("bmp280_init,OK\r\n");
            return true;
        }

        Logger_Write("bmp280_init,failed\r\n");
        return false;
    }

    bool bmp280_ready = false;

    if (I2cBus_Lock(pdMS_TO_TICKS(SENSOR_I2C_MUTEX_TIMEOUT_MS)))
    {
        bmp280_ready = I2cBus_IsDeviceReady(BMP280_I2C_ADDRESS, SENSOR_I2C_SCAN_TIMEOUT_MS);
        I2cBus_Unlock();
    }

    uint32_t i2c_status = I2cBus_LastStatus();
    uint32_t i2c_error = I2cBus_LastError();

    Logger_Printf("bmp280_id,read_failed,ready,%u,status,%lu,halerr,0x%08lX\r\n",
                  bmp280_ready ? 1U : 0U,
                  i2c_status,
                  i2c_error);

    return false;
}

static void Sensor_LogI2cScan(void)
{
    uint8_t found_count = 0U;

    Logger_Write("i2c_scan,start\r\n");

    if (!I2cBus_Lock(pdMS_TO_TICKS(SENSOR_I2C_MUTEX_TIMEOUT_MS)))
    {
        Logger_Write("i2c_scan,mutex_timeout\r\n");
        return;
    }

    for (uint8_t address = 1U; address < 128U; address++)
    {
        if (I2cBus_IsDeviceReady(address, SENSOR_I2C_SCAN_TIMEOUT_MS))
        {
            found_count++;
            Logger_Printf("i2c_found,0x%02X\r\n", address);
        }
    }

    I2cBus_Unlock();

    if (found_count == 0U)
    {
        Logger_Write("i2c_scan,no_devices\r\n");
    }

    Logger_Write("i2c_scan,end\r\n");
}
