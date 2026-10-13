"""TrapWin bundle covers (2048x2048) + 9:16 promo stills (1080x1920) for every product and bundle.

    python3 make_bundle.py              # everything: 4 bundle covers + 20 vertical stills
    python3 make_bundle.py bundles      # only bundle covers
    python3 make_bundle.py vertical     # only 9:16 stills
    python3 make_bundle.py allin heat   # selected bundle / product keys (cover for bundles, 9:16 for both)
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from brand import chrome_title, place, sparkle, text_bbox_offset, trapwin_logo  # noqa: E402
from make_cover import (OUT_DIR, PILL_GREEN, PRODUCTS, chip, font, framed_ui, mix, pill,  # noqa: E402
                        spaced_text)

VERT_DIR = os.path.join(OUT_DIR, "vertical")
GOLD = (236, 196, 98)

BUNDLES = {
    "allin": dict(title="ALL IN", subtitle="EVERY TRAPWIN PLUGIN · 16 PLUGINS", accent=GOLD, bg_hue=(24, 18, 10),
                  price="$3,000+ VALUE  •  $99", chips=["16 PLUGINS", "WIN + MAC", "LIFETIME UPDATES"],
                  out="all-in-bundle-cover.jpg", tint=(14, 6, -26), multi=True, vscale=0.50,
                  rows=[dict(keys=["sweep", "glow", "halo", "throw", "auraverb", "ghostdelay", "chopper", "wide", "snap"],
                             w=600, spread=176, dy=-0.40, yaw=0.28, scale_k=0.035),
                        dict(keys=["heat", "pump", "vox", "autokilla", "drum", "subkilla", "dust"],
                             w=800, spread=228, dy=0.24, yaw=0.28, scale_k=0.05)]),
    "vocal": dict(title="VOCAL CHAIN", subtitle="TUNE · PITCH · DELAY · REVERB", accent=(70, 150, 255),
                  bg_hue=(10, 14, 30), price="$1,200+ VALUE  •  $79",
                  chips=["5 PLUGINS", "RADIO-READY VOCALS", "WIN + MAC"], out="vocal-bundle-cover.jpg", multi=True,
                  rows=[dict(keys=["ghostdelay", "vox", "autokilla", "throw", "auraverb"], w=1120, spread=335,
                             dy=0.08, yaw=0.45, scale_k=0.10)]),
    "beat": dict(title="BEAT MACHINE", subtitle="808 · DRUMS · PUMP · CHOPS", accent=(255, 50, 40), bg_hue=(26, 10, 10),
                 price="$1,000+ VALUE  •  $79", chips=["6 PLUGINS", "HARDER 808s & DRUMS", "WIN + MAC"],
                 out="beat-bundle-cover.jpg", multi=True,
                 rows=[dict(keys=["chopper", "heat", "snap"], w=720, spread=580, dy=-0.30, yaw=0.45, scale_k=0.0),
                       dict(keys=["pump", "drum", "subkilla"], w=960, spread=480, dy=0.22, yaw=0.55, scale_k=0.10)]),
    "free": dict(title="FREE PACK", subtitle="HALO REVERB + WIDE IMAGER", accent=(120, 200, 255), bg_hue=(10, 16, 30),
                 price="FREE DOWNLOAD", pill=PILL_GREEN, chips=["2 FREE PLUGINS", "NO CATCH", "WIN + MAC"],
                 out="free-bundle-cover.jpg", multi=True,
                 rows=[dict(keys=["halo", "wide"], w=1060, spread=830, dy=0.05, yaw=0.50, scale_k=0.0, inward=True)]),
}
VERT_NAMES = {"allin": "all-in-bundle", "vocal": "vocal-bundle", "beat": "beat-bundle", "free": "free-bundle"}


# ---------------------------------------------------------------- geometry
def _coeffs(dst, src):
    A, B = [], []
    for (x, y), (u, v) in zip(dst, src):
        A.append([x, y, 1, 0, 0, 0, -u * x, -u * y]); B.append(u)
        A.append([0, 0, 0, x, y, 1, -v * x, -v * y]); B.append(v)
    return np.linalg.solve(np.array(A, float), np.array(B, float))


def warp(img, quad, size):
    w, h = img.size
    c = _coeffs(quad, [(0, 0), (w, 0), (w, h), (0, h)])
    return img.transform(size, Image.PERSPECTIVE, tuple(c), Image.BICUBIC)


def shade(img, f):
    a = np.asarray(img).astype(np.float32)
    a[..., :3] *= f
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), "RGBA")


def card_layers(key, width, yaw, side, darken=1.0, glow=1.0, refl=0.32):
    """Returns (layer RGBA, x_off, y_off) where offsets map layer origin relative to the card centre.
    yaw 0..1, side -1 = card left of centre (its outer/left edge recedes), +1 = right."""
    cfg = PRODUCTS[key]
    acc = cfg["accent"]
    src = framed_ui(cfg["ui"], acc, width, width)
    w, h = src.size
    fw = w * (1 - 0.30 * yaw)          # foreshortened width
    fh = h * (1 - 0.20 * yaw)          # far edge height
    p = int(max(w, h) * 0.35)
    W, H = int(fw + 2 * p), int(h * 1.5 + 2 * p)
    x0, y0 = p, p
    near_top, near_bot = y0, y0 + h
    far_top, far_bot = y0 + (h - fh) / 2, y0 + (h + fh) / 2
    if side < 0:   # left edge far
        quad = [(x0, far_top), (x0 + fw, near_top), (x0 + fw, near_bot), (x0, far_bot)]
    else:
        quad = [(x0, near_top), (x0 + fw, far_top), (x0 + fw, far_bot), (x0, near_bot)]
    if darken < 1:
        src = shade(src, darken)
    card = warp(src, quad, (W, H))
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    # glow from the card silhouette
    a = card.getchannel("A")
    for rad, k in ((60, 0.85 * glow), (16, 0.9 * glow)):
        g = a.filter(ImageFilter.GaussianBlur(rad)).point(lambda v, k=k: int(min(255, v * k)))
        gl = Image.new("RGBA", (W, H), acc + (255,))
        gl.putalpha(g)
        out.alpha_composite(gl)
    # reflection: flipped source, faded, warped onto the mirrored quad
    if refl > 0:
        fl = src.transpose(Image.FLIP_TOP_BOTTOM)
        fa = np.asarray(fl).copy()
        grad = np.clip(1 - np.arange(h) / (h * 0.42), 0, 1) ** 1.6 * refl
        fa[..., 3] = (fa[..., 3] * grad[:, None]).astype(np.uint8)
        fl = Image.fromarray(fa, "RGBA")
        TL, TR, BR, BL = quad
        gap = 10
        rq = [(BL[0], BL[1] + gap), (BR[0], BR[1] + gap),
              (BR[0], 2 * BR[1] - TR[1] + gap), (BL[0], 2 * BL[1] - TL[1] + gap)]
        out.alpha_composite(warp(fl, rq, (W, H)).filter(ImageFilter.GaussianBlur(1.5)))
    out.alpha_composite(card)
    return out, -(x0 + fw / 2), -(y0 + h / 2)


def stage(canvas, rows, cx, top, bottom, scale=1.0):
    """Cover-flow rows of plugin windows. dy is the row centre as a fraction of half the stage height."""
    cy = (top + bottom) / 2
    half = (bottom - top) / 2
    for r_i, row in enumerate(rows):
        keys = row["keys"]
        n = len(keys)
        back = r_i < len(rows) - 1
        items = []
        for i, k in enumerate(keys):
            off = i - (n - 1) / 2
            d = abs(off)
            if row.get("inward"):
                side = 1 if off < 0 else -1      # pair facing each other (outer edges near)
                side = -side
            else:
                side = -1 if off < 0 else 1
            sc = 1 - row["scale_k"] * d
            yaw = min(0.9, row["yaw"] * (d if not row.get("inward") else 1)) if d > 0 else 0
            items.append((d, k, off, sc, yaw, side))
        for d, k, off, sc, yaw, side in sorted(items, key=lambda t: -t[0]):
            dark = (0.62 if back else 1.0) * (1 - 0.07 * d)
            lay, ox, oy = card_layers(k, int(row["w"] * sc * scale), yaw, side, darken=dark,
                                      glow=0.55 if back else 0.9, refl=0 if back else 0.30)
            x = cx + off * row["spread"] * scale
            y = cy + row["dy"] * half
            canvas = place(canvas, lay, x + ox, y + oy)
    return canvas


# ---------------------------------------------------------------- backgrounds
def backdrop(W, H, acc, hue, blooms, streak_col=None, seed=3):
    acc = np.array(acc, np.float32)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    yy /= H; xx /= W
    img = np.ones((H, W, 3), np.float32) * np.array(hue, np.float32) * 0.6
    img *= (1.15 - 0.5 * yy)[..., None]
    for bx, by, rx, ry, a, col in blooms:
        c = np.array(col if col is not None else acc, np.float32)
        img += np.exp(-(((xx - bx) / rx) ** 2 + ((yy - by) / ry) ** 2))[..., None] * c * a
    sc = (acc * 0.5 + 120) if streak_col is None else np.array(streak_col, np.float32)
    asp = H / W
    for off, w, a in ((-0.15, 0.010, 0.10), (0.05, 0.004, 0.14), (0.62, 0.016, 0.06), (0.78, 0.003, 0.12)):
        d = xx * 0.62 + yy * asp - 0.55 * max(1, asp) - off
        img += (np.exp(-(d / w) ** 2) * np.exp(-((yy - 0.42) / 0.45) ** 2))[..., None] * sc * a
    img += np.random.default_rng(seed).normal(0, 3.2, (H, W, 1))
    r = np.sqrt((xx - 0.5) ** 2 + ((yy - 0.48) * asp) ** 2)
    img *= np.clip(1.25 - r * 1.15 / max(1, asp * 0.8), 0.22, 1)[..., None]
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB").convert("RGBA")


def accents_of(cfg):
    ks = [k for row in cfg["rows"] for k in row["keys"]]
    return [PRODUCTS[k]["accent"] for k in ks]


def light_rays(canvas, cx, cy, length, col, n=18, alpha=40, seed=1):
    W, H = canvas.size
    ray = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(ray)
    rng = np.random.default_rng(seed)
    for i in range(n):
        ang = np.pi + (i + rng.random() * 0.6) / n * np.pi  # upper half fan
        w = 0.025 + rng.random() * 0.03
        p1 = (cx + np.cos(ang - w) * length, cy + np.sin(ang - w) * length)
        p2 = (cx + np.cos(ang + w) * length, cy + np.sin(ang + w) * length)
        d.polygon([(cx, cy), p1, p2], fill=int(alpha * (0.4 + rng.random() * 0.6)))
    ray = ray.filter(ImageFilter.GaussianBlur(18))
    yy = np.linspace(0, 1, H)[:, None]
    ra = (np.asarray(ray, np.float32)).astype(np.uint8)
    lay = Image.new("RGBA", (W, H), col + (255,))
    lay.putalpha(Image.fromarray(ra))
    return Image.alpha_composite(canvas, lay)


# ---------------------------------------------------------------- bundle cover
def make_bundle(key):
    cfg = BUNDLES[key]
    S, M = 2048, 96
    acc = cfg["accent"]
    acc_light = mix(acc, (255, 255, 255), 0.35)
    top_y, logo_h = 70, 210
    title = chrome_title(cfg["title"], cfg.get("cap", 270), glow=acc, glow_strength=1.7, tint=cfg.get("tint", (0, 0, 0)))
    lx0, ly0, lx1, ly1 = text_bbox_offset(title)
    maxw = S - 2 * M - 20
    if lx1 - lx0 > maxw:
        title = chrome_title(cfg["title"], int(cfg.get("cap", 270) * maxw / (lx1 - lx0)), glow=acc, glow_strength=1.7,
                             tint=cfg.get("tint", (0, 0, 0)))
        lx0, ly0, lx1, ly1 = text_bbox_offset(title)
    title_top = top_y + logo_h + 40
    title_bot = title_top + (ly1 - ly0)
    sub_y = title_bot + 44
    plat_y = sub_y + 92 + 16
    stage_top = plat_y + 54 + 60
    chips_y = S - 92 - 70 - 96
    stage_bot = chips_y - 70

    blooms = [(0.5, 0.25, 0.60, 0.15, 0.30, None), (0.08, 0.02, 0.30, 0.14, 0.06, None)]
    if cfg.get("multi"):
        accs = accents_of(cfg)
        for i, c in enumerate(accs[:9]):
            bx = 0.1 + 0.8 * i / max(1, min(9, len(accs)) - 1)
            blooms.append((bx, (stage_top + stage_bot) / 2 / S, 0.16, 0.20, 0.16, c))
    img = backdrop(S, S, acc, cfg["bg_hue"], blooms)
    img = light_rays(img, S / 2, stage_bot - 80, 1500, mix(acc, (255, 255, 255), 0.4), alpha=34)
    # floor glow line
    fl = Image.new("L", (S, S), 0)
    ImageDraw.Draw(fl).ellipse((S * 0.1, stage_bot - 40, S * 0.9, stage_bot + 60), fill=120)
    fl = fl.filter(ImageFilter.GaussianBlur(40))
    fll = Image.new("RGBA", (S, S), acc + (255,)); fll.putalpha(fl)
    img = Image.alpha_composite(img, fll)

    img = stage(img, cfg["rows"], S / 2, stage_top, stage_bot)

    # logo + pill
    logo = trapwin_logo(logo_h)
    img = place(img, logo, M, top_y)
    p, ppad = pill(cfg["price"], acc, style=cfg.get("pill"))
    ph = p.height - 2 * ppad
    img = place(img, p, S - M - (p.width - ppad), top_y + (logo_h - ph) // 2 - ppad)

    tx = (S - (lx1 - lx0)) // 2
    img = place(img, title, tx - lx0, title_top - ly0)
    for sx, sy, ss in ((tx - 55, title_top + (title_bot - title_top) * 0.25, 90),
                       (tx + (lx1 - lx0) - 40, title_bot - 40, 130), (tx + (lx1 - lx0) * 0.55, title_top - 50, 60)):
        img = place(img, sparkle(ss), sx - ss / 2, sy - ss / 2)

    sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
    fs = font("BarlowCondensed-Bold.ttf", 92)
    spaced_text(ImageDraw.Draw(sh), (S / 2, sub_y + 6), cfg["subtitle"], fs, (0, 0, 0, 220), 10)
    img = Image.alpha_composite(img, sh.filter(ImageFilter.GaussianBlur(6)))
    d = ImageDraw.Draw(img)
    spaced_text(d, (S / 2, sub_y), cfg["subtitle"], fs, (255, 255, 255, 255), 10)
    fp = font("ShareTechMono-Regular.ttf", 54)
    tw = spaced_text(d, (S / 2, plat_y + 8), "WINDOWS + macOS  ·  VST3 / AU", fp, acc_light + (255,), 3)
    ry = plat_y + 8 + 54 * 0.55
    for side in (-1, 1):
        x0, x1 = S / 2 + side * (tw / 2 + 30), S / 2 + side * (tw / 2 + 230)
        d.line((x0, ry, x1, ry), fill=acc + (200,), width=3)
        d.ellipse((x1 - 6, ry - 6, x1 + 6, ry + 6), fill=acc + (255,))

    chips = [chip(c, acc, 96) for c in cfg["chips"]]
    gap = 34
    inner = [c.width - 2 * pd for c, pd in chips]
    x = (S - (sum(inner) + gap * (len(chips) - 1))) / 2
    for (c, pd), w in zip(chips, inner):
        img = place(img, c, x - pd, chips_y - pd)
        x += w + gap

    d = ImageDraw.Draw(img)
    fy0 = S - 92
    fw = spaced_text(d, (S / 2, fy0 - 10), "TRAPWIN.COM", font("ArchivoBlack-Regular.ttf", 44), (235, 238, 248, 255), 14)
    for side in (-1, 1):
        d.line((S / 2 + side * (fw / 2 + 40), fy0 + 18, S / 2 + side * (fw / 2 + 360), fy0 + 18),
               fill=(255, 255, 255, 70), width=2)
    os.makedirs(OUT_DIR, exist_ok=True)
    out = os.path.join(OUT_DIR, cfg["out"])
    img.convert("RGB").save(out, "JPEG", quality=92, subsampling=0)
    print("wrote", out)


# ---------------------------------------------------------------- 9:16 stills
def split_price(cfg):
    s = cfg["price"]
    if "•" in s:
        v, p = [t.strip() for t in s.split("•")]
        return v.replace(" VALUE", ""), p
    return None, "FREE"


def make_vertical(key):
    is_bundle = key in BUNDLES
    cfg = BUNDLES[key] if is_bundle else PRODUCTS[key]
    W, H = 1080, 1920
    acc = cfg["accent"]
    acc_light = mix(acc, (255, 255, 255), 0.35)
    L, R = 60, 950              # key text stays left of x=950 (TikTok side rail)
    cx = 540
    safe_bot = int(H * 0.80)    # nothing important below
    tmax = 2 * (R - cx)         # 820 px max width for centred key text

    blooms = [(0.5, 0.20, 0.7, 0.12, 0.30, None), (0.5, 0.50, 0.8, 0.20, 0.22, None)]
    if is_bundle and cfg.get("multi"):
        for i, c in enumerate(accents_of(cfg)[:7]):
            blooms.append((0.1 + 0.8 * i / 6, 0.50, 0.18, 0.12, 0.14, c))
    img = backdrop(W, H, acc, cfg["bg_hue"], blooms, seed=11)

    logo = trapwin_logo(130)
    img = place(img, logo, L, 64)

    tint = cfg.get("tint", (0, 0, 0))

    def fit(text, cap):
        t = chrome_title(text, cap, glow=acc, glow_strength=1.6, tint=tint)
        a, b, c, e = text_bbox_offset(t)
        return cap if c - a <= tmax else int(cap * tmax / (c - a))

    words = cfg["title"].split(" ")
    cap1 = fit(cfg["title"], 200)
    if cap1 < 150 and len(words) > 1:
        cut = max(range(1, len(words)), key=lambda k: -abs(len(" ".join(words[:k])) - len(" ".join(words[k:]))))
        lines = [" ".join(words[:cut]), " ".join(words[cut:])]
        cap = min(fit(l, 175) for l in lines)
    else:
        lines, cap = [cfg["title"]], cap1
    t_top = 236
    y = t_top
    xs = []
    for ln in lines:
        t = chrome_title(ln, cap, glow=acc, glow_strength=1.6, tint=tint)
        x0, y0, x1, y1 = text_bbox_offset(t)
        img = place(img, t, cx - (x1 - x0) / 2 - x0, y - y0)
        xs.append((cx - (x1 - x0) / 2, cx + (x1 - x0) / 2, y, y + (y1 - y0)))
        y += (y1 - y0) + int(cap * 0.16)
    t_bot = xs[-1][3]
    img = place(img, sparkle(70), xs[-1][1] - 30, t_bot - 50)
    img = place(img, sparkle(44), xs[0][0] - 10, t_top - 20)

    d = ImageDraw.Draw(img)
    fs = font("BarlowCondensed-Bold.ttf", 54)
    sub = cfg["subtitle"]
    while ImageDraw.Draw(Image.new("L", (1, 1))).textlength(sub, font=fs) + 5 * len(sub) > tmax:
        fs = font("BarlowCondensed-Bold.ttf", fs.size - 2)
    sub_y = t_bot + 34
    spaced_text(d, (cx, sub_y), sub, fs, (255, 255, 255, 255), 5)

    ui_top, ui_bot = sub_y + fs.size + 50, 1185
    if is_bundle:
        img = stage(img, cfg["rows"], cx, ui_top + 10, ui_bot - 30, scale=min(cfg.get("vscale", 0.62), (ui_bot - ui_top) / 1000))
    else:
        im0 = Image.open(PRODUCTS[key]["ui"])
        avail = ui_bot - ui_top - 40
        w = int(min(980, avail * im0.width / im0.height))
        lay, ox, oy = card_layers(key, w, 0, 1, refl=0.30)
        img = place(img, lay, cx + ox, (ui_top + ui_bot) / 2 - 10 + oy)
        img = place(img, sparkle(90), cx + w / 2 - 10, (ui_top + ui_bot) / 2 - avail / 2 + 10)

    # price lines
    d = ImageDraw.Draw(img)
    val, pr = split_price(cfg)
    f1 = font("ArchivoBlack-Regular.ttf", 46)
    f2 = font("ArchivoBlack-Regular.ttf", 96)
    py = 1222
    if val:
        a_txt, v_txt = "PLUGINS LIKE THIS: ", val
    else:
        a_txt, v_txt = "PAID ELSEWHERE", ""
    arrow = "  →"
    wa = d.textlength(a_txt, font=f1); wv = d.textlength(v_txt, font=f1); wr = d.textlength(arrow, font=f1)
    sx = cx - (wa + wv + wr) / 2
    d.text((sx, py), a_txt, font=f1, fill=(225, 228, 238, 255))
    d.text((sx + wa, py), v_txt, font=f1, fill=(225, 228, 238, 255))
    if v_txt:
        sy = py + f1.size * 0.62
        d.line((sx + wa - 4, sy, sx + wa + wv + 4, sy), fill=(255, 60, 70, 255), width=6)
    d.text((sx + wa + wv, py), arrow, font=f1, fill=acc_light + (255,))
    l2 = "TRAPWIN: " + pr
    w2 = d.textlength(l2, font=f2)
    if w2 > tmax:
        f2 = font("ArchivoBlack-Regular.ttf", int(96 * tmax / w2)); w2 = d.textlength(l2, font=f2)
    y2 = py + 66
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(glow).text((cx - w2 / 2, y2), l2, font=f2, fill=acc + (255,))
    img = Image.alpha_composite(img, glow.filter(ImageFilter.GaussianBlur(18)))
    d = ImageDraw.Draw(img)
    d.text((cx - w2 / 2 + 3, y2 + 5), l2, font=f2, fill=(0, 0, 0, 180))
    d.text((cx - w2 / 2, y2), l2, font=f2, fill=(255, 255, 255, 255))

    # CTA (above the bottom 20 %)
    f3 = font("BarlowCondensed-Bold.ttf", 52)
    cta = "LINK IN BIO  ·  TRAPWIN.COM"
    cw = spaced_text(ImageDraw.Draw(Image.new("L", (1, 1))), (0, 0), cta, f3, 255, 4, anchor_center=False)
    ch = 84
    bx0, bx1 = cx - cw / 2 - 44, cx + cw / 2 + 44
    by0 = safe_bot - ch - 30
    btn = Image.new("RGBA", img.size, (0, 0, 0, 0))
    bd = ImageDraw.Draw(btn)
    bd.rounded_rectangle((bx0, by0, bx1, by0 + ch), ch // 2, fill=acc + (90,))
    img = Image.alpha_composite(img, btn.filter(ImageFilter.GaussianBlur(20)))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((bx0, by0, bx1, by0 + ch), ch // 2, fill=(10, 12, 20, 235), outline=acc + (255,), width=4)
    bb = d.textbbox((0, 0), "L", font=f3)
    spaced_text(d, (cx, by0 + (ch - (bb[3] - bb[1])) / 2 - bb[1]), cta, f3, (255, 255, 255, 255), 4)

    os.makedirs(VERT_DIR, exist_ok=True)
    name = VERT_NAMES.get(key) or PRODUCTS[key]["out"].replace("-cover.jpg", "")
    out = os.path.join(VERT_DIR, name + "-9x16.jpg")
    img.convert("RGB").save(out, "JPEG", quality=92, subsampling=0)
    print("wrote", out)


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args or "bundles" in args:
        for k in BUNDLES:
            make_bundle(k)
    if not args or "vertical" in args:
        for k in list(PRODUCTS) + list(BUNDLES):
            make_vertical(k)
    for a in args:
        if a in BUNDLES:
            make_bundle(a); make_vertical(a)
        elif a in PRODUCTS:
            make_vertical(a)
