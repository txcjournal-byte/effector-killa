"""Replaces the '808 KILLA' logo in Resources/background.jpg with 'DRUM KILLA' (stone texture + red glow).
Original art is kept in Resources/source/background_808.jpg.  Run: python3 tools/make_drumkilla_logo.py"""
import os, shutil
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = os.path.join(ROOT, "Resources", "source", "background_808.jpg")
dst = os.path.join(ROOT, "Resources", "background.jpg")
if not os.path.exists(src):
    shutil.copy(dst, src)
bg = Image.open(src).convert("RGB")
W, H = bg.size
band_h = 262

# 1) smoke over the old logo: blurred + darkened copy of the band, fading out at the bottom
band = bg.crop((0, 0, W, band_h)).filter(ImageFilter.GaussianBlur(28))
a = np.asarray(band, np.float32)
a *= np.array([0.42, 0.20, 0.20])           # dark, keep a red cast
a = np.clip(a, 0, 255)
smoke = Image.fromarray(a.astype(np.uint8))
fade = Image.linear_gradient("L").resize((W, band_h)).point(lambda v: 255 if v < 170 else int(255 * (255 - v) / 85))
out = bg.copy()
out.paste(smoke, (0, 0), fade)

# 2) text mask
font = ImageFont.truetype(os.path.join(ROOT, "tools", "MetalMania-Regular.ttf"), 230)
text = "DRUM KILLA"
tmp = Image.new("L", (W, band_h + 60), 0)
d = ImageDraw.Draw(tmp)
bbox = d.textbbox((0, 0), text, font=font)
tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
scale = min((W - 140) / tw, 210 / th)
font = ImageFont.truetype(os.path.join(ROOT, "tools", "MetalMania-Regular.ttf"), int(230 * scale))
bbox = d.textbbox((0, 0), text, font=font)
tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
x, y = (W - tw) // 2 - bbox[0], 22 - bbox[1]
mask = Image.new("L", (W, H), 0)
ImageDraw.Draw(mask).text((x, y), text, font=font, fill=255)

# 3) red glow behind
glow = mask.filter(ImageFilter.MaxFilter(15)).filter(ImageFilter.GaussianBlur(22))
red = Image.new("RGB", (W, H), (255, 30, 20))
out = Image.composite(red, out, glow.point(lambda v: int(v * 0.85)))
# dark outline
outline = mask.filter(ImageFilter.MaxFilter(9))
out = Image.composite(Image.new("RGB", (W, H), (12, 6, 6)), out, outline)

# 4) stone fill taken from the original logo (grey cracked stone), tiled
stone = bg.crop((560, 40, 760, 200)).convert("RGB")
sa = np.asarray(stone, np.float32)
grey = sa.mean(axis=2, keepdims=True)
sa = grey * 0.85 + sa * 0.15                    # mostly grey, a hint of the red cracks
stone = Image.fromarray(np.clip(sa * 1.15, 0, 255).astype(np.uint8))
tile = Image.new("RGB", (W, H))
for ty in range(0, H, stone.size[1]):
    for tx in range(0, W, stone.size[0]):
        tile.paste(stone if (tx // stone.size[0] + ty // stone.size[1]) % 2 == 0 else stone.transpose(Image.FLIP_LEFT_RIGHT), (tx, ty))
# bevel: light from top-left
emb = mask.filter(ImageFilter.GaussianBlur(3))
e = np.asarray(emb, np.float32)
gy, gx = np.gradient(e)
shade = np.clip(1.0 + (-gx - gy) * 0.035, 0.45, 1.6)[..., None]
lit = np.clip(np.asarray(tile, np.float32) * shade, 0, 255).astype(np.uint8)
out = Image.composite(Image.fromarray(lit), out, mask)
# red hot inner edge
edge = ImageChops.subtract(mask, mask.filter(ImageFilter.MinFilter(5))).filter(ImageFilter.GaussianBlur(1.5))
out = Image.composite(Image.new("RGB", (W, H), (255, 60, 40)), out, edge.point(lambda v: int(v * 0.55)))

out.save(dst, quality=92)
print("wrote", dst, out.size)
