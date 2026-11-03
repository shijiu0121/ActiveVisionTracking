"""Run: python -m host --headless --frames 600 --output runs/demo"""
import argparse
import csv
import heapq
import json
from pathlib import Path
import time
import sys
from functools import lru_cache
import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from .protocol import Frame, TARGET, target_payload
from .transport import Link
from .vision import Tracker, Synthetic, STATES


class DelayModel:
    def __init__(self, delay_ms=120, jitter_ms=20, loss=0.03, seed=7):
        self.delay, self.jitter, self.loss = delay_ms, jitter_ms, loss
        self.rng = np.random.default_rng(seed)
        self.queue = []
        self.sent = self.dropped = 0

    def sample(self):
        return max(0, min(450, self.delay + self.rng.uniform(-self.jitter, self.jitter)))

    def push(self, now, delay, item):
        self.sent += 1
        if self.rng.random() < self.loss:
            self.dropped += 1
        else:
            heapq.heappush(self.queue, (now + delay, self.sent, item))

    def ready(self, now):
        while self.queue and self.queue[0][0] <= now:
            yield heapq.heappop(self.queue)


@lru_cache(maxsize=4)
def ui_font(size):
    for path in ('C:/Windows/Fonts/msyh.ttc', '/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'):
        if Path(path).is_file():
            return ImageFont.truetype(path, size)
    raise ValueError('Chinese UI font unavailable; install Noto Sans CJK or use Windows Microsoft YaHei')


def annotate(image, tracker, prediction, truth, now, link, metrics, synthetic, compensate):
    canvas = np.full((610, 1000, 3), (22, 19, 16), np.uint8)
    h, w = image.shape[:2]
    display = cv2.resize(image, (640, 360))
    if len(tracker.history) >= 2:
        trail = np.rint(np.array(tracker.history) * [640 / w, 360 / h]).astype(np.int32)
        cv2.polylines(display, [trail], False, (100, 120, 160), 1, cv2.LINE_AA)
    def point(p, color, radius=7):
        if p is not None:
            q = np.rint(np.asarray(p) * [640 / w, 360 / h]).astype(int)
            cv2.circle(display, tuple(q), radius, color, 2)
    point(truth, (255, 255, 255), 12)
    point(tracker.observation, (0, 220, 255), 7)
    if tracker.valid:
        point(prediction, (255, 230, 50), 9)
    cv2.drawMarker(display, (320, 180), (160, 160, 160), cv2.MARKER_CROSS, 20, 1)
    canvas[80:440, 20:660] = display
    picture = Image.fromarray(cv2.cvtColor(canvas, cv2.COLOR_BGR2RGB))
    draw = ImageDraw.Draw(picture)
    def line(x, y, value, color=(220, 220, 220), scale=0.57):
        size = round(scale * 30)
        draw.text((x, y-size), value, font=ui_font(size), fill=tuple(reversed(color)))
    line(20, 36, '主动视觉跟踪系统  ActiveVisionTracking', (255, 230, 50), 0.85)
    line(20, 62, '合成视频演示 / 仅虚拟控制，无实体电机' if synthetic else '本地视频 / 颜色目标跟踪', (0, 220, 255))
    state_names = ('正在跟踪', '短时遮挡预测', '目标丢失 / 已停止', '正在重新捕获')
    line(690, 100, state_names[tracker.state], (80, 230, 150), 0.75)
    line(690, 134, f'置信度：{tracker.confidence:.2f}')
    line(690, 164, f'目标有效：{"是" if tracker.valid else "否"}')
    line(690, 194, f'时延补偿：{"开启" if compensate else "关闭"}')
    if tracker.x is not None:
        line(690, 224, f'坐标：{tracker.x[0]:.0f}, {tracker.x[1]:.0f}')
        line(690, 254, f'速度：{tracker.x[2]:.0f}, {tracker.x[3]:.0f} px/s')
        line(690, 284, f'水平偏差：{(tracker.x[0]-w/2)/(w/2):+.2f}')
    status = link.status or {}
    line(690, 330, f'MCU收包：{status.get("accepted", 0)}')
    line(690, 360, f'虚拟水平控制：{status.get("horizontal", 0):+d}')
    line(690, 390, f'虚拟垂直控制：{status.get("vertical", 0):+d}')
    if synthetic:
        line(690, 420, f'无补偿 MAE：{metrics.get("baseline_mae", 0):.2f} px')
        line(690, 450, f'有补偿 MAE：{metrics.get("compensated_mae", 0):.2f} px')
    line(20, 474, '白圈：目标真值    黄圈：图像检测    青圈：预计执行时刻位置')
    label = 'C固件核心软件仿真 / 未连接真实开发板' if link.transport.label.startswith('SIMULATED') else link.transport.label
    line(20, 505, label[:95], (120, 220, 140))
    line(20, 535, f'时间 {now/1000:.2f}s  |  注入时延 {metrics["delay_ms"]:.0f}ms  |  图像处理 {metrics["processing_ms"]:.1f}ms')
    line(20, 565, f'往返时间 RTT：{link.rtt_ms:.1f}ms（非单程时延）' if link.rtt_ms is not None else '往返时间：等待回包；未对不同设备时钟直接相减')
    line(20, 595, 'Q / ESC：退出    P：切换时延补偿    R：重试串口连接', (150, 150, 150))
    return cv2.cvtColor(np.asarray(picture), cv2.COLOR_RGB2BGR)


def run(args):
    if not 0 <= args.loss < 1 or not 0 <= args.delay_ms <= 450 or not 0 <= args.jitter_ms <= 200 or not 1 <= args.fps <= 120:
        raise ValueError('loss must be [0,1); delay [0,450] ms; jitter [0,200] ms; fps [1,120]')
    if args.backend == 'serial' and not args.port:
        raise ValueError('--backend serial requires --port COMx; use --list-ports to inspect')
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    source = Synthetic(args.seed)
    capture = cv2.VideoCapture(args.video) if args.video else None
    if capture is not None and not capture.isOpened():
        raise ValueError(f'Cannot open video: {args.video}')
    tracker = Tracker(roi=tuple(args.roi) if args.roi else None)
    link = Link(args.backend, args.port)
    delay = DelayModel(args.delay_ms, args.jitter_ms, args.loss, args.seed)
    fields = ('time_ms', 'state', 'valid', 'confidence', 'x', 'y', 'vx', 'vy', 'truth_x', 'truth_y', 'pred_x', 'pred_y', 'injected_delay_ms', 'processing_ms', 'mcu_accepted', 'mcu_horizontal', 'mcu_vertical', 'backend')
    counts = {state: 0 for state in STATES}
    errors = [[], []]
    durations = []
    start = time.perf_counter()
    compensate = not args.no_compensation
    realtime = not args.headless or args.backend == 'serial'
    frame_count = args.frames or (600 if args.headless else 1800)
    final_canvas = None
    try:
        with (output / 'trace.csv').open('w', newline='', encoding='utf-8') as log:
            writer = csv.DictWriter(log, fieldnames=fields)
            writer.writeheader()
            for i in range(frame_count):
                t = i / args.fps
                if realtime:
                    wait = start + t - time.perf_counter()
                    if wait > 0:
                        time.sleep(wait)
                    t = time.perf_counter() - start
                now = int(round(t * 1000))
                link.poll(now)
                if i % max(1, round(args.fps)) == 0:
                    link.ping(now)
                if capture is None:
                    image = source.frame(t)
                    truth = source.truth(t)
                else:
                    ok, image = capture.read()
                    if not ok:
                        break
                    truth = None
                h, w = image.shape[:2]
                tick = time.perf_counter()
                tracker.update(image, t)
                latency = delay.sample()
                xy = tracker.predict(0, w, h)
                prediction = tracker.predict(latency / 1000, w, h)
                durations.append((time.perf_counter() - tick) * 1000)
                counts[STATES[tracker.state]] += 1
                item = dict(seq=i, stamp=now, state=tracker.state, valid=tracker.valid, confidence=tracker.confidence,
                            xy=xy.copy(), pred=prediction.copy(), delay=latency, compensate=compensate, size=(w, h))
                delay.push(now, latency, item)
                for due, _, item in delay.ready(now):
                    age = now - item['stamp']
                    if age >= 500:
                        continue
                    selected = item['pred'] if item['compensate'] else item['xy']
                    size = np.array(item['size'], float) - 1
                    payload = target_payload(link.session, item['state'], item['confidence'], item['xy']/size, selected/size,
                                             delay=round(item['delay']), age=age, valid=item['valid'], synthetic=capture is None)
                    if link.hello:
                        link.send(Frame(TARGET, item['seq'], item['stamp'], payload).encode(), now)
                    if capture is None and item['valid']:
                        actual = source.truth(now / 1000)
                        errors[0].append(float(np.linalg.norm(item['xy'] - actual)))
                        errors[1].append(float(np.linalg.norm(item['pred'] - actual)))
                link.poll(now)
                status = link.status or {}
                model = tracker.x if tracker.x is not None else np.zeros(4)
                writer.writerow(dict(zip(fields, (now, STATES[tracker.state], tracker.valid, tracker.confidence, *model,
                                                   *(truth if truth is not None else [None, None]), *prediction, latency,
                                                   durations[-1], status.get('accepted', 0), status.get('horizontal', 0),
                                                   status.get('vertical', 0), link.transport.label))))
                if i % 30 == 0 or not args.headless or i in (100, frame_count - 1):
                    final_canvas = annotate(image, tracker, prediction if compensate else xy, truth, now, link,
                                            {'delay_ms': latency, 'processing_ms': durations[-1],
                                             'baseline_mae': float(np.mean(errors[0])) if errors[0] else 0,
                                             'compensated_mae': float(np.mean(errors[1])) if errors[1] else 0}, capture is None, compensate)
                    if i == 100:
                        cv2.imwrite(str(output / 'occlusion.png'), final_canvas)
                    if not args.headless:
                        cv2.imshow('ActiveVisionTracking - software demonstration', final_canvas)
                        key = cv2.waitKey(1) & 255
                        if key in (27, ord('q')):
                            break
                        if key == ord('p'):
                            compensate = not compensate
                        if key == ord('r') and args.backend == 'serial':
                            link.close()
                            link = Link(args.backend, args.port, session=link.session)
        if final_canvas is not None:
            cv2.imwrite(str(output / 'preview.png'), final_canvas)
        summary = dict(frames=sum(counts.values()), states=counts, injected_delay_ms=args.delay_ms, jitter_ms=args.jitter_ms,
                       sent=delay.sent, simulated_dropped=delay.dropped, evaluated_arrivals=len(errors[0]),
                       uncompensated_mae_px=float(np.mean(errors[0])) if errors[0] else None,
                       compensated_mae_px=float(np.mean(errors[1])) if errors[1] else None,
                       processing_p95_ms=float(np.percentile(durations, 95)) if durations else None,
                       backend=link.transport.label, mcu_status=link.status, error=link.error,
                       hardware_verified=False, note='Paired arrival-time position errors on synthetic truth; not mechanical control accuracy.')
        (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
        print(json.dumps(summary, indent=2))
        return summary
    finally:
        link.close()
        if capture is not None:
            capture.release()
        if not args.headless:
            cv2.destroyAllWindows()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backend', choices=('simulated', 'serial'), default='simulated')
    parser.add_argument('--port')
    parser.add_argument('--list-ports', action='store_true')
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--fps', type=float, default=30)
    parser.add_argument('--seed', type=int, default=7)
    parser.add_argument('--delay-ms', type=float, default=120)
    parser.add_argument('--jitter-ms', type=float, default=20)
    parser.add_argument('--loss', type=float, default=.03)
    parser.add_argument('--no-compensation', action='store_true')
    parser.add_argument('--video', help='Local video; default detector tracks green objects')
    parser.add_argument('--roi', nargs=4, type=int, metavar=('X', 'Y', 'W', 'H'), help='First-frame colored target ROI for hue calibration')
    parser.add_argument('--output', default='runs/demo')
    args = parser.parse_args()
    if args.list_ports:
        from serial.tools.list_ports import comports
        for port in comports():
            print(port.device, port.description, port.hwid)
        return
    try:
        run(args)
    except (ValueError, OSError) as e:
        print(f'Demo error: {e}', file=sys.stderr)
        raise SystemExit(2) from e


if __name__ == '__main__':
    main()
