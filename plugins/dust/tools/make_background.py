"""Generates the DUST editor background (Resources/bg.png) at 2x of the 780x440 base size.

Everything static is painted here (chassis texture, panels, cassette window, VU faces, knob seats,
engraved labels). The editor draws knobs, needles, reels and text values on top.
Run:  python3 tools/make_background.py
"""
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = 2  # scale
W, H = 780 * S, 440 * S
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(7)
random.seed(7)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), int(size * S))


def noise(h, w, amount):
    return rng.normal(0.0, amount, (h, w, 1))


def clamp_img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def rr(draw, box, r, **kw):
    x0, y0, x1, y1 = [v * S for v in box]
    draw.rounded_rectangle((x0, y0, x1, y1), radius=r * S, **kw)


# ---------- chassis: warm dark leatherette with grain ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (31, 24, 19)
yy, xx = np.mgrid[0:H, 0:W]
# soft top-left light and vignette
light = 1.0 + 0.10 * (1 - (xx / W) * 0.6 - (yy / H) * 0.8)
vig = 1.0 - 0.35 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.4
base *= (light * vig)[..., None]
# leather pebble texture: blurred noise
peb = Image.fromarray(np.uint8(np.clip(128 + rng.normal(0, 60, (H // 2, W // 2)), 0, 255)))
peb = peb.resize((W, H), Image.BICUBIC).filter(ImageFilter.GaussianBlur(1.2))
base += (np.asarray(peb, np.float32)[..., None] - 128) * 0.06
base += noise(H, W, 3.0)
img = clamp_img(base).convert("RGBA")
d = ImageDraw.Draw(img)

# ---------- header faceplate: brushed champagne aluminium ----------
hdr_h = 150 * S
brushed = np.zeros((hdr_h, W, 3), np.float32)
brushed[:] = (58, 50, 42)
streaks = rng.normal(0, 1, (hdr_h, 1)) * 5 + rng.normal(0, 1, (hdr_h, W)) * 2.5
streaks = np.asarray(Image.fromarray(np.uint8(np.clip(128 + streaks * 6, 0, 255))).filter(
    ImageFilter.BoxBlur(0)), np.float32) - 128
brushed += streaks[..., None] * 0.35
grad = np.linspace(1.12, 0.82, hdr_h)[:, None, None]
brushed *= grad
hdr = clamp_img(brushed).convert("RGBA")
mask = Image.new("L", (W, hdr_h), 0)
ImageDraw.Draw(mask).rounded_rectangle((8 * S, 8 * S, W - 8 * S, hdr_h - 4 * S), radius=10 * S, fill=255)
img.paste(hdr, (0, 0), mask)
# bevel lines
d.rounded_rectangle((8 * S, 8 * S, W - 8 * S, hdr_h - 4 * S), radius=10 * S, outline=(120, 104, 86, 110), width=S)
d.line((14 * S, hdr_h - 4 * S, W - 14 * S, hdr_h - 4 * S), fill=(0, 0, 0, 160), width=2 * S)

# ---------- logo: TrapWin chrome title + official TrapWin logo ----------
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from brand import chrome_title, place, place_title, trapwin_logo
img = place_title(img, chrome_title("DUST TAPE", 30 * S, glow=(255, 140, 50), glow_strength=0.75), 26 * S, 20 * S)
_lg = trapwin_logo(44 * S)
img = place(img, _lg, 22 * S, 58 * S)
d = ImageDraw.Draw(img)
sub = font("BarlowCondensed-SemiBold.ttf", 13)
d.text((22 * S + _lg.width + 12 * S, 72 * S), "LO-FI TAPE MACHINE", font=sub, fill=(227, 155, 60, 255))

# ---------- cassette window ----------
cx0, cy0, cx1, cy1 = 330, 20, 630, 132
rr(d, (cx0 - 3, cy0 - 3, cx1 + 3, cy1 + 3), 12, fill=(14, 11, 9, 255))
rr(d, (cx0, cy0, cx1, cy1), 10, fill=(22, 17, 13, 255), outline=(96, 84, 70, 255), width=S)
# cassette shell + label
rr(d, (cx0 + 14, cy0 + 8, cx1 - 14, cy1 - 8), 8, fill=(40, 34, 29, 255))
rr(d, (cx0 + 22, cy0 + 12, cx1 - 22, cy0 + 34), 4, fill=(232, 222, 198, 255))
d.rectangle(((cx0 + 22) * S, (cy0 + 27) * S, (cx1 - 22) * S, (cy0 + 31) * S), fill=(180, 81, 45, 255))
lab = font("BarlowCondensed-SemiBold.ttf", 11)
d.text(((cx0 + 30) * S, (cy0 + 13) * S), "TRAPWIN  C-90", font=lab, fill=(30, 24, 20, 255))
d.text(((cx1 - 92) * S, (cy0 + 13) * S), "TYPE II  ·  HIGH BIAS", font=font("BarlowCondensed-SemiBold.ttf", 9),
       fill=(90, 70, 55, 255))
# reel windows (reels themselves are animated in the editor)
for rx in (cx0 + 72, cx1 - 72):
    cy = (cy0 + cy1) / 2 + 12
    d.ellipse(((rx - 34) * S, (cy - 34) * S, (rx + 34) * S, (cy + 34) * S), fill=(10, 8, 6, 255))
# tape window between reels
d.rounded_rectangle(((cx0 + 108) * S, (cy0 + 64) * S, (cx1 - 108) * S, (cy0 + 86) * S), radius=3 * S,
                    fill=(12, 9, 7, 255), outline=(70, 60, 50, 255), width=S)
# glass reflection
glass = Image.new("RGBA", (W, H), (0, 0, 0, 0))
gd = ImageDraw.Draw(glass)
gd.polygon([((cx0 + 20) * S, cy0 * S), ((cx0 + 120) * S, cy0 * S), ((cx0 + 60) * S, cy1 * S), ((cx0 - 20) * S, cy1 * S)],
           fill=(255, 255, 255, 18))
img = Image.alpha_composite(img, glass)
d = ImageDraw.Draw(img)

# ---------- VU meter faces ----------
VU = [(646, 18, 766, 74, "IN"), (646, 80, 766, 136, "OUT")]
vu_font = font("BarlowCondensed-SemiBold.ttf", 9)
for (x0, y0, x1, y1, name) in VU:
    rr(d, (x0 - 2, y0 - 2, x1 + 2, y1 + 2), 5, fill=(12, 10, 8, 255))
    face = Image.new("RGBA", ((x1 - x0) * S, (y1 - y0) * S))
    fa = np.zeros(((y1 - y0) * S, (x1 - x0) * S, 4), np.float32)
    fy = np.linspace(0, 1, (y1 - y0) * S)[:, None]
    fa[..., 0] = 236 - 30 * fy
    fa[..., 1] = 214 - 34 * fy
    fa[..., 2] = 160 - 40 * fy
    fa[..., 3] = 255
    face = clamp_img(fa).convert("RGBA")
    fm = Image.new("L", face.size, 0)
    ImageDraw.Draw(fm).rounded_rectangle((0, 0, face.size[0] - 1, face.size[1] - 1), radius=4 * S, fill=255)
    img.paste(face, (x0 * S, y0 * S), fm)
    d = ImageDraw.Draw(img)
    # scale arc: pivot below the face; classic VU spacing (fraction of the sweep per dB mark)
    px, py = (x0 + x1) / 2, y1 + 30
    R = 52
    marks = [(-20, 0.00), (-10, 0.17), (-7, 0.29), (-5, 0.39), (-3, 0.52), (-2, 0.59), (-1, 0.66), (0, 0.74), (1, 0.82), (2, 0.90), (3, 1.00)]
    num = font("BarlowCondensed-SemiBold.ttf", 7.5)
    for db, t in marks:
        a = math.radians(-48 + 96 * t)
        big = db in (-20, -10, -5, 0, 3)
        x_in, y_in = px + math.sin(a) * (R - 5), py - math.cos(a) * (R - 5)
        x_out, y_out = px + math.sin(a) * (R + (2 if big else 0)), py - math.cos(a) * (R + (2 if big else 0))
        col = (176, 52, 32, 255) if db > 0 else (34, 27, 22, 255)
        d.line((x_in * S, y_in * S, x_out * S, y_out * S), fill=col, width=S + (1 if big else 0))
        if big:
            lx, ly = px + math.sin(a) * (R + 7), py - math.cos(a) * (R + 7)
            txt = ("+" if db > 0 else "") + str(db)
            tw = d.textlength(txt, font=num)
            d.text((lx * S - tw / 2, (ly - 5) * S), txt, font=num, fill=col)
    # red zone arc from 0 to +3
    a0 = -90 - 48 + 96 * 0.74
    d.arc(((px - R + 2) * S, (py - R + 2) * S, (px + R - 2) * S, (py + R - 2) * S), start=a0, end=-90 + 48, fill=(176, 52, 32, 255), width=2 * S)
    lbl = name + "   VU"
    tw = d.textlength(lbl, font=vu_font)
    d.text((((x0 + x1) / 2) * S - tw / 2, (y1 - 13) * S), lbl, font=vu_font, fill=(60, 48, 38, 255))
    # glass shine
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(sh).rectangle((x0 * S, y0 * S, x1 * S, (y0 + (y1 - y0) * 0.45) * S), fill=(255, 255, 255, 22))
    img = Image.alpha_composite(img, sh)
    d = ImageDraw.Draw(img)

# ---------- lower panels: recessed with inner shadow ----------
def inset_panel(box, radius=12):
    global img, d
    x0, y0, x1, y1 = box
    # inner fill slightly lighter + shadow at top edge
    rr(d, (x0, y0, x1, y1), radius, fill=(38, 30, 24, 255))
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh)
    sd.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=radius * S, outline=(0, 0, 0, 170), width=5 * S)
    sh = sh.filter(ImageFilter.GaussianBlur(5 * S))
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=radius * S, fill=255)
    sh.putalpha(Image.fromarray(np.minimum(np.asarray(sh.getchannel("A")), np.asarray(m))))
    img = Image.alpha_composite(img, sh)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=radius * S, outline=(86, 72, 58, 120), width=S)


inset_panel((16, 164, 256, 426))
inset_panel((268, 164, 764, 426))

# ---------- knob seats with tick scales ----------
def seat(cx, cy, r, ticks=11, major=(0, 5, 10)):
    for i in range(ticks):
        a = math.radians(225 - 270 * i / (ticks - 1))
        r0, r1 = r + 3, r + (8 if i in major else 5)
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S,
                (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(200, 186, 160, 200 if i in major else 120), width=S)
    # recessed ring
    d.ellipse(((cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S), fill=(18, 14, 11, 255))
    d.ellipse(((cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S), outline=(70, 58, 46, 255), width=S)


# big AGE knob (centre of slider bounds 36,196,200,190 -> centre 136, 291)
seat(136, 287, 84, ticks=21, major=(0, 5, 10, 15, 20))
# 8 small knobs: x0=280, colW=118, rows 172/296; slider (x+9, y+18, 100, 100) -> centre (x+59, y+62)
for i in range(8):
    col, row = i % 4, i // 4
    x = 280 + col * 118
    y = 172 if row == 0 else 296
    seat(x + 59, y + 62, 36)

# ---------- value windows under knobs ----------
def window(cx, top, w=64, h=17):
    rr(d, (cx - w / 2 - 1, top - 1, cx + w / 2 + 1, top + h + 1), 3, fill=(70, 58, 46, 255))
    rr(d, (cx - w / 2, top, cx + w / 2, top + h), 3, fill=(12, 10, 8, 255))


window(136, 386, 76, 18)
for i in range(8):
    col, row = i % 4, i // 4
    x = 280 + col * 118
    y = 172 if row == 0 else 296
    window(x + 59, y + 104)

# ---------- screws ----------
def screw(cx, cy):
    d.ellipse(((cx - 5) * S, (cy - 5) * S, (cx + 5) * S, (cy + 5) * S), fill=(120, 110, 96, 255), outline=(30, 24, 20, 255), width=S)
    a = random.uniform(0, math.pi)
    d.line(((cx - math.cos(a) * 4) * S, (cy - math.sin(a) * 4) * S, (cx + math.cos(a) * 4) * S, (cy + math.sin(a) * 4) * S),
           fill=(40, 34, 28, 255), width=S + 1)


for (sx, sy) in ((18, 18), (762, 18), (18, 140), (762, 140), (22, 432), (758, 432)):
    if sy < 150 and 300 < sx < 640:
        continue
    screw(sx, sy)

# fine film grain over everything
grain = clamp_img(128 + noise(H, W, 9.0)[..., 0][..., None].repeat(3, 2)).convert("RGBA")
grain.putalpha(10)
img = Image.alpha_composite(img, grain)

out = os.path.join(ROOT, "Resources", "bg.png")
img.convert("RGB").save(out, optimize=True)
print("wrote", out, img.size)
