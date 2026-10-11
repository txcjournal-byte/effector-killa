"""Put the official TrapWin logo (stars + AUDIO · UNDERGROUND) on the Effector shop cover.

usage: python3 -I recover.py <cover.png> <trapwin_logo.png> <out.png>
Text/glow on the cover is light added on a dark background, so it is lifted off as
(original - clean background) and added back at its new position.
"""
import sys
import numpy as np
import cv2
from PIL import Image

cover_p, logo_p, out_p = sys.argv[1:4]
img = np.asarray(Image.open(cover_p).convert("RGB")).astype(np.float32)
H, W, _ = img.shape


def clean_background(region, mask, scale=8, feather=6.0):
    """Smooth background under `mask` (inpaint on a downsampled copy, then upscale)."""
    h, w, _ = region.shape
    small = cv2.resize(region, (w // scale, h // scale), interpolation=cv2.INTER_AREA)
    msmall = cv2.resize(mask, (w // scale, h // scale), interpolation=cv2.INTER_NEAREST)
    msmall = cv2.dilate(msmall, np.ones((3, 3), np.uint8))
    filled = cv2.inpaint(np.clip(small, 0, 255).astype(np.uint8), msmall, 6, cv2.INPAINT_NS).astype(np.float32)
    filled = cv2.GaussianBlur(filled, (0, 0), 2.0)
    big = cv2.resize(filled, (w, h), interpolation=cv2.INTER_CUBIC)
    m = cv2.GaussianBlur(mask.astype(np.float32), (0, 0), feather)[..., None]
    return region * (1 - m) + big * m


def key(layer, floor=10.0, soft=14.0, dehaze=False):
    """Drop the faint background left in a lifted layer so no box edge shows."""
    if dehaze:   # sparse text: most of the crop is background haze
        base = np.median(layer.reshape(-1, 3), axis=0)
        layer = np.clip(layer - base, 0, 255)
    lum = layer.max(axis=2, keepdims=True)
    a = np.clip((lum - floor) / soft, 0, 1)
    # fade the crop edges so a glow cut by the crop never shows as a line
    h, w = layer.shape[:2]
    e = 14.0
    wy = np.clip(np.minimum(np.arange(h), np.arange(h)[::-1]) / e, 0, 1)
    wx = np.clip(np.minimum(np.arange(w), np.arange(w)[::-1]) / e, 0, 1)
    return layer * a * (wy[:, None] * wx[None, :])[..., None]


def add_layer(dst, layer, x, y, dehaze=False):
    layer = key(layer, dehaze=dehaze)
    h, w, _ = layer.shape
    dst[y:y + h, x:x + w] = np.clip(dst[y:y + h, x:x + w] + layer, 0, 255)


def over_logo(dst, logo_rgba, x, y):
    h, w, _ = logo_rgba.shape
    a = logo_rgba[..., 3:4] / 255.0
    dst[y:y + h, x:x + w] = dst[y:y + h, x:x + w] * (1 - a) + logo_rgba[..., :3] * a


def scaled(arr, s):
    h, w = arr.shape[:2]
    return cv2.resize(arr, (int(round(w * s)), int(round(h * s))), interpolation=cv2.INTER_AREA)


logo = np.asarray(Image.open(logo_p).convert("RGBA")).astype(np.float32)
out = img.copy()

# ---- top header: TRAPWIN (old) / EFFECTOR / tagline -----------------------------------------
TOP = 530
top = img[:TOP].copy()
mask = np.zeros((TOP, W), np.uint8)
mask[30:530, 90:1510] = 1
bg = clean_background(top, mask, scale=16, feather=30.0)
lift = np.clip(top - bg, 0, 255)
effector = lift[218:425, 200:1405]          # big chrome EFFECTOR with its glow
tagline = lift[445:500, 500:1100]            # EVERY EFFECT. ONE WIN.
head = bg.copy()

lg = scaled(logo, 820 / logo.shape[1])
over_logo(head, lg, (W - lg.shape[1]) // 2, 6)
ef = scaled(effector, 0.76)
add_layer(head, ef, (W - ef.shape[1]) // 2 + 4, 312)
add_layer(head, tagline, (W - tagline.shape[1]) // 2, 468, dehaze=True)
out[:TOP] = head

# ---- logo panel inside the plugin screenshot ------------------------------------------------
px0, py0, px1, py1 = 130, 578, 412, 758       # panel interior (inside its border)
panel = img[py0:py1, px0:px1].copy()
rows = img[py0:py1, 416:417].copy()           # per-row panel colour (no text in this column)
pbg = np.repeat(rows, px1 - px0, axis=1)
plift = np.clip(panel - pbg, 0, 255)
p_eff = plift[657 - py0:712 - py0, 140 - px0:408 - px0]
p_tag = plift[720 - py0:742 - py0, 140 - px0:335 - px0]
newp = pbg.copy()
pl = scaled(logo, 262 / logo.shape[1])
over_logo(newp, pl, ((px1 - px0) - pl.shape[1]) // 2, 2)
pe = scaled(p_eff, 0.80)
add_layer(newp, pe, ((px1 - px0) - pe.shape[1]) // 2, 2 + pl.shape[0] + 2)
pt = p_tag
add_layer(newp, pt, ((px1 - px0) - pt.shape[1]) // 2, (py1 - py0) - pt.shape[0] - 4)
out[py0:py1, px0:px1] = newp

Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).save(out_p)
print("wrote", out_p)
