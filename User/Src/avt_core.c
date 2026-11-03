#include "avt_core.h"
#include <string.h>
static int16_t limit(int32_t v) { return (int16_t)(v > 1000 ? 1000 : v < -1000 ? -1000 : v); }
void Avt_DeviceInit(AvtDevice *d, uint32_t now) { memset(d, 0, sizeof(*d)); d->last_step = now; }
void Avt_SetState(AvtDevice *d, AvtState state) {
    d->state = state;
    if (state != AVT_RUNNING) { d->horizontal = 0; d->vertical = 0; }
}
bool Avt_Receive(AvtDevice *d, const AvtFrame *f, uint32_t now, AvtFrame *reply) {
    if ((f->type == AVT_HELLO || f->type == AVT_PING) && f->len == 4) {
        *reply = *f; reply->type = f->type | 0x80; reply->stamp = now;
        if (f->type == AVT_HELLO) {
            uint32_t session = Avt_Get32(f->payload);
            if (!session) { d->rejected++; return false; }
            if (d->session != session) {
                d->session = session; d->have_seq = false; d->valid = false; d->online = false;
                d->horizontal = d->vertical = 0;
            }
        }
        return true;
    }
    if (f->type != AVT_TARGET || f->len != 24 || Avt_Get32(f->payload) != d->session || !d->session) { d->rejected++; return false; }
    const uint8_t *p = f->payload + 4;
    uint16_t conf = Avt_Get16(p + 2), ttl = Avt_Get16(p + 14), age = Avt_Get16(p + 16);
    if ((p[0] & ~3U) || p[1] > 3 || conf > 1000 || ttl < 50 || ttl > 1000 || age >= ttl || Avt_Get16(p + 18)) { d->rejected++; return false; }
    for (unsigned i = 4; i < 12; i += 2) if (Avt_Get16(p + i) > 10000) { d->rejected++; return false; }
    if (d->have_seq && ((uint16_t)(f->seq - d->seq) == 0 || (uint16_t)(f->seq - d->seq) >= 0x8000U)) { d->duplicate++; return false; }
    d->have_seq = true; d->seq = f->seq; d->last_rx = now; d->accepted++; d->online = true;
    d->flags = p[0]; d->track = p[1]; d->confidence = conf;
    d->x = Avt_Get16(p + 4); d->y = Avt_Get16(p + 6); d->px = Avt_Get16(p + 8); d->py = Avt_Get16(p + 10);
    d->delay = Avt_Get16(p + 12); d->ttl = ttl; d->age = age;
    d->valid = (p[0] & 1) && p[1] != 2 && conf >= 150;
    if (!d->valid) d->horizontal = d->vertical = 0;
    return false;
}
void Avt_Step(AvtDevice *d, uint32_t now) {
    if (d->online && (uint32_t)(now - d->last_rx) >= (uint32_t)(d->ttl - d->age)) {
        d->online = false; d->valid = false; d->timeouts++; d->horizontal = d->vertical = 0;
    }
    if (d->state != AVT_RUNNING || !d->valid || !d->online) { d->horizontal = d->vertical = 0; d->last_step = now; return; }
    if ((uint32_t)(now - d->last_step) < 10) return;
    d->last_step = now;
    /* 固定 10 ms 一阶平滑。仅为虚拟量，不配置任何 PWM 引脚。 */
    int16_t h = limit(((int32_t)d->px - 5000) / 5), v = limit(((int32_t)d->py - 5000) / 5);
    d->horizontal += (h - d->horizontal) / 4; d->vertical += (v - d->vertical) / 4;
}
void Avt_MakeStatus(const AvtDevice *d, const AvtParser *p, uint32_t now, uint16_t seq, AvtFrame *f) {
    memset(f, 0, sizeof(*f)); f->type = AVT_STATUS; f->len = 32; f->seq = seq; f->stamp = now;
    f->payload[0] = (uint8_t)d->state; f->payload[1] = (d->online ? 1 : 0) | (d->valid ? 2 : 0);
    Avt_Put16(f->payload + 2, d->fault); Avt_Put32(f->payload + 4, d->accepted);
    Avt_Put32(f->payload + 8, d->rejected); Avt_Put32(f->payload + 12, p->crc_errors);
    Avt_Put32(f->payload + 16, d->duplicate); Avt_Put32(f->payload + 20, d->timeouts);
    Avt_Put16(f->payload + 24, (uint16_t)d->horizontal); Avt_Put16(f->payload + 26, (uint16_t)d->vertical);
    Avt_Put16(f->payload + 28, d->seq); Avt_Put16(f->payload + 30, 0x0100);
}
