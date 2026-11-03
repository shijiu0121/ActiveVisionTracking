
#ifndef APP_H
#define APP_H

#include "key.h"
#include "avt_core.h"

/* 设备运行状态 */
typedef enum {
    APP_STATE_IDLE,
    APP_STATE_RUNNING,
    APP_STATE_PAUSED,
    APP_STATE_FAULT
} AppState_t;

/* 初始化应用 */
void App_Init(void);

/* 处理按键事件 */
void App_HandleKeyEvent(KeyEvent_t event);

/* 周期更新应用状态 */
void App_Update(void);

/* 获取当前设备状态 */
AppState_t App_GetState(void);

#endif
