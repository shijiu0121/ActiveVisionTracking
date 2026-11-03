"""Wire format shared with User/Src/avt_protocol.c. All integers little endian."""
from dataclasses import dataclass
import struct

TARGET, PING, HELLO = 1, 2, 3
STATUS, PONG, HELLO_ACK = 0x81, 0x82, 0x83
HEADER = struct.Struct('<2sBBHHI')
TARGET_PAYLOAD = struct.Struct('<IBB9H')
STATUS_PAYLOAD = struct.Struct('<BBH5IhhHH')


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
    return crc


@dataclass
class Frame:
    kind: int
    seq: int
    stamp: int
    payload: bytes = b''

    def encode(self):
        if len(self.payload) > 48:
            raise ValueError('payload exceeds 48 bytes')
        data = HEADER.pack(b'\xa5\x5a', 1, self.kind, len(self.payload), self.seq & 65535, self.stamp & 0xFFFFFFFF) + self.payload
        return data + struct.pack('<H', crc16(data[2:]))


class Parser:
    def __init__(self):
        self.buffer = bytearray()
        self.crc_errors = self.format_errors = 0
        self.last_ms = None

    def feed(self, data, now_ms=0):
        if self.buffer and self.last_ms is not None and ((now_ms - self.last_ms) & 0xFFFFFFFF) > 100:
            self.buffer.clear()
            self.format_errors += 1
        self.last_ms = now_ms
        frames = []
        self.buffer.extend(data)
        while len(self.buffer) >= 2:
            if self.buffer[:2] != b'\xa5\x5a':
                del self.buffer[0]
                continue
            if len(self.buffer) < 6:
                break
            length = struct.unpack_from('<H', self.buffer, 4)[0]
            if self.buffer[2] != 1 or length > 48:
                del self.buffer[0]
                self.format_errors += 1
                continue
            total = 14 + length
            if len(self.buffer) < total:
                break
            packet = bytes(self.buffer[:total])
            if crc16(packet[2:-2]) != struct.unpack_from('<H', packet, total - 2)[0]:
                self.crc_errors += 1
                del self.buffer[0]
                continue
            _, _, kind, _, seq, stamp = HEADER.unpack_from(packet)
            frames.append(Frame(kind, seq, stamp, packet[12:-2]))
            del self.buffer[:total]
        return frames


def target_payload(session, state, confidence, xy, prediction, delay=120, ttl=500, age=0, synthetic=True, valid=True):
    values = [int(round(max(0, min(1, x)) * 10000)) for x in (*xy, *prediction)]
    return TARGET_PAYLOAD.pack(session, int(valid) | (2 if synthetic else 0), state,
                               int(max(0, min(1, confidence)) * 1000), *values, int(delay), ttl, int(age), 0)


def status_dict(frame):
    if frame.kind != STATUS or len(frame.payload) != STATUS_PAYLOAD.size:
        return None
    names = ('state', 'flags', 'fault', 'accepted', 'rejected', 'crc_errors', 'duplicates', 'timeouts', 'horizontal', 'vertical', 'last_seq', 'version')
    return dict(zip(names, STATUS_PAYLOAD.unpack(frame.payload)))
