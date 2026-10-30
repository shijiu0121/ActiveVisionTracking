
#include "key.h"
#include "main.h"
#include <stdint.h>

#define KEY0_DEBOUNCE_MS 20U

/* 上一次采样得到的原始状态 */
static bool last_raw_pressed = false;

/* 已经过消抖确认的稳定状态 */
static bool stable_pressed = false;

/* 原始状态最近一次变化的时间 */
static uint32_t last_change_tick = 0;

/* 读取按键原始电平 */
bool KEY0_IsPressed(void)
{
    return HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin)
           == GPIO_PIN_RESET;
}

/* 初始化按键状态 */
void KEY0_Init(void)
{
    bool pressed = KEY0_IsPressed();

    last_raw_pressed = pressed;
    stable_pressed = pressed;
    last_change_tick = HAL_GetTick();
}

/* 非阻塞消抖，产生按下事件 */
bool KEY0_GetPressEvent(void)
{
    bool raw_pressed = KEY0_IsPressed();
    uint32_t now = HAL_GetTick();

    /* 检测原始电平是否发生变化 */
    if (raw_pressed != last_raw_pressed)
    {
        last_raw_pressed = raw_pressed;
        last_change_tick = now;
    }

    /* 判断新状态是否已持续稳定 20ms */
    if ((uint32_t)(now - last_change_tick)
            >= KEY0_DEBOUNCE_MS)
    {
        if (raw_pressed != stable_pressed)
        {
            stable_pressed = raw_pressed;

            /* 只有确认按下时才产生事件 */
            if (stable_pressed)
            {
                return true;
            }
        }
    }

    return false;
}
