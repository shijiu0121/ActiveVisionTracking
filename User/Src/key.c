
#include "key.h"
#include "main.h"
#include <stdint.h>

/* 按键时间参数 */
#define KEY0_DEBOUNCE_MS    20U
#define KEY0_LONG_PRESS_MS  1000U

/* 消抖状态 */
static bool last_raw_pressed = false;
static bool stable_pressed = false;
static uint32_t last_change_tick = 0;

/* 长短按检测状态 */
static uint32_t press_tick = 0;
static bool long_reported = false;

/* 读取按键原始状态 */
bool KEY0_IsPressed(void) {
    return HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET;
}

/* 初始化按键模块 */
void KEY0_Init(void) {
    bool pressed = KEY0_IsPressed();
    uint32_t now = HAL_GetTick();

    last_raw_pressed = pressed;
    stable_pressed = pressed;
    last_change_tick = now;

    press_tick = now;
    long_reported = false;
}

/* 非阻塞消抖与长短按事件检测 */
KeyEvent_t KEY0_GetEvent(void) {
    bool raw_pressed = KEY0_IsPressed();
    uint32_t now = HAL_GetTick();

    /* 1. 检测原始电平变化 */
    if (raw_pressed != last_raw_pressed) {
        last_raw_pressed = raw_pressed;
        last_change_tick = now;
    }

    /* 2. 确认电平稳定达到消抖时间 */
    if ((uint32_t)(now - last_change_tick) >= KEY0_DEBOUNCE_MS) {
        if (raw_pressed != stable_pressed) {
            stable_pressed = raw_pressed;

            if (stable_pressed) {
                /* 确认按下，开始计时 */
                press_tick = now;
                long_reported = false;
            } else {
                /* 确认松开，判断是否为短按 */
                if (!long_reported) {
                    return KEY_EVENT_SHORT;
                }
            }
        }
    }

    /* 3. 长按检测 */
    if (stable_pressed && raw_pressed && !long_reported) {
        if ((uint32_t)(now - press_tick) >= KEY0_LONG_PRESS_MS) {
            long_reported = true;
            return KEY_EVENT_LONG;
        }
    }

    return KEY_EVENT_NONE;
}
