#include "lcd.h"

#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "i2c_bus.h"

#define LCD_I2C_ADDRESS          0x3EU
#define LCD_I2C_TIMEOUT_MS       50U
#define LCD_MUTEX_TIMEOUT_MS     50U
#define LCD_COLS                 16U
#define LCD_CMD_CONTROL          0x80U
#define LCD_DATA_CONTROL         0x40U
#define LCD_LINE0_ADDR           0x80U
#define LCD_LINE1_ADDR           0xC0U
#define LCD_COMMAND_DELAY_MS     2U
#define LCD_COMMAND_RETRIES      3U

static bool Lcd_Send(uint8_t control, uint8_t value);
static bool Lcd_Command(uint8_t command);
static bool Lcd_CommandRetry(uint8_t command);
static bool Lcd_SetCursor(uint8_t row);
static bool Lcd_WritePadded(const char *text);

bool Lcd_IsPresent(void)
{
    bool present = false;

    if (!I2cBus_Lock(pdMS_TO_TICKS(LCD_MUTEX_TIMEOUT_MS)))
    {
        return false;
    }

    present = I2cBus_IsDeviceReady(LCD_I2C_ADDRESS, LCD_I2C_TIMEOUT_MS);
    I2cBus_Unlock();

    return present;
}

bool Lcd_Init(void)
{
    vTaskDelay(pdMS_TO_TICKS(100U));

    if (!Lcd_CommandRetry(0x38U) ||
        !Lcd_CommandRetry(0x39U) ||
        !Lcd_CommandRetry(0x14U) ||
        !Lcd_CommandRetry(0x70U) ||
        !Lcd_CommandRetry(0x56U) ||
        !Lcd_CommandRetry(0x6CU))
    {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(200U));

    if (!Lcd_CommandRetry(0x38U) ||
        !Lcd_CommandRetry(0x0CU) ||
        !Lcd_CommandRetry(0x01U))
    {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(2U));

    return true;
}

bool Lcd_WriteLines(const char *line1, const char *line2)
{
    return Lcd_SetCursor(0U) &&
           Lcd_WritePadded(line1) &&
           Lcd_SetCursor(1U) &&
           Lcd_WritePadded(line2);
}

static bool Lcd_Send(uint8_t control, uint8_t value)
{
    uint8_t tx_data[2] = {control, value};

    if (I2cBus_Handle() == NULL)
    {
        return false;
    }

    if (!I2cBus_Lock(pdMS_TO_TICKS(LCD_MUTEX_TIMEOUT_MS)))
    {
        return false;
    }

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(I2cBus_Handle(),
                                                       LCD_I2C_ADDRESS << 1,
                                                       tx_data,
                                                       sizeof(tx_data),
                                                       LCD_I2C_TIMEOUT_MS);

    I2cBus_Unlock();

    if (status != HAL_OK)
    {
        I2cBus_Recover();
    }

    return status == HAL_OK;
}

static bool Lcd_Command(uint8_t command)
{
    return Lcd_Send(LCD_CMD_CONTROL, command);
}

static bool Lcd_CommandRetry(uint8_t command)
{
    for (uint8_t attempt = 0U; attempt < LCD_COMMAND_RETRIES; attempt++)
    {
        if (Lcd_Command(command))
        {
            vTaskDelay(pdMS_TO_TICKS(LCD_COMMAND_DELAY_MS));
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(LCD_COMMAND_DELAY_MS));
    }

    return false;
}

static bool Lcd_SetCursor(uint8_t row)
{
    return Lcd_Command((row == 0U) ? LCD_LINE0_ADDR : LCD_LINE1_ADDR);
}

static bool Lcd_WritePadded(const char *text)
{
    char padded[LCD_COLS];
    size_t length = 0U;

    if (text != NULL)
    {
        length = strnlen(text, LCD_COLS);
        memcpy(padded, text, length);
    }

    while (length < LCD_COLS)
    {
        padded[length] = ' ';
        length++;
    }

    for (uint8_t i = 0U; i < LCD_COLS; i++)
    {
        if (!Lcd_Send(LCD_DATA_CONTROL, (uint8_t)padded[i]))
        {
            return false;
        }
    }

    return true;
}
