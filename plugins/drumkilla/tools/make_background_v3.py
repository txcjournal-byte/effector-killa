# Step 3 (layout v0.3): larger control area. Title bar 60 px, artwork column (portrait + logo)
# scaled into a 340 px wide column on the left, meter panel underneath, plaster control area.
# Reads Resources/source/design.png and Resources/source/cleaned.png (see make_background.py).
# Usage: pip install pillow numpy opencv-python-headless && python tools/make_background_v3.py
import os
import numpy as np, cv2
from PIL import Image
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
rng = np.random.default_rng(808)
design = Image.open(os.path.join(ROOT, 'Resources/source/design.png')).convert('RGB')
cleaned = np.asarray(Image.open(os.path.join(ROOT, 'Resources/source/cleaned.png')).convert('RGB')).astype(np.float32)
W, H = 1536, 1024
TITLE_H = 60

out = np.asarray(design).astype(np.float32).copy()
# dark interior
noise = rng.normal(0, 5, (H, W, 1)).astype(np.float32)
out[TITLE_H + 2:H - 8, 8:W - 8] = np.clip(np.array([13, 12, 11], np.float32) + noise[TITLE_H + 2:H - 8, 8:W - 8], 0, 255)

# title bar: left (icon + name) and right (window buttons) keep their look, middle is stretched
title = design.crop((0, 0, W, 80))
s = TITLE_H / 80.0
left = title.crop((0, 0, 330, 80)).resize((int(330 * s), TITLE_H), Image.LANCZOS)
right = title.crop((1360, 0, W, 80)).resize((int((W - 1360) * s), TITLE_H), Image.LANCZOS)
mid = title.crop((420, 0, 1000, 80)).resize((W - left.width - right.width, TITLE_H), Image.LANCZOS)
bar = Image.new('RGB', (W, TITLE_H))
bar.paste(left, (0, 0)); bar.paste(mid, (left.width, 0)); bar.paste(right, (left.width + mid.width, 0))
out[0:TITLE_H] = np.asarray(bar).astype(np.float32)

# artwork column: portrait + logo
col = design.crop((26, 80, 444, 824))
colW = 340
col = col.resize((colW, int(col.height * colW / col.width)), Image.LANCZOS)
out[TITLE_H + 6:TITLE_H + 6 + col.height, 12:12 + colW] = np.asarray(col).astype(np.float32)

# plaster control area (quilted from real plaster patches of the artwork)
X0, Y0, X1, Y1 = 362, TITLE_H + 6, W - 14, H - 14
a = cleaned; lum = a.mean(2); R, G = a[..., 0], a[..., 1]
P = 48
cands = []
for y in range(90, 990 - P, 6):
    for x in range(450, 1360 - P, 6):
        p = lum[y:y + P, x:x + P]
        if np.percentile(p, 3) < 70 or p.mean() < 140: continue
        if (R[y:y + P, x:x + P] - G[y:y + P, x:x + P]).max() > 30: continue
        if p.std() > 30 or (p < 80).mean() > 0.006: continue
        cands.append((x, y))
w, h = X1 - X0, Y1 - Y0
step = 36; ov = P - step
acc = np.zeros((h + P, w + P, 3), np.float32); wsum = np.zeros((h + P, w + P, 1), np.float32)
ramp = np.minimum(np.arange(P), np.arange(P)[::-1]).astype(np.float32)
ramp = np.clip((ramp + 1) / (ov / 2 + 1), 0, 1)
wmask = (ramp[None, :] * ramp[:, None])[..., None]
target = np.array([180, 171, 154], np.float32)
for y in range(0, h + 1, step):
    for x in range(0, w + 1, step):
        cx, cy = cands[rng.integers(len(cands))]
        p = cleaned[cy:cy + P, cx:cx + P].copy()
        p = np.rot90(p, rng.integers(4))
        if rng.random() < 0.5: p = p[:, ::-1]
        p = p - p.reshape(-1, 3).mean(0) + target
        acc[y:y + P, x:x + P] += p * wmask; wsum[y:y + P, x:x + P] += wmask
tex = (acc / np.maximum(wsum, 1e-6))[:h, :w]
n = cv2.resize(rng.standard_normal((6, 9)).astype(np.float32), (w, h), interpolation=cv2.INTER_CUBIC)
tex *= (1 + 0.05 * n)[..., None]
out[Y0:Y1, X0:X1] = np.clip(tex, 0, 255)

Image.fromarray(out.astype(np.uint8)).save(os.path.join(ROOT, 'Resources/background.jpg'), quality=92, optimize=True)
print('artwork column height', col.height)
