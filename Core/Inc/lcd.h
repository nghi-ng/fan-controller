#ifndef LCD_H
#define LCD_H

#include <stdbool.h>

bool Lcd_IsPresent(void);
bool Lcd_Init(void);
bool Lcd_WriteLines(const char *line1, const char *line2);

#endif /* LCD_H */
