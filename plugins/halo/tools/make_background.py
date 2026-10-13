"""Generates the HALO editor background (Resources/bg.png, 2x of the 820x500 base size)
and the photoreal knob images (Resources/knob_big.png, Resources/knob_small.png).
Run:  python3 tools/make_background.py
The layout constants here MUST match Source/PluginEditor.cpp (Layout namespace).
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from brand import chrome_title, place, place_title, text_bbox_offset, trapwin_logo  # noqa: E402
from knobgen import render_knob  # noqa: E402

S = 2
BW, BH = 820, 500
W, H = BW * S, BH * S
ROOT = os.path.dirname(HERE)
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(7)
ACC = (95, 227, 255)          # #5FE3FF ice cyan
INK = (112, 152, 170)
TXT = (222, 242, 250)

# ---- layout (base px) ----
BIG = [(150, 228, "DECAY"), (670, 228, "MIX")]
BIG_ARC_R = 55
VIS = (410, 214, 114)
FREEZE = (358, 340, 104, 28)
DECK = (16, 380, 804, 492)
SMALL_Y = 434
SMALL = [(68, "PRE-DELAY"), (150, "SIZE"), (232, "DIFFUSION"), (314, "MOD"),
         (410, "LOW-CUT"), (492, "HI-CUT"),
         (588, "WIDTH"), (670, "DUCK"), (752, "OUTPUT")]
SEPARATORS = (362, 539)
SMALL_DOT_R = 29
SMALL_DOTS = 24


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), int(size * S))


def to_img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


# ---------- chassis: deep blue-black anodised metal, fine horizontal brushing, cyan bloom ----------
yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
base = np.zeros((H, W, 3), np.float32)
base[:] = (12, 22, 30)
brush = rng.normal(0, 1, (H, 1)) * 2.0 + rng.normal(0, 1, (H, W)) * 1.2
brush = np.asarray(Image.fromarray(np.uint8(np.clip(128 + brush * 6, 0, 255))).filter(ImageFilter.BoxBlur(1)), np.float32) - 128
base += brush[..., None] * 0.35
light = 1.0 + 0.18 * (1.0 - (xx / W) * 0.4 - (yy / H))
base *= light[..., None]
# cyan bloom behind the visualiser
vx, vy = VIS[0] * S, VIS[1] * S
bloom = np.exp(-(((xx - vx) / (W * 0.33)) ** 2 + ((yy - vy) / (H * 0.42)) ** 2))
base += bloom[..., None] * np.array([6, 30, 40])
vig = 1.0 - 0.40 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.6
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = ImageDraw.Draw(img)


def redraw():
    global d
    d = ImageDraw.Draw(img)


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(9, 16, 22, 255), outline=(70, 110, 130, 120), lip=True):
    global img
    rr(box, r, fill=fill)
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    x0, y0, x1, y1 = box
    ImageDraw.Draw(sh).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, outline=(0, 0, 0, 210), width=5 * S)
    sh = sh.filter(ImageFilter.GaussianBlur(4 * S))
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, fill=255)
    sh.putalpha(Image.fromarray(np.minimum(np.asarray(sh.getchannel("A")), np.asarray(m))))
    img = Image.alpha_composite(img, sh)
    redraw()
    rr(box, r, outline=outline, width=S)
    if lip:  # lit bottom lip
        d.line(((x0 + r) * S, y1 * S + S, (x1 - r) * S, y1 * S + S), fill=(120, 170, 190, 22), width=S)


lab = font("BarlowCondensed-Bold.ttf", 15)
lab_s = font("BarlowCondensed-Bold.ttf", 13)
small = font("BarlowCondensed-SemiBold.ttf", 12)


def label(text, cx, y, f=lab, col=TXT + (255,), spacing=0.0):
    if spacing:
        widths = [d.textlength(ch, font=f) for ch in text]
        tw = sum(widths) + spacing * S * (len(text) - 1)
        x = cx * S - tw / 2
        for ch, w in zip(text, widths):
            d.text((x, y * S + S), ch, font=f, fill=(0, 0, 0, 170))
            d.text((x, y * S), ch, font=f, fill=col)
            x += w + spacing * S
        return
    tw = d.textlength(text, font=f)
    d.text((cx * S - tw / 2, y * S + S), text, font=f, fill=(0, 0, 0, 170))      # engraved shadow
    d.text((cx * S - tw / 2, y * S), text, font=f, fill=col)


def lcd(cx, cy, w, h):
    """recessed LCD window; the editor draws the glowing value text on top"""
    global img
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    inset((x0, y0, x1, y1), 4, fill=(5, 14, 18, 255), outline=(60, 120, 140, 110), lip=False)
    # scanlines + faint unlit segments glow
    scr = np.zeros((int((h - 2) * S), int((w - 2) * S), 4), np.float32)
    scr[..., :3] = (8, 26, 32)
    scr[::2, :, :3] *= 0.75
    scr[..., 3] = 255
    cy2 = np.linspace(-1, 1, scr.shape[0])[:, None]
    scr[..., :3] *= (1.0 - 0.35 * cy2 ** 2)[..., None]
    si = Image.fromarray(np.clip(scr, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", si.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=3 * S, fill=255)
    img.paste(si, (int((x0 + 1) * S), int((y0 + 1) * S)), m)
    redraw()


# ---------- header: chrome HALO wordmark + official TrapWin logo ----------
_tt = chrome_title("HALO", 38 * S, glow=ACC, glow_strength=0.9, tracking=0.06)
img = place_title(img, _tt, 24 * S, 12 * S)
_bx = text_bbox_offset(_tt)
title_w = (_bx[2] - _bx[0]) / S
_lg = trapwin_logo(34 * S)
img = place(img, _lg, 20 * S, 60 * S)
redraw()
d.text((20 * S + _lg.width + 12 * S, 72 * S), "LUSH REVERB  ·  FREEZE  ·  DUCK", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))
# "FREE" badge next to the wordmark
bx0, by0 = 24 + title_w + 14, 30
rr((bx0, by0, bx0 + 40, by0 + 18), 4, outline=ACC + (200,), width=S)
label("FREE", bx0 + 20, by0 + 1, lab_s, ACC + (255,), spacing=1.0)


# ---------- big dials ----------
def big_seat(cx, cy, marks, minor=31):
    global img
    R = 64
    pad = 6
    n = (2 * R + 2 * pad) * S
    yy2, xx2 = np.mgrid[0:n, 0:n].astype(np.float32)
    cc = (R + pad) * S
    rr2 = np.sqrt((xx2 - cc) ** 2 + (yy2 - cc) ** 2) / S
    ang = np.arctan2(yy2 - cc, xx2 - cc)
    lip = (rr2 > R - 3) & (rr2 <= R + 2)
    floor = rr2 <= R - 3
    well = np.zeros(rr2.shape + (4,), np.float32)
    lit = 0.5 + 0.5 * np.cos(ang - math.radians(45))
    well[..., :3] = np.where(lip[..., None], (np.array([40, 58, 70]) + 120 * lit[..., None] ** 2), 0)
    shadowin = np.clip(1.0 - (R - 3 - rr2) / 14.0, 0, 1) * np.clip(0.5 - 0.5 * np.cos(ang - math.radians(45)), 0, 1)
    flo = np.array([7, 13, 18]) * (1.0 - 0.6 * shadowin[..., None])
    well[..., :3] = np.where(floor[..., None], flo, well[..., :3])
    well[..., 3] = np.where(lip | floor, 255, 0) * np.clip(R + 2 - rr2 + 0.5, 0, 1)
    img.alpha_composite(Image.fromarray(np.clip(well, 0, 255).astype(np.uint8), "RGBA"), (int((cx - R - pad) * S), int((cy - R - pad) * S)))
    redraw()
    # value-arc groove
    r = BIG_ARC_R
    d.arc(((cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S), start=135, end=45, fill=(16, 34, 42, 255), width=7 * S)
    for i in range(minor):
        a = math.radians(225 - 270 * i / (minor - 1))
        r0, r1 = R + 5, R + 9
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(170, 210, 225, 90), width=S)
    for t, pos in marks:
        a = math.radians(225 - 270 * pos)
        r0, r1 = R + 5, R + 13
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(215, 240, 250, 230), width=2 * S)
        tx, ty = cx + math.cos(a) * (R + 25), cy - math.sin(a) * (R + 25)
        tw = d.textlength(t, font=small)
        d.text((tx * S - tw / 2, (ty - 8) * S), t, font=small, fill=INK + (255,))


def lognorm(v, lo, hi):
    return math.log(v / lo) / math.log(hi / lo)


big_seat(*BIG[0][:2], [(s, lognorm(v, 0.2, 30)) for s, v in (("0.2", 0.2), ("1", 1), ("3", 3), ("10", 10), ("30s", 30))])
big_seat(*BIG[1][:2], [(s, v / 100) for s, v in (("0", 0), ("25", 25), ("50", 50), ("75", 75), ("100", 100))])
for cx, cy, t in BIG:
    label(t, cx, 102, lab, spacing=1.5)
    lcd(cx, cy + 96, 100, 22)


# ---------- visualiser: machined bezel + dark glass ----------
cx, cy, R = VIS
pad = 14
n = (2 * (R + pad)) * S
yy2, xx2 = np.mgrid[0:n, 0:n].astype(np.float32)
cc = (R + pad) * S
rad = np.sqrt((xx2 - cc) ** 2 + (yy2 - cc) ** 2) / S
ang = np.arctan2(yy2 - cc, xx2 - cc)
vis = np.zeros(rad.shape + (4,), np.float32)
# bezel: lathe brushed ring R..R+9 with anisotropic highlight
bez = (rad > R) & (rad <= R + 9)
grooves = rng.normal(0, 1, 4096)
gidx = np.clip(((rad - R) / 9 * 4095).astype(int), 0, 4095)
lathe = np.convolve(grooves, np.ones(5) / 5, mode="same")[gidx]
la = math.radians(-125)
aniso = np.abs(np.cos(ang - la)) ** 5
t = (rad - R) / 9
prof = np.sin(np.clip(t, 0, 1) * math.pi)                  # rounded bezel profile
bcol = (np.array([70, 86, 96]) * (0.55 + 0.45 * prof[..., None]) + (120 * aniso * prof)[..., None] + (lathe * 8)[..., None])
vis[..., :3] = np.where(bez[..., None], bcol, 0)
# glass floor
glass = rad <= R
gcol = np.array([4, 12, 17], np.float32) + np.array([8, 30, 40]) * np.exp(-(rad / (R * 0.55)) ** 2)[..., None]
inner_sh = np.clip(1.0 - (R - rad) / 18.0, 0, 1) ** 2
gcol = gcol * (1.0 - 0.75 * inner_sh[..., None])
vis[..., :3] = np.where(glass[..., None], gcol, vis[..., :3])
vis[..., 3] = np.where(bez | glass, 255, 0) * np.clip(R + 9 - rad + 0.5, 0, 1)
# soft drop shadow ring around the bezel
sh = Image.fromarray((np.clip(1.0 - np.abs(rad - (R + 10)) / 8.0, 0, 1) * 160).astype(np.uint8)).filter(ImageFilter.GaussianBlur(4 * S))
shadow = Image.new("RGBA", (n, n), (0, 0, 0, 255))
shadow.putalpha(sh)
img.alpha_composite(shadow, (int((cx - R - pad) * S), int((cy - R - pad + 3) * S)))
img.alpha_composite(Image.fromarray(np.clip(vis, 0, 255).astype(np.uint8), "RGBA"), (int((cx - R - pad) * S), int((cy - R - pad) * S)))
redraw()
# scope graticule: faint rings + degree ticks
for gr in (48, 92):
    d.ellipse(((cx - gr) * S, (cy - gr) * S, (cx + gr) * S, (cy + gr) * S), outline=(60, 130, 150, 16), width=S)
for i in range(72):
    a = math.radians(i * 5)
    major = i % 6 == 0
    r0, r1 = R - (9 if major else 5), R - 2
    d.line(((cx + math.cos(a) * r0) * S, (cy + math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy + math.sin(a) * r1) * S),
           fill=(110, 190, 210, 90 if major else 32), width=S)

# freeze button slot
fx, fy, fw, fh = FREEZE
inset((fx - 5, fy - 4, fx + fw + 5, fy + fh + 4), 9, fill=(6, 12, 16, 255), outline=(60, 100, 120, 110), lip=False)

# ---------- bottom deck ----------
inset(DECK, 12, fill=(10, 18, 24, 255))
for x, t in SMALL:
    label(t, x, 387, lab_s, (200, 225, 235, 255), spacing=0.8)
    cy = SMALL_Y
    for k in range(SMALL_DOTS + 1):
        a = math.radians(225 - 270 * k / SMALL_DOTS)
        px, py = x + math.cos(a) * SMALL_DOT_R, cy - math.sin(a) * SMALL_DOT_R
        d.ellipse(((px - 1.7) * S, (py - 1.7) * S, (px + 1.7) * S, (py + 1.7) * S), fill=(26, 48, 58, 255))
    d.ellipse(((x - 23) * S, (cy - 23) * S, (x + 23) * S, (cy + 23) * S), fill=(4, 8, 11, 255))
    lcd(x, 476, 70, 18)
for x in SEPARATORS:
    d.line((x * S, 392 * S, x * S, 482 * S), fill=(36, 60, 72, 255), width=S)
    d.line(((x + 1) * S, 392 * S, (x + 1) * S, 482 * S), fill=(0, 0, 0, 120), width=S)

# ---------- screws ----------
screw = render_knob(int(14 * S), "alu", seed=9)
sd = ImageDraw.Draw(screw)
cs = screw.size[0] / 2
for a in (0.4, 0.4 + math.pi / 2):
    sd.line((cs - math.cos(a) * cs * 0.38, cs - math.sin(a) * cs * 0.38, cs + math.cos(a) * cs * 0.38, cs + math.sin(a) * cs * 0.38),
            fill=(40, 40, 44, 255), width=max(2, S))
for (x, y) in ((10, 10), (BW - 10, 10), (10, BH - 10), (BW - 10, BH - 10)):
    img.alpha_composite(screw, (int(x * S - cs), int(y * S - cs)))

out = os.path.join(ROOT, "Resources", "bg.png")
img.convert("RGB").save(out, optimize=True)
print("wrote", out, img.size)


# ---------- knobs (cool-tinted aluminium) ----------
def cool(im, k=(0.92, 0.98, 1.06)):
    a = np.asarray(im, np.float32)
    a[..., :3] *= np.array(k)
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), "RGBA")


cool(render_knob(320, "alu", seed=3)).save(os.path.join(ROOT, "Resources", "knob_big.png"), optimize=True)
cool(render_knob(160, "black", seed=5), (0.95, 1.0, 1.08)).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("wrote knobs")
