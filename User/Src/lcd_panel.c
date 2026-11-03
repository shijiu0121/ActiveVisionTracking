#include "lcd_panel.h"
#include "lcd_bus.h"
#include <ctype.h>
#include <string.h>
#include "font_cn.h"
LcdDiagnostics lcd_diag;
static void command(uint16_t cmd, const uint8_t *p, unsigned n) {
    LcdBus_Command(cmd);
    while (n--) LcdBus_Data(*p++);
}
#define CMD(c, ...) do { const uint8_t p[] = {__VA_ARGS__}; command(c, p, sizeof(p)); } while (0)
void Lcd_Init(uint32_t now) { memset(&lcd_diag, 0, sizeof(lcd_diag)); LcdBus_Init(); lcd_diag.since = now; }
void Lcd_Backlight(uint8_t percent) {
    if (percent > 100) percent = 100;
    CMD(0xbe, 5, (uint8_t)(255U * percent / 100U), 1, 0, 0, 0);
}
void Lcd_Task(uint32_t now) {
    if (lcd_diag.ready || lcd_diag.error) return;
    uint32_t elapsed = now - lcd_diag.since;
    switch (lcd_diag.stage) {
    case 0:
        if (elapsed < 60) return;
        LcdBus_Command(0xa1); (void)LcdBus_Read();
        lcd_diag.id = (uint16_t)((LcdBus_Read() & 255U) << 8); lcd_diag.id |= LcdBus_Read() & 255U;
        if (lcd_diag.id != 0x5761) { lcd_diag.error = 1; return; }
        /* 原理图 Y1=10MHz；VCO=300MHz，PLL=100MHz。保留数据手册要求的 100ms 锁定等待。 */
        CMD(0xe2, 0x1d, 2, 4); CMD(0xe0, 1); break;
    case 1:
        if (elapsed < 100) return;
        CMD(0xe0, 3); break;
    case 2:
        if (elapsed < 12) return;
        LcdBus_Command(1); break;
    case 3:
        if (elapsed < 10) return;
        /* 独立实现，寄存器含义依据 SSD1963 Rev1.1，面板参数依据 AT070TN92。
         * FPR=0x553f7，100MHz*(FPR+1)/2^20 = 33.3MHz；HT=1056 VT=525。 */
        CMD(0xe6, 0x05, 0x53, 0xf7);
        CMD(0xb0, 0x20, 0, 3, 0x1f, 1, 0xdf, 0);
        CMD(0xb4, 4, 0x1f, 0, 46, 0, 0, 0, 0);
        CMD(0xb6, 2, 0x0c, 0, 23, 0, 0, 0);
        CMD(0xf0, 3); CMD(0x36, 0); CMD(0xb8, 3, 1); CMD(0xba, 1);
        CMD(0xd0, 0); Lcd_Backlight(0); LcdBus_Command(0x29);
        lcd_diag.ready = true; break;
    default: lcd_diag.error = 2; return;
    }
    lcd_diag.stage++; lcd_diag.since = now;
}
static void window(int x, int y, int w, int h) {
    unsigned xe = (unsigned)(x + w - 1), ye = (unsigned)(y + h - 1);
    CMD(0x2a, x >> 8, x, xe >> 8, xe); CMD(0x2b, y >> 8, y, ye >> 8, ye); LcdBus_Command(0x2c);
}
void Lcd_Rect(int x, int y, int w, int h, uint16_t color) {
    if (!lcd_diag.ready || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT || w <= 0 || h <= 0) return;
    if (w > LCD_WIDTH - x) w = LCD_WIDTH - x;
    if (h > LCD_HEIGHT - y) h = LCD_HEIGHT - y;
    window(x, y, w, h);
    for (int i = 0; i < w * h; i++) LcdBus_Data(color);
}
/* 原创紧凑 5x7 字模：每行五位；小写映射为大写，不依赖外部字库/文件。 */
static const char symbols[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.:/+=?%()_";
static const uint8_t glyphs[][7] = {
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},
 {31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{15,16,16,16,16,16,15},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},
 {31,16,16,30,16,16,16},{15,16,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},
 {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
 {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},
 {31,1,2,4,8,16,31},{0,0,0,31,0,0,0},{0,0,0,0,0,12,12},{0,12,12,0,12,12,0},{1,2,2,4,8,8,16},
 {0,4,4,31,4,4,0},{0,0,31,0,31,0,0},{14,17,1,2,4,0,4},{25,25,2,4,8,19,19},{2,4,8,8,8,4,2},
 {8,4,2,2,2,4,8},{0,0,0,0,0,0,31}
};
void Lcd_Text(int x, int y, const char *text, unsigned scale, uint16_t fg, uint16_t bg) {
    if (!scale || scale > 4) return;
    /* 最多 24 像素行缓存，边界逐字符裁剪。 */
    uint16_t line[32];
    while (*text && x < LCD_WIDTH) {
        if ((unsigned char)*text >= 0x80) {
            const unsigned char *u = (const unsigned char *)text;
            uint16_t code = 0;
            unsigned bytes = 1;
            if ((u[0] & 0xf0) == 0xe0 && u[1] && u[2] && (u[1] & 0xc0) == 0x80 && (u[2] & 0xc0) == 0x80) {
                code = (uint16_t)(((u[0] & 15U) << 12) | ((u[1] & 63U) << 6) | (u[2] & 63U)); bytes = 3;
            }
            const uint8_t *bits = FontCN_Find(code);
            unsigned size = scale == 4 ? 32 : 24;
            if (!bits) lcd_diag.missing_glyphs++;
            if (x < 0 || x + (int)size > LCD_WIDTH) { text += bytes; x += (int)size; continue; }
            for (unsigned row = 0; row < size; row++) {
                int yy = y + (int)row;
                if (yy < 0 || yy >= LCD_HEIGHT) continue;
                for (unsigned col = 0; col < size; col++) {
                    unsigned sy = row * 24 / size, sx = col * 24 / size;
                    bool on = bits && (bits[sy * 3 + sx / 8] & (128U >> (sx % 8)));
                    line[col] = on ? fg : bg;
                }
                window(x, yy, size, 1);
                for (unsigned col = 0; col < size; col++) LcdBus_Data(line[col]);
            }
            text += bytes; x += (int)size; continue;
        }
        char ch = (char)toupper((unsigned char)*text);
        const char *found = ch ? strchr(symbols, ch) : NULL;
        const uint8_t *g = found ? glyphs[found - symbols] : NULL;
        for (unsigned row = 0; row < 8 * scale; row++) {
            int yy = y + (int)row;
            if (x < 0 || yy < 0 || yy >= LCD_HEIGHT) continue;
            unsigned width = 6 * scale;
            if (x + (int)width > LCD_WIDTH) width = LCD_WIDTH - x;
            for (unsigned col = 0; col < width; col++) {
                bool on = g && row / scale < 7 && col / scale < 5 && (g[row / scale] & (16U >> (col / scale)));
                line[col] = on ? fg : bg;
            }
            window(x, yy, width, 1);
            for (unsigned col = 0; col < width; col++) LcdBus_Data(line[col]);
        }
        text++; x += (int)(6 * scale);
    }
}
