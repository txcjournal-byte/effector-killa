"""Shop cover in the TrapWin plugin style (same layout as the TIMER cover):
small "PLUGIN" / "VST3 · AU" corners, big chrome EFFECTOR, subtitle, UI render with an ice-blue
glow frame, "TRAPWIN · AUDIO UNDERGROUND" footer. No big TRAPWIN logo on the cover.

usage: python3 -I Tools/make_cover.py <ui_2x.png> <out.png>
The UI render comes from the snapshot tool (00_cover_ui.png).
"""
import sys, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ui_p, out_p = sys.argv[1:3]
F = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Resources', 'Fonts') + os.sep
W = H = 1600

yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
bg = np.zeros((H, W, 3), np.float32); bg[:] = (5, 7, 12)
bg += np.exp(-(((xx - 800) / 760) ** 2 + ((yy - 1000) / 560) ** 2))[..., None] * np.array([22, 48, 105])
bg += np.exp(-(((xx - 800) / 700) ** 2 + ((yy - 300) / 260) ** 2))[..., None] * np.array([10, 18, 38])
im = Image.fromarray(bg.clip(0, 255).astype(np.uint8))
d = ImageDraw.Draw(im)

mono = ImageFont.truetype(F + 'vfd.ttf', 30)
d.text((70, 60), 'PLUGIN', font=mono, fill=(150, 165, 190))
t = 'VST3 · AU'
d.text((W - 70 - d.textlength(t, font=mono), 60), t, font=mono, fill=(150, 165, 190))


def chrome_title(im, text, size, cy, shear=0.12):
    f = ImageFont.truetype(F + 'display.ttf', size)
    tmp = Image.new('L', (W * 2, size * 2), 0)
    td = ImageDraw.Draw(tmp)
    tw = td.textlength(text, font=f)
    td.text((W - tw / 2, size * 0.3), text, font=f, fill=255)
    tmp = tmp.transform(tmp.size, Image.AFFINE, (1, shear, -shear * size, 0, 1, 0), resample=Image.BICUBIC)
    tmp = tmp.crop(tmp.getbbox())
    w, h = tmp.size
    x0, y0 = (W - w) // 2, int(cy - h / 2)
    mask = Image.new('L', (W, H), 0); mask.paste(tmp, (x0, y0))
    grad = np.zeros((H, W, 3), np.float32)
    stops = [(0, (246, 248, 252)), (0.46, (206, 213, 226)), (0.54, (128, 140, 162)), (1, (232, 236, 244))]
    for y in range(y0, y0 + h):
        t = (y - y0) / max(1, h)
        for (a, ca), (b, cb) in zip(stops, stops[1:]):
            if a <= t <= b:
                u = (t - a) / (b - a)
                grad[y] = [ca[j] + (cb[j] - ca[j]) * u for j in range(3)]
    glow = np.asarray(mask.filter(ImageFilter.GaussianBlur(22)))[..., None] / 255.0
    base = np.asarray(im).astype(np.float32) + glow * np.array([70, 118, 196]) * 0.9
    m = np.asarray(mask)[..., None] / 255.0
    return Image.fromarray((base * (1 - m) + grad * m).clip(0, 255).astype(np.uint8))


im = chrome_title(im, 'EFFECTOR', 210, 270)
d = ImageDraw.Draw(im)
f1 = ImageFont.truetype(F + 'osd.ttf', 44)
f2 = ImageFont.truetype(F + 'vfd.ttf', 32)


def centre(t, y, f, col):
    d.text(((W - d.textlength(t, font=f)) / 2, y), t, font=f, fill=col)


centre('TRAP MULTI-EFFECT', 425, f1, (230, 236, 247))
centre('WIN + MAC  ·  VST3 / AU', 488, f2, (159, 195, 255))

ui = Image.open(ui_p).convert('RGB')
tw = 1260
th = int(ui.height * tw / ui.width)
ui = ui.resize((tw, th), Image.LANCZOS)
x, y = (W - tw) // 2, 568
g = Image.new('L', (W, H), 0)
ImageDraw.Draw(g).rounded_rectangle((x - 6, y - 6, x + tw + 6, y + th + 6), radius=22, fill=255)
g = np.asarray(g.filter(ImageFilter.GaussianBlur(28)))[..., None] / 255.0
im = Image.fromarray((np.asarray(im).astype(np.float32) + g * np.array([60, 110, 210]) * 0.8).clip(0, 255).astype(np.uint8))
mask = Image.new('L', (tw, th), 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, tw - 1, th - 1), radius=16, fill=255)
im.paste(ui, (x, y), mask)
d = ImageDraw.Draw(im)
d.rounded_rectangle((x - 1, y - 1, x + tw, y + th), radius=16, outline=(120, 160, 230), width=2)
centre('TRAPWIN  ·  AUDIO UNDERGROUND', y + th + 40, f2, (150, 165, 190))
im.save(out_p)
print('wrote', out_p, 'UI', tw, 'x', th, 'bottom', y + th)
