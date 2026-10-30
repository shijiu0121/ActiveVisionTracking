
#ifndef KEY_H
#define KEY_H

#include <stdbool.h>

/* 读取当前原始按键状态 */
bool KEY0_IsPressed(void);

/* 初始化按键检测模块 */
void KEY0_Init(void);

/* 检测一次有效按下事件 */
bool KEY0_GetPressEvent(void);

#endif
