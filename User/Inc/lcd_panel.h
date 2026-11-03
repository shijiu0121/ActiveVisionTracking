#ifndef LCD_PANEL_H
#define LCD_PANEL_H
#include <stdint.h>
#include <stdbool.h>
#define LCD_WIDTH 800
#define LCD_HEIGHT 480
typedef struct { uint16_t id, missing_glyphs; uint8_t stage, error; bool ready; uint32_t since; } LcdDiagnostics;
extern LcdDiagnostics lcd_diag;
void Lcd_Init(uint32_t now);
void Lcd_Task(uint32_t now);
void Lcd_Rect(int x, int y, int w, int h, uint16_t color);
void Lcd_Text(int x, int y, const char *text, unsigned scale, uint16_t fg, uint16_t bg);
void Lcd_Backlight(uint8_t percent);
#endif
