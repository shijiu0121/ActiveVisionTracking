#ifndef LCD_BUS_H
#define LCD_BUS_H
#include <stdint.h>
void LcdBus_Init(void);
void LcdBus_Command(uint16_t value);
void LcdBus_Data(uint16_t value);
uint16_t LcdBus_Read(void);
#endif
