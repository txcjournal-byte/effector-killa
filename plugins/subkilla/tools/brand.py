"""TrapWin brand helpers shared by the plugin background generators and the store cover generator.

chrome_title(text, height, glow=(r,g,b))  -> RGBA image: heavy italic chrome wordmark (TrapWin house style)
trapwin_logo(height)                       -> RGBA image of the official TrapWin logo (chrome + sparkles)
sparkle(size)                              -> RGBA four-point star
"""
import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))


def _find(name):
    for p in (os.path.join(HERE, name), os.path.join(HERE, "..", "Resources", "fonts", name),
              os.path.join(HERE, "fonts", name)):
        if os.path.exists(p):
            return p
    raise FileNotFoundError(name)


def trapwin_logo(height=None, tagline=True):
    im = Image.open(_find("trapwin_logo.png")).convert("RGBA")
    if not tagline:
        im = im.crop((0, 0, im.width, int(im.height * 0.80)))
    im = im.crop(im.getbbox())
    if height:
        im = im.resize((max(1, round(im.width * height / im.height)), height), Image.LANCZOS)
    return im


def sparkle(size, col=(255, 255, 255)):
    S = size * 4
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    c = (S - 1) / 2
    dx, dy = np.abs(xx - c) / c, np.abs(yy - c) / c
    star = np.clip(1.0 - (np.sqrt(dx) + np.sqrt(dy)) / 1.0, 0, 1) ** 0.6
    core = np.exp(-((dx ** 2 + dy ** 2) / 0.02))
    a = np.clip(star * 1.6 + core * 0.8, 0, 1)
    out = np.zeros((S, S, 4), np.uint8)
    out[..., :3] = col
    out[..., 3] = (a * 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA").resize((size, size), Image.LANCZOS)


def chrome_title(text, height, glow=(70, 130, 255), font="ArchivoBlack-Regular.ttf", slant=0.22,
                 tracking=0.0, glow_strength=1.0, tint=(0, 0, 0)):
    """Chrome italic wordmark. height = cap height in px. Returns a tightly padded RGBA image."""
    f = ImageFont.truetype(_find(font), int(height * 1.38))
    # measure + draw text mask with optional tracking
    tmp = ImageDraw.Draw(Image.new("L", (10, 10)))
    widths = [tmp.textlength(ch, font=f) for ch in text]
    total = sum(widths) + tracking * height * (len(text) - 1)
    pad = int(height * 0.9)
    W, H = int(total + pad * 2 + height * slant * 1.5), int(height * 1.38 * 1.3 + pad * 2)
    mask = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(mask)
    x = pad
    for ch, w in zip(text, widths):
        md.text((x, pad), ch, font=f, fill=255)
        x += w + tracking * height
    bb = mask.getbbox()
    # italic shear about the baseline
    base_y = bb[3]
    mask = mask.transform(mask.size, Image.AFFINE, (1, slant, -slant * base_y, 0, 1, 0), Image.BICUBIC)
    bb = mask.getbbox()
    top, bot = bb[1], bb[3]
    h = bot - top

    # chrome gradient (sky -> horizon line -> ground reflection)
    t = (np.arange(H, dtype=np.float32) - top) / max(1, h)
    stops = [(0.00, (250, 251, 255)), (0.30, (214, 222, 238)), (0.50, (150, 165, 196)), (0.53, (58, 74, 112)),
             (0.58, (96, 128, 196)), (0.72, (206, 216, 236)), (0.90, (240, 244, 252)), (1.00, (176, 188, 214))]
    col = np.zeros((H, 3), np.float32)
    for i in range(3):
        col[:, i] = np.interp(t, [s[0] for s in stops], [s[1][i] for s in stops])
    col = np.clip(col + np.array(tint, np.float32), 0, 255)
    grad = np.broadcast_to(col[:, None, :], (H, W, 3)).copy()
    # diagonal sheen
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    sheen = np.exp(-(((xx - yy * 1.4) - W * 0.30) / (W * 0.06)) ** 2) * 40
    grad += sheen[..., None]
    m = np.asarray(mask, np.float32) / 255.0

    # bevel: lighten top edges, darken bottom edges using the mask gradient
    blur = np.asarray(mask.filter(ImageFilter.GaussianBlur(max(1, height * 0.025))), np.float32) / 255.0
    gy = np.zeros_like(blur)
    gy[1:-1] = blur[2:] - blur[:-2]
    grad += (np.clip(gy, 0, 1) * 260)[..., None]       # top edge highlight
    grad -= (np.clip(-gy, 0, 1) * 220)[..., None]      # bottom edge shade
    grad = np.clip(grad, 0, 255)

    letters = np.dstack([grad, m * 255]).astype(np.uint8)
    letters = Image.fromarray(letters, "RGBA")

    # dark outline for punch
    outline = mask.filter(ImageFilter.MaxFilter(max(3, int(height * 0.03) | 1)))
    ol = Image.new("RGBA", (W, H), (8, 12, 22, 255))
    ol.putalpha(outline)

    # glow
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for rad, a in ((height * 0.45, 0.55), (height * 0.12, 0.55)):
        g = mask.filter(ImageFilter.GaussianBlur(rad))
        ga = (np.asarray(g, np.float32) * a * glow_strength).clip(0, 255).astype(np.uint8)
        gl = Image.new("RGBA", (W, H), glow + (255,))
        gl.putalpha(Image.fromarray(ga))
        out = Image.alpha_composite(out, gl)
    out = Image.alpha_composite(out, ol)
    out = Image.alpha_composite(out, letters)
    return out.crop(out.getbbox())


def place(base, overlay, x, y):
    """alpha-composite overlay onto base at (x, y); negative / overhanging offsets are fine. Returns new image."""
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    layer.paste(overlay, (int(round(x)), int(round(y))), overlay)
    return Image.alpha_composite(base.convert("RGBA"), layer)


def place_title(base, title_img, left, top):
    """place a chrome_title so that its LETTERS (not the glow) start at (left, top)."""
    x0, y0, _, _ = text_bbox_offset(title_img)
    return place(base, title_img, left - x0, top - y0)


def text_bbox_offset(img):
    """Glow makes the image larger than the letters; returns the letter box inside the image (approx)."""
    a = np.asarray(img.getchannel("A"))
    ys, xs = np.where(a > 200)
    return xs.min(), ys.min(), xs.max(), ys.max()


if __name__ == "__main__":
    bg = Image.new("RGBA", (1400, 700), (10, 14, 26, 255))
    t = chrome_title("DUST TAPE", 150, glow=(255, 150, 60))
    bg.alpha_composite(t, (40, 20))
    lg = trapwin_logo(120)
    bg.alpha_composite(lg, (40, 400))
    bg.alpha_composite(sparkle(60), (1200, 420))
    bg.convert("RGB").save("brand_test.png")
    print("ok", t.size, lg.size)
