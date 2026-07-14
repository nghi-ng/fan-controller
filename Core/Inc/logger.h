#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"

bool Logger_Init(UART_HandleTypeDef *uart);
bool Logger_Write(const char *text);
bool Logger_Printf(const char *format, ...);

#endif /* LOGGER_H */
