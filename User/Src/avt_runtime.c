#include "avt_runtime.h"
#include "board_io.h"
#include "lcd_panel.h"
#include "ui.h"
#include "app.h"
#include "stm32f1xx_hal.h"
AvtDevice avt_device;
AvtParser avt_parser;
volatile uint32_t avt_tasks, avt_max_loop_ms;
static uint32_t last_status, previous_tick;
static uint16_t status_seq;
static void send_frame(const AvtFrame *f) { uint8_t bytes[AVT_MAX_FRAME]; size_t n = Avt_Encode(f, bytes); if (n) Board_Send(bytes, n); }
void AVT_Init(void) {
    uint32_t now = HAL_GetTick(); previous_tick = now;
    Avt_DeviceInit(&avt_device, now); Board_Init(); Lcd_Init(now); UI_Init(now, KEY0_IsPressed());
    if (HAL_RCC_GetHCLKFreq() != 72000000U) { avt_device.fault = 2; Avt_SetState(&avt_device, AVT_FAULT); }
}
void AVT_Task(void) {
    uint32_t now = HAL_GetTick(), gap = now - previous_tick;
    previous_tick = now; if (gap > avt_max_loop_ms) avt_max_loop_ms = gap; avt_tasks++;
    AvtFrame f, reply; int b;
    bool was_online = avt_device.online;
    /* 每轮有界地处理收包，显示与按键同样得到运行机会。 */
    for (unsigned i = 0; i < 256 && (b = Board_ReadByte()) >= 0; i++) {
        if (Avt_ParseByte(&avt_parser, (uint8_t)b, now, &f)) if (Avt_Receive(&avt_device, &f, now, &reply)) send_frame(&reply);
    }
    Avt_Step(&avt_device, now);
    if (was_online != avt_device.online) UI_Event(now, avt_device.online ? "电脑已连接" : "通信超时：已归零");
    Lcd_Task(now);
    if (lcd_diag.error) avt_device.fault = 1;
    UI_Task(now, &avt_device, &avt_parser);
    if ((uint32_t)(now - last_status) >= 200) { last_status = now; Avt_MakeStatus(&avt_device, &avt_parser, now, status_seq++, &f); send_frame(&f); }
}
