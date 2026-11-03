"""C firmware simulation and nonblocking pyserial connection with safe fallback."""
from pathlib import Path
import ctypes
import os
import subprocess
import sys
import struct
import secrets
from .protocol import Frame, Parser, HELLO

ROOT = Path(__file__).resolve().parents[1]


def build_native():
    lib = ROOT / 'build' / 'native' / ('avt_sim.dll' if os.name == 'nt' else 'libavt_sim.so')
    sources = [ROOT / 'User/Src/avt_protocol.c', ROOT / 'User/Src/avt_core.c', ROOT / 'tests/native_bridge.c']
    headers = list((ROOT / 'User/Inc').glob('avt_*.h'))
    if not lib.exists() or any(p.stat().st_mtime > lib.stat().st_mtime for p in sources + headers):
        lib.parent.mkdir(parents=True, exist_ok=True)
        try:
            subprocess.run(['gcc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-shared', '-I' + str(ROOT / 'User/Inc'), *map(str, sources), '-o', str(lib)], check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as e:
            raise OSError(f'Native C simulator build failed: {e.stderr}') from e
    return lib


class Simulated:
    label = 'SIMULATED C MCU / NOT HARDWARE'

    def __init__(self):
        self.lib = ctypes.CDLL(str(build_native()))
        self.lib.sim_feed.argtypes = [ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint32]
        self.lib.sim_step.argtypes = [ctypes.c_uint32]
        self.lib.sim_init.argtypes = [ctypes.c_uint32]
        self.lib.sim_read.argtypes = [ctypes.c_void_p]
        self.lib.sim_read.restype = ctypes.c_uint
        self.lib.sim_init(0)

    def send(self, data, now):
        self.lib.sim_feed(data, len(data), int(now) & 0xFFFFFFFF)

    def read(self, now):
        self.lib.sim_step(int(now) & 0xFFFFFFFF)
        self.lib.sim_status(int(now) & 0xFFFFFFFF)
        buf = ctypes.create_string_buffer(4096)
        n = self.lib.sim_read(buf)
        return buf.raw[:n]

    def close(self):
        pass


class SerialTransport:
    def __init__(self, port):
        import serial
        # 显式关闭 DTR/RTS，避免板载一键下载电路被串口监视器意外驱动。
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=0.05)
        self.serial.dtr = False
        self.serial.rts = False
        self.serial.port = port
        self.serial.open()
        self.label = f'SERIAL {port} / WAITING FOR MCU'

    def send(self, data, now):
        if self.serial.write(data) != len(data):
            raise OSError('serial write incomplete')

    def read(self, now):
        return self.serial.read(min(4096, self.serial.in_waiting))

    def close(self):
        self.serial.close()


class Link:
    def __init__(self, backend='simulated', port=None, session=None):
        self.requested_backend = backend
        self.port = port
        self.session = session if session is not None else secrets.randbits(32) or 1
        self.error = ''
        self.parser = Parser()
        self.status = None
        self.last_rx = None
        self.last_hello = -1000
        self.hello = False
        self.rtt_ms = None
        self.pings = {}
        try:
            self.transport = SerialTransport(port) if backend == 'serial' else Simulated()
        except (OSError, ValueError) as e:
            self.error = f'Serial unavailable ({e}); switched to simulated C MCU'
            print(self.error, file=sys.stderr)
            self.transport = Simulated()

    def fallback(self, error):
        self.transport.close()
        self.transport = Simulated()
        self.hello = False
        self.status = None
        self.last_rx = None
        self.last_hello = -1000
        self.error = f'Serial disconnected ({error}); switched to simulated C MCU'
        print(self.error, file=sys.stderr)

    def send(self, data, now):
        try:
            self.transport.send(data, now)
        except (OSError, ValueError) as e:
            self.fallback(e)

    def poll(self, now):
        from .protocol import HELLO_ACK, PONG, status_dict
        if not self.hello and now - self.last_hello >= 500:
            self.last_hello = now
            self.send(Frame(HELLO, 0, int(now), struct.pack('<I', self.session)).encode(), now)
        try:
            data = self.transport.read(now)
        except (OSError, ValueError) as e:
            self.fallback(e)
            data = b''
        for frame in self.parser.feed(data, int(now)):
            if frame.kind == HELLO_ACK and frame.payload == struct.pack('<I', self.session):
                self.hello = True
            if frame.kind == PONG and len(frame.payload) == 4:
                token = struct.unpack('<I', frame.payload)[0]
                if token in self.pings:
                    self.rtt_ms = now - self.pings.pop(token)
            result = status_dict(frame)
            if result is not None:
                self.status = result
                self.last_rx = now
        if self.last_rx is not None and now - self.last_rx > 1000:
            self.status = None
            self.hello = False
        return self.status

    def ping(self, now):
        from .protocol import PING
        token = int(now) & 0xFFFFFFFF
        self.pings = {k: v for k, v in self.pings.items() if now - v < 2000}
        self.pings[token] = now
        self.send(Frame(PING, token, token, struct.pack('<I', token)).encode(), now)

    def close(self):
        self.transport.close()
