import ctypes
import random
import struct
import subprocess
from pathlib import Path
from types import SimpleNamespace
import numpy as np
import pytest
from host.protocol import Frame, Parser, crc16, target_payload, HELLO, TARGET, STATUS, status_dict
from host.transport import Simulated, Link, ROOT
from host.vision import Synthetic, Tracker
from host.__main__ import run


def target(seq=1, age=0, **kwargs):
    return Frame(TARGET, seq, 0, target_payload(123, 0, 0.9, (.7, .6), (.8, .65), age=age, **kwargs)).encode()


@pytest.fixture
def sim():
    s = Simulated()
    s.send(Frame(HELLO, 0, 0, struct.pack('<I', 123)).encode(), 0)
    return s


def status(sim, now):
    return [status_dict(f) for f in Parser().feed(sim.read(now), now) if f.kind == STATUS][-1]


def test_fixed_cross_language_vectors(sim):
    assert crc16(b'123456789') == 0x29B1
    sim.lib.sim_crc.argtypes = [ctypes.c_char_p, ctypes.c_uint]
    assert sim.lib.sim_crc(b'123456789', 9) == 0x29B1
    f = Frame(2, 0x1234, 0x01020304, b'\x44\x33\x22\x11')
    # 固定字节向量在文档中同样使用，时间戳不需要时钟同步。
    expected = bytes.fromhex('a55a0102040034120403020144332211dc6c')
    import binascii
    assert binascii.crc_hqx(expected[2:-2], 0xffff) == 0x6cdc
    assert f.encode() == expected
    output = ctypes.create_string_buffer(64)
    sim.lib.sim_encode.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_uint, ctypes.c_void_p]
    n = sim.lib.sim_encode(f.kind, f.seq, f.stamp, f.payload, len(f.payload), output)
    assert output.raw[:n] == expected


def test_parser_fragment_noise_crc_and_oversize(sim):
    packet = target()
    bad = bytearray(packet); bad[-1] ^= 0x40
    data = b'noise\xa5\xa5\x5a\x01\x01\xff\xff' + bad + packet + target(2)
    parser = Parser()
    frames = []
    rng = random.Random(4)
    pos = 0
    while pos < len(data):
        n = rng.randint(1, 7)
        chunk = data[pos:pos+n]
        frames.extend(parser.feed(chunk, 10))
        sim.send(chunk, 10)
        pos += n
    assert [f.seq for f in frames] == [1, 2]
    st = status(sim, 10)
    assert st['accepted'] == 2 and st['crc_errors'] == 1 and parser.crc_errors == 1


def test_half_frame_timeout(sim):
    packet = target()
    sim.send(packet[:9], 0); sim.send(packet, 101)
    assert status(sim, 101)['accepted'] == 1
    p = Parser(); p.feed(packet[:9], 0)
    assert len(p.feed(packet, 101)) == 1 and p.format_errors == 1


def test_duplicate_out_of_order_and_wrap(sim):
    for seq in (65534, 65535, 0, 0, 65533):
        sim.send(target(seq), 20)
    st = status(sim, 20)
    assert st['accepted'] == 3 and st['duplicates'] == 2 and st['last_seq'] == 0


@pytest.mark.parametrize('safe_state', [0, 2, 3])
def test_expiry_and_pause_safe(sim, safe_state):
    sim.send(target(age=300), 10)
    for now in range(20, 100, 10):
        sim.lib.sim_step(now)
    st = status(sim, 100)
    assert st['horizontal'] > 0
    sim.lib.sim_state(safe_state)
    assert status(sim, 110)['horizontal'] == 0
    sim.lib.sim_state(1)
    st = status(sim, 210)
    assert st['flags'] == 0 and st['horizontal'] == st['vertical'] == 0 and st['timeouts'] == 1


@pytest.mark.parametrize('change', ['expired', 'coords', 'version', 'session', 'confidence', 'reserved', 'type'])
def test_bad_fields_rejected(sim, change):
    payload = bytearray(target_payload(123, 0, .9, (.7,.6), (.8,.65)))
    kind = TARGET
    if change == 'expired': struct.pack_into('<H', payload, 20, 500)
    if change == 'coords': struct.pack_into('<H', payload, 8, 10001)
    if change == 'session': struct.pack_into('<I', payload, 0, 55)
    if change == 'confidence': struct.pack_into('<H', payload, 6, 1001)
    if change == 'reserved': struct.pack_into('<H', payload, 22, 1)
    if change == 'type': kind = 99
    packet = bytearray(Frame(kind, 1, 0, payload).encode())
    if change == 'version': packet[2] = 2
    sim.send(bytes(packet), 10)
    assert status(sim, 20)['accepted'] == 0


def test_lost_and_tick_wrap(sim):
    sim.send(target(), 0xfffffff0)
    for now in (0xfffffffa, 10, 20): sim.lib.sim_step(now)
    assert status(sim, 20)['flags'] == 3
    assert status(sim, 600)['flags'] == 0
    sim.send(target(2, valid=False), 601)
    assert status(sim, 602)['horizontal'] == 0


def test_reconnect_new_session(sim):
    sim.send(target(200), 0)
    sim.send(Frame(HELLO, 0, 1, struct.pack('<I', 123)).encode(), 1)
    sim.send(target(1), 1)
    assert status(sim, 1)['accepted'] == 1
    sim.send(Frame(HELLO, 0, 2, struct.pack('<I', 999)).encode(), 2)
    payload = target_payload(999, 0, .9, (.2,.3), (.2,.3))
    sim.send(Frame(TARGET, 0, 3, payload).encode(), 3)
    assert status(sim, 3)['accepted'] == 2


def test_image_only_occlusion_recovery():
    source = Synthetic(7); tracker = Tracker(); states = []
    for i in range(300):
        t = i / 30
        tracker.update(source.frame(t), t)
        states.append(tracker.state)
        if 7.7 < t < 8.7:
            assert not tracker.valid and tracker.state == 2
    assert set(states) == {0, 1, 2, 3}
    assert states[90] == 1 and states[110] == 0 and states[270] == 0
    tracker = Tracker()
    for i in range(30): tracker.update(np.zeros((360,640,3), np.uint8), i/30)
    assert not tracker.valid and tracker.x is None


def test_serial_missing_fallback():
    link = Link('serial', 'COM9999')
    link.poll(0)
    assert isinstance(link.transport, Simulated) and link.hello and link.error


def test_serial_disconnect_fallback():
    class Broken:
        def send(self, *args): raise OSError('device removed')
        def close(self): pass
    link = Link(); link.transport = Broken()
    link.send(b'x', 0); link.poll(1)
    assert isinstance(link.transport, Simulated) and link.hello


def test_end_to_end_and_delay_compensation(tmp_path):
    args = SimpleNamespace(loss=.03, delay_ms=120, jitter_ms=20, fps=30, backend='simulated', port=None,
                           output=tmp_path, video=None, seed=7, roi=None, no_compensation=False, headless=True, frames=360)
    result = run(args)
    assert result['frames'] == 360
    assert result['mcu_status']['accepted'] > 300
    assert result['mcu_status']['crc_errors'] == 0
    assert result['compensated_mae_px'] < result['uncompensated_mae_px'] * .7
    assert (tmp_path/'trace.csv').is_file() and (tmp_path/'preview.png').is_file()


def test_key_app_real_sources():
    out = ROOT/'build/native/key_app_test.exe'
    subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Itests/stubs', '-IUser/Inc',
                    'tests/key_app_test.c', 'User/Src/key.c', 'User/Src/app.c', 'User/Src/led.c', 'User/Src/avt_core.c', 'User/Src/avt_protocol.c', '-o', str(out)], check=True, cwd=ROOT)
    subprocess.run([str(out)], check=True)


def test_hmi_real_c_render(tmp_path):
    lib = ROOT/'build/native/ui_test.dll'
    subprocess.run(['gcc', '-std=c11', '-O2', '-shared', '-Wall', '-Wextra', '-Werror', '-IUser/Inc',
                    'User/Src/lcd_panel.c', 'User/Src/font_cn.c', 'User/Src/ui.c', 'tests/lcd_mock.c', '-o', str(lib)], check=True, cwd=ROOT)
    dll = ctypes.CDLL(str(lib)); dll.ui_pixels.restype = ctypes.POINTER(ctypes.c_uint16)
    for demo, test in ((0,0), (1,0), (0,1)):
        dll.ui_render(demo, test); dll.ui_boundary()
        assert dll.ui_violations() == 0
        assert dll.ui_missing_glyphs() == 0
        pixels = np.ctypeslib.as_array(dll.ui_pixels(), shape=(800*480,)).copy().reshape(480,800)
        assert len(np.unique(pixels)) >= 4
        # 原生 C 渲染的截图只用于布局检查，不冒充实物 LCD 照片。
        import cv2
        bgr = np.stack(((pixels&31)*255//31, ((pixels>>5)&63)*255//63, ((pixels>>11)&31)*255//31), axis=2).astype(np.uint8)
        cv2.imwrite(str(tmp_path/f'hmi_{demo}_{test}.png'), bgr)
