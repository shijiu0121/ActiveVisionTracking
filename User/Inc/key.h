
#ifndef KEY_H
#define KEY_H

#include <stdbool.h>

/* 按键事件类型 */
typedef enum {
    KEY_EVENT_NONE,
    KEY_EVENT_SHORT,
    KEY_EVENT_LONG
} KeyEvent_t;

/* 读取按键原始状态 */
bool KEY0_IsPressed(void);

/* 初始化按键检测 */
void KEY0_Init(void);

/* 获取按键事件 */
KeyEvent_t KEY0_GetEvent(void);

#endif
