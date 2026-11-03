#ifndef AVT_UI_H
#define AVT_UI_H
#include "avt_core.h"
#include "key.h"
void UI_Init(uint32_t now, bool selftest);
void UI_Task(uint32_t now, const AvtDevice *d, const AvtParser *p);
bool UI_HandleKey(KeyEvent_t event);
void UI_Event(uint32_t now, const char *message);
#endif
