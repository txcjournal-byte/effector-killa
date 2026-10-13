"""Generates the TrapWin SUB KILLA editor background (Resources/bg.png) at 2x of the 820x540 base size,
plus the photoreal knob images (Resources/knob_big.png, knob_small.png).
Run:  python3 tools/make_background.py

Layout constants here must match Source/PluginEditor.cpp.
Palette: black chassis, ice-white / silver accent, red details.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from brand import chrome_title, place, place_title, trapwin_logo  # noqa: E402
from knobgen import render_knob  # noqa: E402

S = 2
BW, BH = 820, 540
W, H = BW * S, BH * S
ROOT = os.path.dirname(HERE)
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(808)
ICE = (226, 236, 248)
RED = (255, 42, 58)
TEXT = (232, 236, 242)
INK = (132, 140, 152)

# ---- layout (base px) ----
BODY = (16, 96, 252, 320)
SCREEN = (264, 96, 556, 254)
GLIDEP = (568, 96, 804, 320)
GLIDE_C = (686, 194)
GLIDE_LCD = (631, 262, 741, 284)
SAT_Y = (262, 298)
SAT_X = [266 + 74 * i for i in range(4)]
DECK = (16, 330, 804, 446)
KEYBED = (20, 452, 800, 534)
BODY_X = [56, 134, 212]
BODY_Y = [150, 254]
BODY_NAMES = [["PUNCH", "P.TIME", "CLICK"], ["SUB OCT", "TUNE", "FINE"]]
DECK_X = [65, 131, 197, 273, 339, 415, 481, 547, 613, 689, 755]
DECK_Y = 380
DECK_NAMES = ["CUTOFF", "RESO", "ENV", "DRIVE", "MIX", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "WIDTH", "OUTPUT"]


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), int(size * S))


def to_img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


class BlendDraw:
    """ImageDraw that alpha-blends (Pillow overwrites alpha on RGBA targets): each call draws on an overlay."""

    def __init__(self, holder, key):
        self.holder, self.key = holder, key

    def textlength(self, *a, **k):
        return ImageDraw.Draw(self.holder[self.key]).textlength(*a, **k)

    def __getattr__(self, name):
        def call(*a, **k):
            base = self.holder[self.key]
            layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
            getattr(ImageDraw.Draw(layer), name)(*a, **k)
            self.holder[self.key] = Image.alpha_composite(base.convert("RGBA"), layer)
        return call


# ---------- chassis: black with a fine carbon weave + cold key light ----------
yy, xx = np.mgrid[0:H, 0:W]
base = np.zeros((H, W, 3), np.float32)
base[:] = (17, 18, 21)
# carbon twill: two interleaved diagonal tows
P = 6 * S
u = ((xx + yy) // P) % 2
v = ((xx - yy) // P) % 2
fx = ((xx + yy) % P) / P
fy = ((xx - yy) % P) / P
tow = np.where(u == v, 0.5 + 0.5 * np.sin(math.pi * fx), 0.5 + 0.5 * np.sin(math.pi * fy))
base += (tow[..., None] - 0.5) * 9.0
base += rng.normal(0, 1, (H, W, 1)) * 1.2
light = 1.0 + 0.22 * (1.0 - (xx / W) * 0.5 - (yy / H))
base *= light[..., None]
glow = np.exp(-(((xx - W * 0.5) / (W * 0.5)) ** 2 + ((yy - H * 0.28) / (H * 0.45)) ** 2))
base += glow[..., None] * np.array([7, 9, 12])
vig = 1.0 - 0.40 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = BlendDraw(globals(), "img")


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(12, 13, 15, 255), outline=(92, 98, 108, 140), shadow=210, brushed=False):
    """recessed panel: dark floor (optionally brushed), blurred inner shadow, thin lit rim"""
    global img, d
    x0, y0, x1, y1 = box
    if brushed:
        w, h = (x1 - x0) * S, (y1 - y0) * S
        fl = np.zeros((h, w, 3), np.float32)
        fl[:] = fill[:3]
        br = rng.normal(0, 1, (h, 1)) * 1.6 + rng.normal(0, 1, (h, w)) * 0.8
        fl += br[..., None] * 1.6
        fy_ = np.linspace(1.12, 0.9, h)[:, None, None]
        fl *= fy_
        fi = to_img(fl).convert("RGBA")
        m = Image.new("L", fi.size, 0)
        ImageDraw.Draw(m).rounded_rectangle((0, 0, w - 1, h - 1), radius=r * S, fill=255)
        img.paste(fi, (x0 * S, y0 * S), m)
        d = BlendDraw(globals(), "img")
    else:
        rr(box, r, fill=fill)
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(sh).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, outline=(0, 0, 0, shadow), width=5 * S)
    sh = sh.filter(ImageFilter.GaussianBlur(5 * S))
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, fill=255)
    sh.putalpha(Image.fromarray(np.minimum(np.asarray(sh.getchannel("A")), np.asarray(m))))
    img = Image.alpha_composite(img, sh)
    d = BlendDraw(globals(), "img")
    rr(box, r, outline=outline, width=S)
    d.line(((x0 + r) * S, y1 * S + S, (x1 - r) * S, y1 * S + S), fill=(150, 158, 170, 60), width=S)


lab = font("BarlowCondensed-Bold.ttf", 14.5)
lab_big = font("BarlowCondensed-Bold.ttf", 19)
small = font("BarlowCondensed-Bold.ttf", 11.5)
tiny = font("BarlowCondensed-SemiBold.ttf", 11)


def label(text, cx, y, f=lab, col=TEXT + (255,), spacing=0.0):
    widths = [d.textlength(ch, font=f) for ch in text]
    tw = sum(widths) + spacing * S * (len(text) - 1)
    x = cx * S - tw / 2
    if spacing:
        for ch, w in zip(text, widths):
            d.text((x, y * S + S), ch, font=f, fill=(0, 0, 0, 170))
            d.text((x, y * S), ch, font=f, fill=col)
            x += w + spacing * S
        return
    d.text((x, y * S + S), text, font=f, fill=(0, 0, 0, 170))      # engraved shadow
    d.text((x, y * S), text, font=f, fill=col)


def caption(name, xa, xb, y, col=RED):
    """group caption centred between xa..xb with hairlines either side"""
    cx = (xa + xb) / 2
    tw = d.textlength(name, font=tiny) / S + 1.2 * (len(name) - 1)
    d.line((xa * S, (y + 6) * S, (cx - tw / 2 - 7) * S, (y + 6) * S), fill=col + (80,), width=S)
    d.line(((cx + tw / 2 + 7) * S, (y + 6) * S, xb * S, (y + 6) * S), fill=col + (80,), width=S)
    label(name, cx, y, tiny, col + (215,), spacing=1.2)


# ---------- header: chrome wordmark (red glow) + full official TrapWin logo ----------
img = place_title(img, chrome_title("SUB KILLA", 30 * S, glow=RED, glow_strength=0.9), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "808  ·  SUB-BASS SYNTH", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))

hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    a = int(110 * math.sin(math.pi * t) ** 2)
    hd.point((x, 86 * S), fill=RED + (a,))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")

# ---------- panels ----------
inset(BODY, 12, fill=(15, 16, 18, 255), brushed=True)
inset(GLIDEP, 12, fill=(15, 16, 18, 255), brushed=True)
inset(DECK, 12, fill=(15, 16, 18, 255), brushed=True)


def small_seat(cx, cy, bipolar=False):
    for k in range(21):
        a = math.radians(225 - 270 * k / 20)
        x, y = cx + math.cos(a) * 29, cy - math.sin(a) * 29
        col = (150, 30, 40, 255) if (bipolar and k == 10) else (44, 46, 52, 255)
        d.ellipse(((x - 1.6) * S, (y - 1.6) * S, (x + 1.6) * S, (y + 1.6) * S), fill=col)
    d.ellipse(((cx - 24) * S, (cy - 24) * S, (cx + 24) * S, (cy + 24) * S), fill=(6, 6, 7, 255))


for r, row in enumerate(BODY_NAMES):
    for c, nm in enumerate(row):
        cx, cy = BODY_X[c], BODY_Y[r]
        label(nm, cx, cy - 49, lab)
        small_seat(cx, cy, bipolar=nm in ("TUNE", "FINE"))
caption("BODY", BODY[0] + 16, BODY[2] - 16, 303)

for cx, nm in zip(DECK_X, DECK_NAMES):
    label(nm, cx, DECK_Y - 48, lab)
    small_seat(cx, DECK_Y)
caption("FILTER", DECK_X[0] - 28, DECK_X[2] + 28, 430)
caption("DISTORTION", DECK_X[3] - 28, DECK_X[4] + 28, 430)
caption("AMP ENVELOPE", DECK_X[5] - 28, DECK_X[8] + 28, 430)
for i in (2, 4, 8):
    x = (DECK_X[i] + DECK_X[i + 1]) / 2
    d.line((x * S, 338 * S, x * S, 440 * S), fill=(46, 48, 54, 255), width=S)
    d.line(((x + 1) * S, 338 * S, (x + 1) * S, 440 * S), fill=(255, 255, 255, 10), width=S)


# ---------- glide dial seat ----------
def big_seat(cx, cy, R=55, arc_r=47, n_ticks=21, majors=(0, 5, 10, 15, 20), names=None):
    global img, d
    yy2, xx2 = np.mgrid[0:(2 * R + 8) * S, 0:(2 * R + 8) * S].astype(np.float32)
    cc = (R + 4) * S
    rr2 = np.sqrt((xx2 - cc) ** 2 + (yy2 - cc) ** 2) / S
    ang = np.arctan2(yy2 - cc, xx2 - cc)
    lip = (rr2 > R - 3) & (rr2 <= R + 2)
    floor = rr2 <= R - 3
    well = np.zeros(rr2.shape + (4,), np.float32)
    lit = 0.5 + 0.5 * np.cos(ang - math.radians(45))
    well[..., :3] = np.where(lip[..., None], (np.array([52, 54, 58]) + 110 * lit[..., None] ** 2), 0)
    shadowin = np.clip(1.0 - (R - 3 - rr2) / 16.0, 0, 1) * np.clip(0.5 - 0.5 * np.cos(ang - math.radians(45)), 0, 1)
    flo = np.array([11, 11, 13]) * (1.0 - 0.6 * shadowin[..., None])
    well[..., :3] = np.where(floor[..., None], flo, well[..., :3])
    well[..., 3] = np.where(lip | floor, 255, 0) * np.clip(R + 2 - rr2 + 0.5, 0, 1)
    img.alpha_composite(Image.fromarray(np.clip(well, 0, 255).astype(np.uint8), "RGBA"), (int((cx - R - 4) * S), int((cy - R - 4) * S)))
    d = BlendDraw(globals(), "img")
    d.arc(((cx - arc_r) * S, (cy - arc_r) * S, (cx + arc_r) * S, (cy + arc_r) * S), start=135, end=45, fill=(34, 36, 40, 255), width=5 * S)
    for i in range(n_ticks):
        a = math.radians(225 - 270 * i / (n_ticks - 1))
        major = i in majors
        r0, r1 = R + 5, (R + 13 if major else R + 9)
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(228, 234, 244, 220 if major else 90), width=S + (1 if major else 0))
    if names:
        for frac, t in names:
            a = math.radians(225 - 270 * frac)
            tx, ty = cx + math.cos(a) * (R + 23), cy - math.sin(a) * (R + 23)
            tw = d.textlength(t, font=small)
            d.text((tx * S - tw / 2, (ty - 7) * S), t, font=small, fill=INK + (255,))


big_seat(*GLIDE_C, names=[(0.0, "OFF"), (1.0, "2 s")])
label("GLIDE", GLIDE_C[0], 99, lab_big, spacing=2.0)


def lcd(box):
    """ice-white backlit LCD window (text painted live)"""
    global img, d
    inset(box, 6, fill=(7, 8, 10, 255), outline=(80, 86, 96, 170), shadow=230)
    x0, y0, x1, y1 = box
    g = np.zeros(((y1 - y0 - 4) * S, (x1 - x0 - 4) * S, 4), np.float32)
    g[..., :3] = (12, 16, 22)
    g[::3, :, :3] *= 0.7
    g[..., 3] = 255
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=4 * S, fill=255)
    img.paste(gi, ((x0 + 2) * S, (y0 + 2) * S), m)
    d = BlendDraw(globals(), "img")


lcd(GLIDE_LCD)

# ---------- scope screen ----------
inset(SCREEN, 12, fill=(6, 7, 8, 255), shadow=230)
x0, y0, x1, y1 = SCREEN
sw, sh_ = (x1 - x0 - 10) * S, (y1 - y0 - 10) * S
scr = np.zeros((sh_, sw, 3), np.float32)
syy, sxx = np.mgrid[0:sh_, 0:sw]
scr[:] = (8, 10, 13)
scr += (np.exp(-(((sxx - sw * 0.5) / (sw * 0.6)) ** 2 + ((syy - sh_ * 0.55) / (sh_ * 0.7)) ** 2)) * 9)[..., None] * np.array([0.7, 0.85, 1.0])
scr[::3] *= 0.84
scr += rng.normal(0, 0.9, scr.shape)
si = to_img(scr).convert("RGBA")
_SI = {"si": si}
sd = BlendDraw(_SI, "si")
# scope grid in screen-image coords (base px relative to the inner screen; the editor's scope area matches)
TR_TOP, TR_BOT = 22, 128          # trace area inside the 280x146 scope component (component at x0+6, y0+6)
ox, oy = 1, 1                     # component offset inside the screen image (base px)
gx0, gx1 = ox + 6, ox + 274
for k in range(9):
    gx = (gx0 + (gx1 - gx0) * k / 8) * S
    sd.line((gx, (oy + TR_TOP) * S, gx, (oy + TR_BOT) * S), fill=ICE + (14 if k % 2 else 26,), width=1)
for k in range(5):
    gy = (oy + TR_TOP + (TR_BOT - TR_TOP) * k / 4) * S
    sd.line((gx0 * S, gy, gx1 * S, gy), fill=ICE + (30 if k == 2 else 12,), width=1)
si = _SI["si"]
m = Image.new("L", si.size, 0)
ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=8 * S, fill=255)
img.paste(si, ((x0 + 5) * S, (y0 + 5) * S), m)
d = BlendDraw(globals(), "img")
d.line(((x0 + 12) * S, (y0 + 6 + 19) * S, (x1 - 12) * S, (y0 + 6 + 19) * S), fill=ICE + (36,), width=S)
d.line(((x0 + 12) * S, (y0 + 6 + 131) * S, (x1 - 12) * S, (y0 + 6 + 131) * S), fill=ICE + (22,), width=S)
gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(gl).polygon([((x0 + 5) * S, (y0 + 5) * S), ((x0 + 140) * S, (y0 + 5) * S), ((x0 + 40) * S, (y1 - 5) * S), ((x0 + 5) * S, (y1 - 5) * S)],
                           fill=(255, 255, 255, 8))
img = Image.alpha_composite(img, gl)
d = BlendDraw(globals(), "img")


# ---------- illuminated button bezels ----------
def bezel(box, r=8):
    global img, d
    x0, y0, x1, y1 = box
    rr((x0 - 3, y0 - 3, x1 + 3, y1 + 3), r + 2, fill=(6, 6, 7, 255))
    grad = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    gd.rounded_rectangle(((x0 - 3) * S, (y0 - 3) * S, (x1 + 3) * S, (y1 + 3) * S), radius=(r + 2) * S, outline=(130, 136, 148, 110), width=S)
    gd.rounded_rectangle(((x0 - 1) * S, (y0 - 1) * S, (x1 + 1) * S, (y1 + 1) * S), radius=r * S, outline=(0, 0, 0, 220), width=2 * S)
    img = Image.alpha_composite(img, grad)
    d = BlendDraw(globals(), "img")


for x in SAT_X:
    bezel((x, SAT_Y[0], x + 68, SAT_Y[1]))
caption("SATURATION", SCREEN[0] + 4, SCREEN[2] - 4, 305)

# ---------- keyboard bed ----------
inset(KEYBED, 10, fill=(6, 6, 7, 255), shadow=240)
x0, y0, x1, y1 = KEYBED
# felt strip behind the keys
d.rectangle(((x0 + 6) * S, (y0 + 13) * S, (x1 - 6) * S, (y0 + 16) * S), fill=(120, 14, 22, 200))

# screws
screw = render_knob(int(14 * S), "alu", seed=9)
sdr = ImageDraw.Draw(screw)
cs = screw.size[0] / 2
for a in (0.4, 0.4 + math.pi / 2):
    sdr.line((cs - math.cos(a) * cs * 0.38, cs - math.sin(a) * cs * 0.38, cs + math.cos(a) * cs * 0.38, cs + math.sin(a) * cs * 0.38), fill=(40, 40, 44, 255), width=max(2, S))
for (x, y) in ((10, 10), (BW - 10, 10), (10, BH - 10), (BW - 10, BH - 10)):
    img.alpha_composite(screw, (int(x * S - cs), int(y * S - cs)))

out = os.path.join(ROOT, "Resources", "bg.png")
img.convert("RGB").save(out, optimize=True)
print("wrote", out, img.size)

# ---------- knobs (static image; the editor draws the rotating pointer) ----------
render_knob(300, os.environ.get("SK_KNOB_BIG", "alu"), seed=4).save(os.path.join(ROOT, "Resources", "knob_big.png"), optimize=True)
render_knob(128, os.environ.get("SK_KNOB_SMALL", "black"), seed=5).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("knobs ok")
