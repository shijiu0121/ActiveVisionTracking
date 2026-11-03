#ifndef AVT_RUNTIME_H
#define AVT_RUNTIME_H
#include "avt_core.h"
extern AvtDevice avt_device;
extern AvtParser avt_parser;
extern volatile uint32_t avt_tasks, avt_max_loop_ms;
void AVT_Init(void);
void AVT_Task(void);
#endif
