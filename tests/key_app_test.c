#include "app.h"
#include "avt_runtime.h"
#include "stm32f1xx_hal.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
AvtDevice avt_device;
static uint32_t tick;
static GPIO_PinState key = GPIO_PIN_SET, led = GPIO_PIN_SET;
uint32_t HAL_GetTick(void) { return tick; }
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin) { (void)port; (void)pin; return key; }
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state) { (void)port; (void)pin; led = state; }
void HAL_GPIO_TogglePin(void *port, uint16_t pin) { (void)port; (void)pin; led = !led; }
bool UI_HandleKey(KeyEvent_t e) { (void)e; return false; }
void UI_Event(uint32_t now, const char *s) { (void)now; (void)s; }
static KeyEvent_t sample(uint32_t now, GPIO_PinState value) { tick = now; key = value; return KEY0_GetEvent(); }
int main(void) {
    KEY0_Init(); App_Init(); assert(App_GetState() == APP_STATE_IDLE && led == GPIO_PIN_SET);
    assert(sample(1, GPIO_PIN_RESET) == KEY_EVENT_NONE);
    assert(sample(10, GPIO_PIN_SET) == KEY_EVENT_NONE);
    assert(sample(15, GPIO_PIN_RESET) == KEY_EVENT_NONE);
    assert(sample(34, GPIO_PIN_RESET) == KEY_EVENT_NONE);
    assert(sample(35, GPIO_PIN_RESET) == KEY_EVENT_NONE);
    assert(sample(100, GPIO_PIN_SET) == KEY_EVENT_NONE);
    assert(sample(119, GPIO_PIN_SET) == KEY_EVENT_NONE);
    assert(sample(120, GPIO_PIN_SET) == KEY_EVENT_SHORT);
    App_HandleKeyEvent(KEY_EVENT_SHORT); assert(App_GetState() == APP_STATE_RUNNING && led == GPIO_PIN_RESET);
    App_HandleKeyEvent(KEY_EVENT_SHORT); assert(App_GetState() == APP_STATE_PAUSED);
    tick += 499; App_Update(); assert(led == GPIO_PIN_SET);
    tick++; App_Update(); assert(led == GPIO_PIN_RESET);
    App_HandleKeyEvent(KEY_EVENT_SHORT); assert(App_GetState() == APP_STATE_RUNNING);
    sample(1000, GPIO_PIN_RESET); sample(1020, GPIO_PIN_RESET);
    assert(sample(2019, GPIO_PIN_RESET) == KEY_EVENT_NONE);
    assert(sample(2020, GPIO_PIN_RESET) == KEY_EVENT_LONG);
    assert(sample(3020, GPIO_PIN_RESET) == KEY_EVENT_NONE);
    sample(3021, GPIO_PIN_SET); assert(sample(3041, GPIO_PIN_SET) == KEY_EVENT_NONE);
    App_HandleKeyEvent(KEY_EVENT_LONG); assert(App_GetState() == APP_STATE_IDLE);
    tick = 0xfffffff0U; key = GPIO_PIN_SET; KEY0_Init();
    sample(0xfffffff5U, GPIO_PIN_RESET); sample(9, GPIO_PIN_RESET);
    sample(20, GPIO_PIN_SET); assert(sample(40, GPIO_PIN_SET) == KEY_EVENT_SHORT);
    tick = 0; key = GPIO_PIN_RESET; KEY0_Init();
    assert(sample(1000, GPIO_PIN_RESET) == KEY_EVENT_LONG);
    puts("KEY0/app: debounce, bounce, short, long-once, pause LED, tick wrap, boot-held PASS");
    return 0;
}
