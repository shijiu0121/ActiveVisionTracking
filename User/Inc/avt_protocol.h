#ifndef AVT_PROTOCOL_H
#define AVT_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define AVT_MAX_PAYLOAD 48U
#define AVT_HEADER 12U
#define AVT_MAX_FRAME (AVT_HEADER + AVT_MAX_PAYLOAD + 2U)
#define AVT_TARGET 1U
#define AVT_PING 2U
#define AVT_HELLO 3U
#define AVT_STATUS 0x81U
#define AVT_PONG 0x82U
#define AVT_HELLO_ACK 0x83U
typedef struct {
    uint8_t type;
    uint16_t len, seq;
    uint32_t stamp;
    uint8_t payload[AVT_MAX_PAYLOAD];
} AvtFrame;
typedef struct {
    uint8_t data[AVT_MAX_FRAME];
    uint16_t used;
    uint32_t last_byte, crc_errors, format_errors, frames;
} AvtParser;
uint16_t Avt_Get16(const uint8_t *p);
uint32_t Avt_Get32(const uint8_t *p);
void Avt_Put16(uint8_t *p, uint16_t v);
void Avt_Put32(uint8_t *p, uint32_t v);
uint16_t Avt_Crc(const uint8_t *p, size_t n);
size_t Avt_Encode(const AvtFrame *f, uint8_t *out);
bool Avt_ParseByte(AvtParser *p, uint8_t b, uint32_t now, AvtFrame *out);
#endif
