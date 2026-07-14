#include "bmp280.h"

#include "FreeRTOS.h"
#include "i2c_bus.h"

#define BMP280_REG_CHIP_ID        0xD0U
#define BMP280_REG_CALIB_START    0x88U
#define BMP280_REG_CTRL_MEAS      0xF4U
#define BMP280_REG_CONFIG         0xF5U
#define BMP280_REG_TEMP_MSB       0xFAU
#define BMP280_I2C_TIMEOUT_MS     100U
#define BMP280_MUTEX_TIMEOUT_MS   100U
#define BMP280_CALIB_LENGTH       24U
#define BMP280_TEMP_RAW_LENGTH    3U
#define BMP280_CTRL_MEAS_NORMAL   0xB7U
#define BMP280_CONFIG_DEFAULT     0x00U

typedef struct
{
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
} Bmp280Calib_t;

static uint8_t bmp280_address = BMP280_I2C_ADDRESS;
static Bmp280Calib_t bmp280_calib;
static bool bmp280_ready;
static int32_t bmp280_last_raw_temperature;

static bool Bmp280_ReadBytes(uint8_t address, uint8_t reg, uint8_t *data, uint16_t length);
static bool Bmp280_WriteByte(uint8_t address, uint8_t reg, uint8_t value);
static bool Bmp280_ReadChipIdAtAddress(uint8_t address, uint8_t *chip_id);
static void Bmp280_ParseCalibration(const uint8_t *data);
static int32_t Bmp280_CompensateTemperature(int32_t adc_t);

bool Bmp280_Detect(uint8_t *address, uint8_t *chip_id)
{
    if ((address == NULL) || (chip_id == NULL))
    {
        return false;
    }

    if (Bmp280_ReadChipIdAtAddress(BMP280_I2C_ADDRESS, chip_id))
    {
        *address = BMP280_I2C_ADDRESS;
        return true;
    }

    return false;
}

bool Bmp280_Init(uint8_t address)
{
    uint8_t chip_id = 0U;
    uint8_t calib[BMP280_CALIB_LENGTH] = {0};

    bmp280_ready = false;

    if (!Bmp280_ReadChipIdAtAddress(address, &chip_id) ||
        (chip_id != BMP280_EXPECTED_CHIP_ID))
    {
        return false;
    }

    if (!Bmp280_ReadBytes(address, BMP280_REG_CALIB_START, calib, BMP280_CALIB_LENGTH))
    {
        return false;
    }

    Bmp280_ParseCalibration(calib);

    if (!Bmp280_WriteByte(address, BMP280_REG_CONFIG, BMP280_CONFIG_DEFAULT))
    {
        return false;
    }

    if (!Bmp280_WriteByte(address, BMP280_REG_CTRL_MEAS, BMP280_CTRL_MEAS_NORMAL))
    {
        return false;
    }

    bmp280_address = address;
    bmp280_ready = true;

    return true;
}

bool Bmp280_IsReady(void)
{
    return bmp280_ready;
}

void Bmp280_SetNotReady(void)
{
    bmp280_ready = false;
}

bool Bmp280_ReadTemperatureCentiC(int32_t *temp_centi_c)
{
    uint8_t raw[BMP280_TEMP_RAW_LENGTH] = {0};

    if ((temp_centi_c == NULL) || !bmp280_ready)
    {
        return false;
    }

    if (!Bmp280_ReadBytes(bmp280_address, BMP280_REG_TEMP_MSB, raw, BMP280_TEMP_RAW_LENGTH))
    {
        return false;
    }

    int32_t adc_t = (((int32_t)raw[0]) << 12) |
                    (((int32_t)raw[1]) << 4) |
                    (((int32_t)raw[2]) >> 4);

    bmp280_last_raw_temperature = adc_t;
    *temp_centi_c = Bmp280_CompensateTemperature(adc_t);

    return true;
}

int32_t Bmp280_LastRawTemperature(void)
{
    return bmp280_last_raw_temperature;
}

static bool Bmp280_ReadChipIdAtAddress(uint8_t address, uint8_t *chip_id)
{
    return Bmp280_ReadBytes(address, BMP280_REG_CHIP_ID, chip_id, 1U);
}

static bool Bmp280_ReadBytes(uint8_t address, uint8_t reg, uint8_t *data, uint16_t length)
{
    if ((data == NULL) || (I2cBus_Handle() == NULL))
    {
        return false;
    }

    if (!I2cBus_Lock(pdMS_TO_TICKS(BMP280_MUTEX_TIMEOUT_MS)))
    {
        return false;
    }

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(I2cBus_Handle(),
                                                address << 1,
                                                reg,
                                                I2C_MEMADD_SIZE_8BIT,
                                                data,
                                                length,
                                                BMP280_I2C_TIMEOUT_MS);

    I2cBus_Unlock();

    if (status != HAL_OK)
    {
        I2cBus_Recover();
    }

    return status == HAL_OK;
}

static bool Bmp280_WriteByte(uint8_t address, uint8_t reg, uint8_t value)
{
    uint8_t tx_data[2] = {reg, value};

    if (I2cBus_Handle() == NULL)
    {
        return false;
    }

    if (!I2cBus_Lock(pdMS_TO_TICKS(BMP280_MUTEX_TIMEOUT_MS)))
    {
        return false;
    }

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(I2cBus_Handle(),
                                                       address << 1,
                                                       tx_data,
                                                       sizeof(tx_data),
                                                       BMP280_I2C_TIMEOUT_MS);

    I2cBus_Unlock();

    if (status != HAL_OK)
    {
        I2cBus_Recover();
    }

    return status == HAL_OK;
}

static void Bmp280_ParseCalibration(const uint8_t *data)
{
    bmp280_calib.dig_t1 = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    bmp280_calib.dig_t2 = (int16_t)((uint16_t)data[2] | ((uint16_t)data[3] << 8));
    bmp280_calib.dig_t3 = (int16_t)((uint16_t)data[4] | ((uint16_t)data[5] << 8));
}

static int32_t Bmp280_CompensateTemperature(int32_t adc_t)
{
    int32_t var1 = ((((adc_t >> 3) - ((int32_t)bmp280_calib.dig_t1 << 1))) *
                    ((int32_t)bmp280_calib.dig_t2)) >> 11;
    int32_t var2 = (((((adc_t >> 4) - ((int32_t)bmp280_calib.dig_t1)) *
                      ((adc_t >> 4) - ((int32_t)bmp280_calib.dig_t1))) >> 12) *
                    ((int32_t)bmp280_calib.dig_t3)) >> 14;

    int32_t t_fine = var1 + var2;

    return (t_fine * 5 + 128) >> 8;
}
