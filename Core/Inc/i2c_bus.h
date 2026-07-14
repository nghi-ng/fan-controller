#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"

bool I2cBus_Init(I2C_HandleTypeDef *i2c);
bool I2cBus_Lock(uint32_t timeout_ticks);
void I2cBus_Unlock(void);
I2C_HandleTypeDef *I2cBus_Handle(void);
bool I2cBus_IsDeviceReady(uint8_t address, uint32_t timeout_ms);
void I2cBus_Recover(void);
uint32_t I2cBus_LastStatus(void);
uint32_t I2cBus_LastError(void);

#endif /* I2C_BUS_H */
