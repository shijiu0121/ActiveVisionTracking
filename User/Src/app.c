
#include "app.h"
#include "led.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include "avt_runtime.h"
#include "ui.h"

#define APP_BLINK_INTERVAL_MS 500U

/* 当前设备状态 */
static AppState_t current_state = APP_STATE_IDLE;

/* 上次 LED 闪烁切换时间 */
static uint32_t last_blink_tick = 0U;

/* 内部函数：进入指定状态 */
static void App_SetState(AppState_t new_state) {
    current_state = new_state;
    last_blink_tick = HAL_GetTick();
    Avt_SetState(&avt_device, (AvtState)new_state);

    if (current_state == APP_STATE_RUNNING) {
        LED_On();
    } else {
        LED_Off();
    }
}

/* 初始化设备 */
void App_Init(void) {
    App_SetState(avt_device.fault == 2 ? APP_STATE_FAULT : APP_STATE_IDLE);
}

/* 处理按键事件 */
void App_HandleKeyEvent(KeyEvent_t event) {
    if (UI_HandleKey(event)) return;
    if (event == KEY_EVENT_LONG) {
        App_SetState(APP_STATE_IDLE);
    } else if (event == KEY_EVENT_SHORT) {
        if (current_state == APP_STATE_IDLE) {
            App_SetState(APP_STATE_RUNNING);
        } else if (current_state == APP_STATE_RUNNING) {
            App_SetState(APP_STATE_PAUSED);
        } else if (current_state == APP_STATE_PAUSED) {
            App_SetState(APP_STATE_RUNNING);
        }
    }
    if (event != KEY_EVENT_NONE) UI_Event(HAL_GetTick(), current_state == APP_STATE_IDLE ? "按键：进入待机" : current_state == APP_STATE_RUNNING ? "按键：开始运行" : "按键：暂停运行");
}

/* 更新需要周期处理的功能 */
void App_Update(void) {
    if (current_state == APP_STATE_PAUSED) {
        uint32_t now = HAL_GetTick();

        if ((uint32_t)(now - last_blink_tick) >= APP_BLINK_INTERVAL_MS) {
            last_blink_tick = now;
            LED_Toggle();
        }
    }
}

/* 返回当前状态 */
AppState_t App_GetState(void) {
    return current_state;
}
