#include "avt_core.h"
#include <string.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
static AvtDevice device;
static AvtParser parser;
static uint8_t output[4096];
static size_t output_n;
static void push(const AvtFrame *f) {
    uint8_t bytes[AVT_MAX_FRAME]; size_t n = Avt_Encode(f, bytes);
    if (output_n + n <= sizeof(output)) { memcpy(output + output_n, bytes, n); output_n += n; }
}
EXPORT void sim_init(uint32_t now) { Avt_DeviceInit(&device, now); Avt_SetState(&device, AVT_RUNNING); memset(&parser, 0, sizeof(parser)); output_n = 0; }
EXPORT void sim_state(unsigned state) { if (state <= AVT_FAULT) Avt_SetState(&device, (AvtState)state); }
EXPORT void sim_feed(const uint8_t *p, unsigned n, uint32_t now) {
    AvtFrame frame, reply;
    for (unsigned i = 0; i < n; i++) if (Avt_ParseByte(&parser, p[i], now, &frame)) if (Avt_Receive(&device, &frame, now, &reply)) push(&reply);
}
EXPORT void sim_step(uint32_t now) { Avt_Step(&device, now); }
EXPORT void sim_status(uint32_t now) { AvtFrame f; Avt_MakeStatus(&device, &parser, now, 0, &f); push(&f); }
EXPORT unsigned sim_read(uint8_t *p) { unsigned n = (unsigned)output_n; memcpy(p, output, n); output_n = 0; return n; }
EXPORT unsigned sim_crc(const uint8_t *p, unsigned n) { return Avt_Crc(p, n); }
EXPORT unsigned sim_encode(unsigned type, unsigned seq, uint32_t stamp, const uint8_t *p, unsigned n, uint8_t *out) {
    if (n > AVT_MAX_PAYLOAD) return 0;
    AvtFrame f = {0}; f.type = type; f.seq = seq; f.stamp = stamp; f.len = n; memcpy(f.payload, p, n); return (unsigned)Avt_Encode(&f, out);
}
