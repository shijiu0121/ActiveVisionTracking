/* 只在 Windows/Linux 原生测试中分配整帧缓冲。STM32 固件不编译此文件。 */
#include "lcd_bus.h"
#include "lcd_panel.h"
#include "ui.h"
#include "board_io.h"
#include <string.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
BoardDiagnostics board_diag;
static uint16_t framebuffer[800 * 480];
static unsigned command, index, x0, x1, y0, y1, x, y, violations, reads;
static uint8_t params[8];
void LcdBus_Init(void) { }
void LcdBus_Command(uint16_t value) { command = value; index = 0; if (value == 0x2c) { x = x0; y = y0; } }
void LcdBus_Data(uint16_t value) {
    if (command == 0x2c) {
        if (x >= 800 || y >= 480) violations++;
        else framebuffer[y * 800 + x] = value;
        if (++x > x1) { x = x0; if (++y > y1) y = y0; }
    } else {
        if (index < sizeof(params)) params[index++] = (uint8_t)value;
        if (command == 0x2a && index == 4) { x0 = params[0] * 256 + params[1]; x1 = params[2] * 256 + params[3]; }
        if (command == 0x2b && index == 4) { y0 = params[0] * 256 + params[1]; y1 = params[2] * 256 + params[3]; }
    }
}
uint16_t LcdBus_Read(void) { static const uint16_t id[] = {0, 0x57, 0x61}; return id[reads++ % 3]; }
EXPORT void ui_render(unsigned demo, unsigned selftest) {
    AvtDevice d = {0}; AvtParser p = {0};
    memset(framebuffer, 0, sizeof(framebuffer)); violations = reads = 0;
    Lcd_Init(0); for (unsigned t = 0; t < 300; t++) Lcd_Task(t);
    UI_Init(300, selftest != 0); d.state = demo ? AVT_RUNNING : AVT_IDLE;
    for (unsigned t = 300; t < 5000; t++) UI_Task(t, &d, &p);
}
EXPORT uint16_t *ui_pixels(void) { return framebuffer; }
EXPORT unsigned ui_violations(void) { return violations; }
EXPORT unsigned ui_missing_glyphs(void) { return lcd_diag.missing_glyphs; }
EXPORT void ui_boundary(void) {
    Lcd_Rect(-100, -100, 110, 110, 0xffff); Lcd_Rect(799, 479, 50, 50, 0xf800);
    Lcd_Rect(0, 0, -2, 4, 0); Lcd_Text(795, 478, "TEST", 4, 0xffff, 0);
}
