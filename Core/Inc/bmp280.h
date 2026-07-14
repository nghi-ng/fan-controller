#ifndef BMP280_H
#define BMP280_H

#include <stdbool.h>
#include <stdint.h>

#define BMP280_I2C_ADDRESS        0x77U
#define BMP280_EXPECTED_CHIP_ID   0x58U

bool Bmp280_Detect(uint8_t *address, uint8_t *chip_id);
bool Bmp280_Init(uint8_t address);
bool Bmp280_IsReady(void);
void Bmp280_SetNotReady(void);
bool Bmp280_ReadTemperatureCentiC(int32_t *temp_centi_c);
int32_t Bmp280_LastRawTemperature(void);

#endif /* BMP280_H */
