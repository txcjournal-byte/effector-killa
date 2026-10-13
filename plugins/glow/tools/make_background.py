"""Generates the TrapWin GLOW editor background (Resources/bg.png) at 2x of the 840x520 base size,
the lit sunburst overlay (Resources/rays_lit.png) and the photoreal knob images (knob_big.png, knob_small.png).
Run:  python3 tools/make_background.py

Layout constants here must match Source/PluginEditor.cpp.
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
BW, BH = 840, 520
W, H = BW * S, BH * S
ROOT = os.path.dirname(HERE)
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(17)
SUN = (255, 228, 92)
TEXT = (238, 232, 218)
INK = (150, 142, 124)

# ---- layout (base px) ----
KNOB_C = (420, 300)
RAY_R0, RAY_LONG, RAY_SHORT = 140, 174, 161
N_RAYS = 41
RING_R = 126
LIT_HALF = 204                      # rays_lit.png covers KNOB_C +- LIT_HALF
GLOW_LCD = (366, 440, 474, 468)
L_PANEL = (18, 100, 240, 504)
R_PANEL = (600, 100, 822, 504)
MODE_X = (34, 224)
MODE_Y = [132, 188, 244, 300]
MODE_H = 48
ENH_LCD = (32, 374, 226, 490)
MET_LCD = (614, 374, 808, 490)
SMALL = {"CONTRAST": (662, 180), "COLOUR": (762, 180), "OUTPUT": (662, 300)}
AG_BTN = (722, 276, 806, 326)


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


# ---------- chassis: warm graphite, vertical brushing, a low sun glow behind the big knob ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (24, 23, 24)
yy, xx = np.mgrid[0:H, 0:W]
brush = rng.normal(0, 1, (1, W)) * 2.0 + rng.normal(0, 1, (H, W)) * 1.1
brush = np.asarray(Image.fromarray(np.uint8(np.clip(128 + brush * 6, 0, 255))).filter(ImageFilter.BoxBlur(1)), np.float32) - 128
base += brush[..., None] * 0.40
light = 1.0 + 0.16 * (1.0 - (yy / H))
base *= light[..., None]
kx, ky = KNOB_C[0] * S, KNOB_C[1] * S
glow = np.exp(-(((xx - kx) / (W * 0.30)) ** 2 + ((yy - ky) / (H * 0.48)) ** 2))
base += glow[..., None] * np.array([26, 19, 4])
vig = 1.0 - 0.40 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = BlendDraw(globals(), "img")


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(14, 14, 15, 255), outline=(88, 82, 68, 150), shadow=200):
    """recessed panel: dark floor, blurred inner shadow, thin lit rim"""
    global img, d
    rr(box, r, fill=fill)
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    x0, y0, x1, y1 = box
    ImageDraw.Draw(sh).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, outline=(0, 0, 0, shadow), width=5 * S)
    sh = sh.filter(ImageFilter.GaussianBlur(5 * S))
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, fill=255)
    sh.putalpha(Image.fromarray(np.minimum(np.asarray(sh.getchannel("A")), np.asarray(m))))
    img = Image.alpha_composite(img, sh)
    d = BlendDraw(globals(), "img")
    rr(box, r, outline=outline, width=S)
    d.line(((x0 + r) * S, y1 * S + S, (x1 - r) * S, y1 * S + S), fill=(126, 116, 96, 70), width=S)


lab = font("BarlowCondensed-Bold.ttf", 15)
lab_big = font("BarlowCondensed-Bold.ttf", 18)
small = font("BarlowCondensed-Bold.ttf", 12)
tiny = font("BarlowCondensed-SemiBold.ttf", 11)


def label(text, cx, y, f=lab, col=TEXT + (255,), spacing=0.0):
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
    d.text((cx * S - tw / 2, y * S + S), text, font=f, fill=(0, 0, 0, 170))
    d.text((cx * S - tw / 2, y * S), text, font=f, fill=col)


def caption_rule(text, x0, x1, y):
    """small sun-coloured caption with hairlines either side"""
    cx = (x0 + x1) / 2
    tw = d.textlength(text, font=tiny) / S
    d.line((x0 * S, (y + 7) * S, (cx - tw / 2 - 7) * S, (y + 7) * S), fill=SUN + (60,), width=S)
    d.line(((cx + tw / 2 + 7) * S, (y + 7) * S, x1 * S, (y + 7) * S), fill=SUN + (60,), width=S)
    label(text, cx, y, tiny, SUN + (200,), spacing=1.2)


# ---------- header: chrome wordmark + full official TrapWin logo ----------
img = place_title(img, chrome_title("GLOW", 30 * S, glow=SUN, glow_strength=0.85), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "ONE-KNOB ENHANCER  ·  WARM · BRIGHT · FAT · LOUD", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))

hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    a = int(95 * math.sin(math.pi * t) ** 2)
    hd.point((x, 86 * S), fill=SUN + (a,))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")

# ---------- side panels ----------
inset(L_PANEL, 14, fill=(17, 16, 16, 255))
inset(R_PANEL, 14, fill=(17, 16, 16, 255))
caption_rule("MODE", L_PANEL[0] + 14, L_PANEL[2] - 14, 108)
caption_rule("ENHANCE", L_PANEL[0] + 14, L_PANEL[2] - 14, 355)
caption_rule("SHAPE", R_PANEL[0] + 14, R_PANEL[2] - 14, 108)
caption_rule("LEVELS", R_PANEL[0] + 14, R_PANEL[2] - 14, 355)


def bezel(box, r=10):
    """machined bezel an illuminated button sits in"""
    global img, d
    x0, y0, x1, y1 = box
    rr((x0 - 4, y0 - 4, x1 + 4, y1 + 4), r + 3, fill=(8, 8, 9, 255))
    grad = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    gd.rounded_rectangle(((x0 - 4) * S, (y0 - 4) * S, (x1 + 4) * S, (y1 + 4) * S), radius=(r + 3) * S, outline=(124, 114, 96, 120), width=S)
    gd.rounded_rectangle(((x0 - 2) * S, (y0 - 2) * S, (x1 + 2) * S, (y1 + 2) * S), radius=(r + 1) * S, outline=(0, 0, 0, 220), width=2 * S)
    img = Image.alpha_composite(img, grad)
    d = BlendDraw(globals(), "img")


for y in MODE_Y:
    bezel((MODE_X[0], y, MODE_X[1], y + MODE_H), 9)
bezel(AG_BTN, 9)


def lcd(box, r=6):
    """sun-tinted LCD window (content painted live)"""
    global img, d
    inset(box, r, fill=(9, 8, 6, 255), outline=(74, 68, 54, 170), shadow=230)
    x0, y0, x1, y1 = box
    g = np.zeros(((y1 - y0 - 4) * S, (x1 - x0 - 4) * S, 4), np.float32)
    g[..., :3] = (20, 17, 6)
    gy, gx = np.mgrid[0:g.shape[0], 0:g.shape[1]]
    g[..., :3] += (np.exp(-(((gx - g.shape[1] * 0.5) / (g.shape[1] * 0.7)) ** 2 + ((gy - g.shape[0] * 0.4) / (g.shape[0] * 0.9)) ** 2)) * 7)[..., None]
    g[::3, :, :3] *= 0.72
    g[..., 3] = 255
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=(r - 2) * S, fill=255)
    img.paste(gi, ((x0 + 2) * S, (y0 + 2) * S), m)
    d = BlendDraw(globals(), "img")
    gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(gl).polygon([((x0 + 3) * S, (y0 + 3) * S), ((x0 + (x1 - x0) * 0.42) * S, (y0 + 3) * S),
                                ((x0 + (x1 - x0) * 0.22) * S, (y1 - 3) * S), ((x0 + 3) * S, (y1 - 3) * S)], fill=(255, 255, 255, 7))
    img = Image.alpha_composite(img, gl)
    d = BlendDraw(globals(), "img")


lcd(ENH_LCD)
lcd(MET_LCD)

# ---------- small knob seats ----------
for name, (cx, cy) in SMALL.items():
    label(name, cx, cy - 53, lab, spacing=0.6)
    for k in range(21):
        a = math.radians(225 - 270 * k / 20)
        x, y = cx + math.cos(a) * 29, cy - math.sin(a) * 29
        d.ellipse(((x - 1.6) * S, (y - 1.6) * S, (x + 1.6) * S, (y + 1.6) * S), fill=(48, 44, 38, 255))
    d.ellipse(((cx - 24) * S, (cy - 24) * S, (cx + 24) * S, (cy + 24) * S), fill=(7, 7, 8, 255))
label("DARK", SMALL["COLOUR"][0] - 30, SMALL["COLOUR"][1] + 23, tiny, INK + (200,))
label("BRIGHT", SMALL["COLOUR"][0] + 32, SMALL["COLOUR"][1] + 23, tiny, INK + (200,))
label("DENSE", SMALL["CONTRAST"][0] - 31, SMALL["CONTRAST"][1] + 23, tiny, INK + (200,))
label("PUNCH", SMALL["CONTRAST"][0] + 31, SMALL["CONTRAST"][1] + 23, tiny, INK + (200,))
label("LOUDNESS", (AG_BTN[0] + AG_BTN[2]) / 2, SMALL["OUTPUT"][1] - 53, lab, spacing=0.6)
d.line((((SMALL["CONTRAST"][0] + SMALL["COLOUR"][0]) / 2) * S, 140 * S, ((SMALL["CONTRAST"][0] + SMALL["COLOUR"][0]) / 2) * S, 226 * S), fill=(52, 48, 42, 255), width=S)
d.line(((R_PANEL[0] + 16) * S, 238 * S, (R_PANEL[2] - 16) * S, 238 * S), fill=(52, 48, 42, 255), width=S)

# ---------- the big knob seat: machined well + sunburst slots ----------
cx, cy = KNOB_C


def ray_poly(i, r0, r1, widen=0.0):
    f = i / (N_RAYS - 1)
    a = math.radians(225 - 270 * f)
    long_ = (i % 5) == 0
    w0, w1 = (2.3 if long_ else 1.7) + widen, (4.6 if long_ else 3.2) + widen   # half widths (base px) at inner / outer radius
    r1 = r1 if r1 else (RAY_LONG if long_ else RAY_SHORT)
    ux, uy = math.cos(a), -math.sin(a)
    px, py = -uy, ux
    return [(r0 * ux + w0 * px, r0 * uy + w0 * py), (r1 * ux + w1 * px, r1 * uy + w1 * py),
            (r1 * ux - w1 * px, r1 * uy - w1 * py), (r0 * ux - w0 * px, r0 * uy - w0 * py)]


# outer bezel ring
R_OUT = 184
yy2, xx2 = np.mgrid[0:(2 * R_OUT + 8) * S, 0:(2 * R_OUT + 8) * S].astype(np.float32)
cc = (R_OUT + 4) * S
rr2 = np.sqrt((xx2 - cc) ** 2 + (yy2 - cc) ** 2) / S
ang = np.arctan2(yy2 - cc, xx2 - cc)
plate = np.zeros(rr2.shape + (4,), np.float32)
lit = 0.5 + 0.5 * np.cos(ang - math.radians(-135))
inside = rr2 <= R_OUT
# brushed dark plate with concentric lathe marks
lathe = rng.normal(0, 1, 4096)
lathe = np.convolve(lathe, np.ones(5) / 5, mode="same")
idx = np.clip((rr2 / R_OUT * 4095).astype(int), 0, 4095)
plate[..., :3] = (20, 19, 18)
plate[..., :3] += (lathe[idx] * 3.0)[..., None]
plate[..., :3] += (np.abs(np.cos(ang - math.radians(-135))) ** 8 * 10 * (rr2 / R_OUT))[..., None]
rim = (rr2 > R_OUT - 2.5) & inside
plate[..., :3] = np.where(rim[..., None], (np.array([52, 48, 42]) + 90 * lit[..., None] ** 2), plate[..., :3])
plate[..., 3] = np.where(inside, 255, 0) * np.clip(R_OUT - rr2 + 0.5, 0, 1)
plate_img = Image.fromarray(np.clip(plate, 0, 255).astype(np.uint8), "RGBA")
# drop shadow of the plate
shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(shadow).ellipse(((cx - R_OUT) * S, (cy - R_OUT + 5) * S, (cx + R_OUT) * S, (cy + R_OUT + 5) * S), fill=(0, 0, 0, 170))
shadow = shadow.filter(ImageFilter.GaussianBlur(9 * S))
img = Image.alpha_composite(img, shadow)
img.alpha_composite(plate_img, (int((cx - R_OUT - 4) * S), int((cy - R_OUT - 4) * S)))
d = BlendDraw(globals(), "img")

# engraved ray slots (unlit)
slots = Image.new("RGBA", (W, H), (0, 0, 0, 0))
sd = ImageDraw.Draw(slots)
lips = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ld = ImageDraw.Draw(lips)
for i in range(N_RAYS):
    poly = [((cx + x) * S, (cy + y) * S) for x, y in ray_poly(i, RAY_R0, 0)]
    sd.polygon(poly, fill=(6, 6, 6, 255))
    lpoly = [((cx + x) * S, (cy + y + 0.8) * S) for x, y in ray_poly(i, RAY_R0, 0)]
    ld.polygon(lpoly, fill=(120, 110, 92, 60))
img = Image.alpha_composite(img, lips)
img = Image.alpha_composite(img, slots)
# faint unlit phosphor inside each slot
dim = Image.new("RGBA", (W, H), (0, 0, 0, 0))
dd = ImageDraw.Draw(dim)
for i in range(N_RAYS):
    poly = [((cx + x) * S, (cy + y) * S) for x, y in ray_poly(i, RAY_R0 + 2, (RAY_LONG if i % 5 == 0 else RAY_SHORT) - 2, -0.9)]
    dd.polygon(poly, fill=(60, 52, 22, 255))
img = Image.alpha_composite(img, dim)
d = BlendDraw(globals(), "img")

# inner light-ring groove (the editor paints the glow on top)
d.ellipse(((cx - RING_R - 4) * S, (cy - RING_R - 4) * S, (cx + RING_R + 4) * S, (cy + RING_R + 4) * S), outline=(5, 5, 5, 255), width=6 * S)
d.ellipse(((cx - RING_R - 1.5) * S, (cy - RING_R - 1.5) * S, (cx + RING_R + 1.5) * S, (cy + RING_R + 1.5) * S), outline=(70, 60, 26, 255), width=int(1.5 * S))

# scale captions at the ends of the arc
for frac, t in ((0.0, "0"), (0.5, "50"), (1.0, "100")):
    a = math.radians(225 - 270 * frac)
    tx, ty = cx + math.cos(a) * 192, cy - math.sin(a) * 192
    if frac == 0.5:
        ty -= 2
    tw = d.textlength(t, font=small)
    d.text((tx * S - tw / 2, (ty - 7) * S), t, font=small, fill=INK + (255,))
lcd(GLOW_LCD)
label("G L O W   A M O U N T", cx, GLOW_LCD[3] + 9, tiny, INK + (210,))

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

# ---------- lit sunburst overlay (the editor reveals it up to the knob value) ----------
LS = 2 * LIT_HALF * S
core = Image.new("L", (LS, LS), 0)
cd = ImageDraw.Draw(core)
for i in range(N_RAYS):
    poly = [((LIT_HALF + x) * S, (LIT_HALF + y) * S) for x, y in ray_poly(i, RAY_R0 + 1.5, (RAY_LONG if i % 5 == 0 else RAY_SHORT) - 1.5, -0.6)]
    cd.polygon(poly, fill=255)
core_a = np.asarray(core, np.float32) / 255.0
lit = np.zeros((LS, LS, 4), np.float32)
# bloom layers
for rad, amt in ((14 * S, 0.55), (5 * S, 0.75), (1.5 * S, 0.9)):
    b = np.asarray(core.filter(ImageFilter.GaussianBlur(rad)), np.float32) / 255.0
    lit[..., 3] = np.maximum(lit[..., 3], np.clip(b * amt * (2.2 if rad > 10 else 1.6), 0, 1))
lit[..., :3] = SUN
# white-hot core along the ray centre, radial gradient (hotter at the inner end)
yy3, xx3 = np.mgrid[0:LS, 0:LS].astype(np.float32)
rad = np.sqrt((xx3 - LS / 2) ** 2 + (yy3 - LS / 2) ** 2) / S
hot = np.clip(1.0 - (rad - RAY_R0) / 40.0, 0.35, 1.0)
coreb = np.asarray(core.filter(ImageFilter.GaussianBlur(0.8 * S)), np.float32) / 255.0
white = np.array([255, 252, 228], np.float32)
mixw = (coreb * hot)[..., None]
lit[..., :3] = lit[..., :3] * (1 - mixw) + white * mixw
lit[..., 3] = np.maximum(lit[..., 3], coreb)
lit[..., 3] *= 255
Image.fromarray(np.clip(lit, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(ROOT, "Resources", "rays_lit.png"), optimize=True)
print("wrote rays_lit.png", LS)

# ---------- knobs (static image; the editor draws the rotating pointer) ----------
render_knob(560, "black", seed=4).save(os.path.join(ROOT, "Resources", "knob_big.png"), optimize=True)
render_knob(128, "black", seed=5).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("knobs done")
