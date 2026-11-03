#ifndef AVT_CORE_H
#define AVT_CORE_H
#include "avt_protocol.h"
typedef enum { AVT_IDLE, AVT_RUNNING, AVT_PAUSED, AVT_FAULT } AvtState;
typedef struct {
    uint32_t session, last_rx, last_step, accepted, rejected, duplicate, timeouts;
    uint16_t seq, x, y, px, py, confidence, delay, ttl, age;
    uint8_t flags, track;
    bool have_seq, valid, online;
    AvtState state;
    int16_t horizontal, vertical;
    uint16_t fault;
} AvtDevice;
void Avt_DeviceInit(AvtDevice *d, uint32_t now);
void Avt_SetState(AvtDevice *d, AvtState state);
bool Avt_Receive(AvtDevice *d, const AvtFrame *f, uint32_t now, AvtFrame *reply);
void Avt_Step(AvtDevice *d, uint32_t now);
void Avt_MakeStatus(const AvtDevice *d, const AvtParser *p, uint32_t now, uint16_t seq, AvtFrame *f);
#endif
