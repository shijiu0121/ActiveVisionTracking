#include "ui.h"
#include "lcd_panel.h"
#include "board_io.h"
#include <stdio.h>
#include <string.h>
#define BG 0x0863U
#define FG 0xef7dU
#define CYAN 0x3e9fU
#define GREEN 0x77e8U
#define AMBER 0xfda0U
static uint16_t clear_y;
static uint8_t phase, row, test_color;
static uint32_t last_refresh, welcome_at, test_at;
static bool test_mode;
static bool test_drawn;
static char events[4][43];
static unsigned event_head;
static int old_x = -1, old_y, old_px, old_py;
void UI_Event(uint32_t now, const char *message) {
    snprintf(events[event_head++ % 4], sizeof(events[0]), "%6lu %s", (unsigned long)(now / 1000), message);
}
void UI_Init(uint32_t now, bool selftest) {
    clear_y = phase = row = test_color = 0; test_mode = selftest; test_drawn = false;
    memset(events, 0, sizeof(events)); event_head = 0; welcome_at = now;
    old_x = -1; UI_Event(now, "启动：仅虚拟控制");
}
bool UI_HandleKey(KeyEvent_t event) {
    if (!test_mode || event == KEY_EVENT_NONE) return false;
    if (event == KEY_EVENT_SHORT) { test_mode = false; phase = 0; clear_y = 0; old_x = -1; }
    return true;
}
static void text(int x, int y, const char *s, uint16_t color) { Lcd_Text(x, y, s, 2, color, BG); }
static void cross(int x, int y, uint16_t c) { Lcd_Rect(x - 7, y, 15, 2, c); Lcd_Rect(x, y - 7, 2, 15, c); }
static void border(void) {
    Lcd_Rect(20, 155, 432, 1, CYAN); Lcd_Rect(20, 400, 432, 1, CYAN);
    Lcd_Rect(20, 155, 1, 246, CYAN); Lcd_Rect(451, 155, 1, 246, CYAN);
    cross(236, 278, 0x39e7);
}
void UI_Task(uint32_t now, const AvtDevice *d, const AvtParser *p) {
    if (!lcd_diag.ready) return;
    static const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffff, 0};
    if (test_mode) {
        if (clear_y < LCD_HEIGHT) { Lcd_Rect(0, clear_y, LCD_WIDTH, 8, colors[test_color]); clear_y += 8; test_at = now; return; }
        if (!test_drawn) {
            Lcd_Text(24, 24, "屏幕色块自检 / 短按KEY0退出", 3, test_color == 3 ? 0 : 0xffff, colors[test_color]);
            Lcd_Rect(0, 0, 12, 12, CYAN); Lcd_Rect(788, 0, 12, 12, AMBER);
            Lcd_Rect(0, 468, 12, 12, GREEN); Lcd_Rect(788, 468, 12, 12, 0xf81f);
            Lcd_Backlight(75); test_drawn = true;
        }
        if ((uint32_t)(now - test_at) >= 1000) { test_color = (test_color + 1) % 5; clear_y = 0; test_drawn = false; }
        return;
    }
    if (phase == 0) {
        Lcd_Rect(0, clear_y, LCD_WIDTH, 8, BG); clear_y += 8;
        if (clear_y >= LCD_HEIGHT) { phase = 1; welcome_at = now; Lcd_Backlight(75); }
        return;
    }
    if (phase == 1) {
        Lcd_Text(24, 16, "主动视觉跟踪系统", 4, CYAN, BG);
        text(330, 24, "ActiveVisionTracking", FG);
        text(30, 78, "固件1.0 / SSD1963 / 800 X 480", FG);
        text(30, 106, "离线启动：仅虚拟控制，无电机输出", AMBER);
        phase = 2; return;
    }
    if (phase == 2) {
        if ((uint32_t)(now - welcome_at) < 1200) return;
        phase = 3; row = 0;
    }
    if (phase == 3) {
        if (row == 0) { Lcd_Rect(20, 76, 760, 60, BG); text(20, 128, "目标示意区 / 仅虚拟显示", CYAN); border(); }
        if (row == 1) text(20, 410, "短按KEY0：启动 / 暂停", FG);
        if (row == 2) text(20, 442, "长按：待机 / KEY0+复位：自检", FG);
        if (++row >= 3) { phase = 4; row = 0; last_refresh = now - 200; }
        return;
    }
    if (row == 0 && (uint32_t)(now - last_refresh) < 200) return;
    char s[68];
    bool local = !d->online && !d->accepted && d->state == AVT_RUNNING;
    const char *states[] = {"待机", "运行中", "已暂停", "故障"};
    const char *tracks[] = {"正在跟踪", "短时遮挡", "目标丢失", "正在重捕获"};
    switch (row++) {
    case 0:
        last_refresh = now;
        Lcd_Rect(20, 76, 760, 28, BG);
        snprintf(s, sizeof(s), "%s  /  %s  /  运行%lus", states[d->state], d->online ? "电脑已连接" : local ? "本地模拟" : "离线", (unsigned long)(now / 1000)); text(20, 78, s, GREEN); break;
    case 1:
        Lcd_Rect(20, 104, 760, 24, BG);
        text(20, 104, d->online ? ((d->flags & 2) ? "电脑合成视频 / SIMULATED" : "电脑本地视频") : local ? "本地生成模拟轨迹 / DEMO" : "无有效目标数据 / OFFLINE", AMBER); break;
    case 2:
        Lcd_Rect(476, 150, 312, 24, BG); text(476, 150, d->online ? tracks[d->track] : "离线：等待电脑数据", CYAN); break;
    case 3:
        snprintf(s, sizeof(s), "数据年龄%5lums      ", d->online ? (unsigned long)((uint32_t)(now - d->last_rx) + d->age) : 0UL); text(476, 180, s, FG); break;
    case 4:
        snprintf(s, sizeof(s), "当前%5u %5u     ", d->valid ? d->x : 0, d->valid ? d->y : 0); text(476, 210, s, FG); break;
    case 5:
        snprintf(s, sizeof(s), "预测%5u %5u     ", d->valid ? d->px : 0, d->valid ? d->py : 0); text(476, 240, s, GREEN); break;
    case 6:
        snprintf(s, sizeof(s), "模拟时延%4ums     ", d->online ? d->delay : 0); text(476, 270, s, FG); break;
    case 7:
        snprintf(s, sizeof(s), "虚拟H%5d V%5d     ", d->horizontal, d->vertical); text(476, 300, s, FG); break;
    case 8:
        snprintf(s, sizeof(s), "RX%7lu TX%7lu ", (unsigned long)d->accepted, (unsigned long)board_diag.tx_frames); text(476, 330, s, FG); break;
    case 9:
        snprintf(s, sizeof(s), "CRC%5lu ERR%5lu    ", (unsigned long)p->crc_errors, (unsigned long)(d->rejected + board_diag.uart_errors)); text(476, 360, s, AMBER); break;
    case 10:
        if (old_x >= 0) { cross(old_x, old_y, BG); cross(old_px, old_py, BG); }
        border(); old_x = -1;
        if (d->valid || local) {
            uint32_t t = (now / 20) % 200;
            uint16_t lx = (uint16_t)((t < 100 ? t : 200 - t) * 90 + 500), ly = (uint16_t)(2000 + (now / 20 % 80) * 70);
            old_x = 30 + (int)((uint32_t)(local ? lx : d->x) * 410 / 10000); old_y = 166 + (int)((uint32_t)(local ? ly : d->y) * 222 / 10000);
            old_px = 30 + (int)((uint32_t)(local ? lx : d->px) * 410 / 10000); old_py = 166 + (int)((uint32_t)(local ? ly : d->py) * 222 / 10000);
            cross(old_x, old_y, AMBER); cross(old_px, old_py, GREEN);
        }
        break;
    case 11:
    case 12:
        Lcd_Rect(476, 400 + (row - 12) * 24, 312, 24, BG);
        text(476, 400 + (row - 12) * 24, events[(event_head + 3 - (row - 12)) % 4], FG); break;
    case 13:
        snprintf(s, sizeof(s), "F%u TIM%7lu OV%4lu ", d->fault, (unsigned long)board_diag.timer_irqs, (unsigned long)board_diag.rx_overflow); text(476, 454, s, FG); break;
    default: row = 0; break;
    }
}
