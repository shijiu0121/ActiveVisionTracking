"""Image-only color detector + constant velocity Kalman filter. No truth input."""
import math
from collections import deque
import cv2
import numpy as np

STATES = ('TRACKING', 'OCCLUDED', 'LOST', 'REACQUIRING')


class Synthetic:
    width, height = 640, 360

    def __init__(self, seed=7):
        self.rng = np.random.default_rng(seed)

    @staticmethod
    def truth(t):
        return np.array([320 + 220 * math.sin(0.62 * t) + 25 * math.sin(1.8 * t),
                         180 + 105 * math.sin(0.83 * t + 0.2)])

    @staticmethod
    def visibility(t):
        phase = t % 12
        if 3 <= phase < 3.55 or 7 <= phase < 8.8:
            return 'full occlusion'
        return 'partial occlusion' if 5 <= phase < 5.4 else 'visible'

    def frame(self, t):
        image = np.full((self.height, self.width, 3), (34, 28, 24), np.uint8)
        noise = self.rng.integers(0, 9, image.shape, dtype=np.uint8)
        image = cv2.add(image, noise)
        for x in range(0, self.width, 40):
            cv2.line(image, (x, 0), (x, self.height - 1), (44, 38, 34), 1)
        x, y = np.rint(self.truth(t)).astype(int)
        if self.visibility(t) != 'full occlusion':
            cv2.rectangle(image, (x - 16, y - 12), (x + 16, y + 12), (60, 220, 80), -1)
            if self.visibility(t) == 'partial occlusion':
                cv2.rectangle(image, (x, y - 13), (x + 18, y + 13), (70, 70, 70), -1)
        cv2.circle(image, (80, 290), 18, (220, 85, 50), -1)
        return image


class Tracker:
    def __init__(self, max_occlusion=0.65, roi=None):
        self.x = None
        self.P = np.eye(4) * 100
        self.last_t = self.observed_t = None
        self.state = 2
        self.hits = 0
        self.roi = roi
        self.hue = None
        self.max_occlusion = max_occlusion
        self.confidence = 0.0
        self.observation = None
        self.history = deque(maxlen=120)

    def detect(self, image):
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        if self.roi is not None and self.hue is None:
            x, y, w, h = self.roi
            crop = hsv[y:y+h, x:x+w]
            if crop.size == 0 or not np.any(crop[:, :, 1] > 60):
                raise ValueError('ROI must contain a saturated colored target inside the image')
            self.hue = float(np.median(crop[:, :, 0][crop[:, :, 1] > 60]))
        if self.hue is None:
            mask = cv2.inRange(hsv, (35, 90, 80), (85, 255, 255))
        else:
            delta = np.minimum(abs(hsv[:, :, 0].astype(float) - self.hue), 180 - abs(hsv[:, :, 0].astype(float) - self.hue))
            mask = ((delta <= 12) & (hsv[:, :, 1] > 70) & (hsv[:, :, 2] > 60)).astype(np.uint8) * 255
        count, _, stats, centers = cv2.connectedComponentsWithStats(mask)
        candidates = [(centers[i], stats[i, cv2.CC_STAT_AREA]) for i in range(1, count) if stats[i, cv2.CC_STAT_AREA] >= 70]
        if not candidates:
            return None, 0.0
        if self.x is not None and self.state != 2:
            candidates = [(c, a) for c, a in candidates if np.linalg.norm(c - self.x[:2]) < 90]
            if not candidates:
                return None, 0.0
            center, area = min(candidates, key=lambda pair: np.linalg.norm(pair[0] - self.x[:2]))
        else:
            center, area = max(candidates, key=lambda pair: pair[1])
        return center, min(1.0, area / 825)

    def update(self, image, t):
        dt = 1 / 30 if self.last_t is None else max(0.001, min(1, t - self.last_t))
        self.last_t = t
        if self.x is not None:
            F = np.array([[1, 0, dt, 0], [0, 1, 0, dt], [0, 0, 1, 0], [0, 0, 0, 1]], float)
            G = np.array([[dt * dt / 2, 0], [0, dt * dt / 2], [dt, 0], [0, dt]])
            self.x = F @ self.x
            self.P = F @ self.P @ F.T + G @ G.T * 18000
        z, confidence = self.detect(image)
        self.observation = z
        if z is not None:
            if self.x is None or self.state == 2:
                self.x = np.array([*z, 0.0, 0.0])
                self.P = np.diag([20., 20., 20000., 20000.])
                self.hits = 1
            else:
                H = np.eye(2, 4)
                K = self.P @ H.T @ np.linalg.inv(H @ self.P @ H.T + np.eye(2) * 5)
                self.x += K @ (z - H @ self.x)
                self.P = (np.eye(4) - K @ H) @ self.P
                self.hits += 1
            self.state = 0 if self.hits >= 3 else 3
            self.observed_t = t
            self.confidence = confidence
        elif self.x is not None:
            age = t - self.observed_t
            self.state = 1 if age <= self.max_occlusion else 2
            self.confidence = max(0, 1 - age / self.max_occlusion) * 0.8 if self.state == 1 else 0
            self.hits = 0
        if self.valid:
            self.history.append(self.x[:2].copy())
        elif self.state == 2:
            self.history.clear()
        return self

    @property
    def valid(self):
        return self.x is not None and self.state in (0, 1) and self.confidence >= 0.15

    def predict(self, horizon, width=640, height=360):
        if self.x is None:
            return np.zeros(2)
        return np.clip(self.x[:2] + self.x[2:] * max(0, min(0.5, horizon)), [0, 0], [width - 1, height - 1])
