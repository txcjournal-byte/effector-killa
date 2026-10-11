"""Shop cover: keeps the header (official logo, EFFECTOR, tagline) and the bottom text of the
current cover and puts a fresh 2x render of the plugin UI in between.

usage: python3 -I Tools/make_cover.py <cover.png> <ui_2x.png> <out.png>
The UI render comes from the snapshot tool (00_cover_ui.png).
"""
import sys
import numpy as np
import cv2
from PIL import Image

cover_p, ui_p, out_p = sys.argv[1:4]
cover = np.asarray(Image.open(cover_p).convert("RGB")).astype(np.float32)
ui = np.asarray(Image.open(ui_p).convert("RGB")).astype(np.float32)
H, W, _ = cover.shape

# ---- clean the band where the old UI was ---------------------------------------------------
# The background outside the old frame is a smooth vertical gradient: rebuild the band from the
# left / right edge columns, then blend it in with soft vertical ramps (the bottom ramp ends
# above the feature text).
y0, y1 = 520, 1424
cols = np.arange(W, dtype=np.float32) / (W - 1)
left = cover[:, 12:28].mean(axis=1, keepdims=True)
right = cover[:, W - 28:W - 12].mean(axis=1, keepdims=True)
model = left * (1 - cols[None, :, None]) + right * cols[None, :, None]
model = cv2.GaussianBlur(model, (0, 0), 6.0)
wy = np.zeros(H, np.float32)
for yy in range(H):
    if y0 <= yy <= y1:
        wy[yy] = min(1.0, (yy - y0) / 10.0, (y1 - yy) / 26.0)
wx = np.clip(np.minimum(np.arange(W), np.arange(W)[::-1]) / 30.0, 0, 1).astype(np.float32)
m = (wy[:, None] * wx[None, :])[..., None]
out = cover * (1 - m) + model * m

# ---- UI with an ice-blue glow and a thin frame ---------------------------------------------
target_h = 852
target_w = int(round(ui.shape[1] * target_h / ui.shape[0]))
ui_s = cv2.resize(ui, (target_w, target_h), interpolation=cv2.INTER_AREA)
x = (W - target_w) // 2
y = 536

glow = np.zeros((H, W), np.float32)
glow[y - 6:y + target_h + 6, x - 6:x + target_w + 6] = 1.0
glow = cv2.GaussianBlur(glow, (0, 0), 46.0)[..., None]
out = out + glow * np.array([30, 70, 150], np.float32) * 0.30

frame = np.array([159, 195, 255], np.float32)
out[y - 3:y + target_h + 3, x - 3:x + target_w + 3] = frame * 0.55
out[y - 1:y + target_h + 1, x - 1:x + target_w + 1] = frame * 0.9
out[y:y + target_h, x:x + target_w] = ui_s

Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).save(out_p)
print("wrote", out_p, "UI", target_w, "x", target_h)
