"""TrapWin store cover generator (2048x2048 Shopify product images).

    python3 make_cover.py            # all products
    python3 make_cover.py dust pump  # selected
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from brand import chrome_title, place, sparkle, text_bbox_offset, trapwin_logo  # noqa: E402

FONTS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fonts")
OUT_DIR = "/home/claude/effector-killa/store"
S = 2048
M = 96  # outer margin

PILL_GREEN = dict(glow=(40, 230, 140), top=(70, 245, 160), bot=(0, 160, 90), edge=(190, 255, 220), shadow=(0, 70, 40))

PRODUCTS = {
    "dust": dict(title="DUST TAPE", subtitle="LO-FI TAPE MACHINE", accent=(255, 140, 50),
                 price="$99 VALUE  •  $29", chips=["20 PRESETS", "TAPE · VINYL · WOBBLE", "ZERO-HASSLE LO-FI"],
                 ui="/home/claude/trapwin-dust/shots/dust_ui.png", out="dust-tape-cover.jpg",
                 bg_hue=(28, 18, 14)),
    "pump": dict(title="PUMP KING", subtitle="VOLUME SHAPER & SIDECHAIN", accent=(182, 255, 59),
                 price="$199 VALUE  •  $29", chips=["DRAW ANY CURVE", "MIDI · AUDIO · SYNC TRIGGER", "MULTIBAND"],
                 ui="/home/claude/trapwin-pump/shots/pump_ui.png", out="pump-king-cover.jpg",
                 bg_hue=(12, 22, 12), bloom=0.6),
    "vox": dict(title="VOX DEMON", subtitle="PITCH · FORMANT · HARD TUNE", accent=(255, 63, 164),
                price="$129 VALUE  •  $39", chips=["HARD-TUNE IN KEY", "FORMANT SHIFT", "MIDI CONTROL"],
                ui="/home/claude/trapwin-vox/shots/vox_ui.png", out="vox-demon-cover.jpg",
                bg_hue=(24, 10, 26)),
    "drum": dict(title="DRUM KILLA", subtitle="DRUM DAMAGE UNIT", accent=(255, 40, 40),
                 price="$149 VALUE  •  $39", chips=["808 · KICK · SNARE · CLAP", "42 PRESETS", "CLIP · DRIVE · DUCK"],
                 ui="/home/claude/dk-shots/dk_ui.png",
                 out="drum-killa-cover.jpg", bg_hue=(26, 8, 10)),
    "halo": dict(title="HALO", subtitle="LUSH REVERB · FREEZE · DUCK", accent=(95, 227, 255),
                 price="FREE DOWNLOAD", chips=["26 PRESETS", "FREEZE · DUCK", "100% FREE"],
                 ui="/home/claude/trapwin-halo/shots/halo_ui.png", out="halo-cover.jpg",
                 bg_hue=(8, 20, 30), bloom=0.75,
                 pill=dict(glow=(40, 230, 140), top=(70, 245, 160), bot=(0, 160, 90), edge=(190, 255, 220),
                           shadow=(0, 70, 40))),
    "throw": dict(title="THROW", subtitle="VOCAL THROW · TAPE DELAY", accent=(255, 178, 46),
                  price="$199 VALUE  •  $29", chips=["ONE-CLICK THROWS", "TAPE · PING-PONG", "29 PRESETS"],
                  ui="/home/claude/trapwin-throw/shots/throw_ui.png", out="throw-cover.jpg",
                  bg_hue=(26, 18, 10), bloom=0.85),
    "chopper": dict(title="CHOPPER", subtitle="STUTTER · TAPE STOP · REVERSE", accent=(155, 92, 255),
                    price="$149 VALUE  •  $29", chips=["16-STEP CHOPS", "7 EFFECTS", "33 PRESETS"],
                    ui="/home/claude/trapwin-chopper/shots/chopper_ui.png", out="chopper-cover.jpg",
                    bg_hue=(16, 10, 30)),
    "subkilla": dict(title="SUB KILLA", subtitle="808 & SUB BASS SYNTH", accent=(255, 45, 45),
                     price="$179 VALUE  •  $39", chips=["808 GLIDE", "4 SATURATION TYPES", "30 PRESETS"],
                     ui="/home/claude/trapwin-subkilla/shots/subkilla_ui.png", out="sub-killa-cover.jpg",
                     bg_hue=(16, 16, 18), bloom=0.6),
    "auraverb": dict(title="AURA VERB", subtitle="PRO STUDIO REVERB · 7 MODES", accent=(232, 196, 106),
                price="$249 VALUE  •  $39", chips=['7 REVERB MODES', 'SHIMMER · 80s · DARK', '44 PRESETS'],
                ui="/home/claude/trapwin-auraverb/shots/auraverb_darkspace.png", out="aura-verb-cover.jpg", bg_hue=(10, 14, 30), bloom=0.8),
    "ghostdelay": dict(title="GHOST DELAY", subtitle="TAPE · ANALOG · PITCH · REVERSE", accent=(125, 255, 216),
                price="$199 VALUE  •  $29", chips=['7 DELAY MODES', 'GHOST PITCH ECHOES', '47 PRESETS'],
                ui="/home/claude/trapwin-ghostdelay/shots/ghostdelay_ghost.png", out="ghost-delay-cover.jpg", bg_hue=(8, 22, 22), bloom=0.6),
    "autokilla": dict(title="AUTO KILLA", subtitle="REAL-TIME VOCAL TUNE", accent=(47, 123, 255),
                price="$399 VALUE  •  $49", chips=['17 ms LIVE MODE', 'AUTO KEY DETECT', '38 PRESETS'],
                ui="/home/claude/trapwin-autokilla/shots/autokilla_live_autokey.png", out="auto-killa-cover.jpg", bg_hue=(8, 12, 30), bloom=1.0),
    "heat": dict(title="HEAT", subtitle="MULTIBAND SATURATION", accent=(255, 90, 54),
                price="$199 VALUE  •  $29", chips=['8 SATURATION MODELS', '3 BANDS · 8x OVERSAMPLING', 'LOW KEEP FOR 808s'],
                ui="/home/claude/trapwin-heat/shots/heat_3band.png", out="heat-cover.jpg", bg_hue=(28, 12, 10), bloom=0.9),
    "glow": dict(title="GLOW", subtitle="ONE-KNOB ENHANCER", accent=(255, 228, 92),
                price="$49 VALUE  •  $9", chips=['ONE KNOB', 'WARM · BRIGHT · FAT · LOUD', 'AUTO GAIN'],
                ui="/home/claude/trapwin-glow/shots/glow_ui.png", out="glow-cover.jpg", bg_hue=(22, 18, 8), bloom=0.6),
    "wide": dict(title="WIDE", subtitle="STEREO IMAGER", accent=(185, 168, 255),
                price="FREE DOWNLOAD", chips=['3-BAND WIDTH', 'MONO-SAFE BASS', '100% FREE'],
                ui="/home/claude/trapwin-wide/shots/wide_ui.png", out="wide-cover.jpg", bg_hue=(14, 12, 30), bloom=0.9, pill=PILL_GREEN),
    "snap": dict(title="SNAP", subtitle="TRANSIENT SHAPER", accent=(255, 61, 127),
                price="$129 VALUE  •  $19", chips=['ATTACK · SUSTAIN', 'SPLIT BAND', '30 PRESETS'],
                ui="/home/claude/trapwin-snap/shots/snap_ui.png", out="snap-cover.jpg", bg_hue=(26, 10, 18), bloom=0.9),
    "sweep": dict(title="SWEEP", subtitle="FILTER MOTION · LFO · ENVELOPE", accent=(32, 217, 138),
                price="$149 VALUE  •  $19", chips=['2 FILTERS · 8 TYPES', 'DRAW YOUR LFO', '39 PRESETS'],
                ui="/home/claude/trapwin-sweep/shots/sweep_ui.png", out="sweep-cover.jpg", bg_hue=(8, 22, 16), bloom=0.7),
}


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def mix(a, b, t):
    return tuple(int(round(a[i] * (1 - t) + b[i] * t)) for i in range(3))


# ---------------------------------------------------------------- background
def background(cfg, ui_box):
    acc = np.array(cfg["accent"], np.float32) * cfg.get("bloom", 1.0)
    base = np.array(cfg["bg_hue"], np.float32)
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32) / S
    img = np.ones((S, S, 3), np.float32) * base * 0.55
    # vertical gradient, darker at bottom
    img *= (1.15 - 0.55 * yy)[..., None]
    # big accent bloom behind title + behind UI
    def blob(cx, cy, rx, ry, a):
        d = ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2
        return np.exp(-d)[..., None] * acc * a
    img += blob(0.5, 0.25, 0.60, 0.15, 0.30)
    ux0, uy0, ux1, uy1 = [v / S for v in ui_box]
    img += blob((ux0 + ux1) / 2, (uy0 + uy1) / 2, 0.60, 0.34, 0.24)
    img += blob(0.08, 0.02, 0.30, 0.14, 0.06)
    img += blob(0.95, 0.04, 0.30, 0.14, 0.08)
    # diagonal light streaks
    for off, w, a in ((-0.15, 0.010, 0.10), (0.05, 0.004, 0.13), (0.62, 0.016, 0.06), (0.78, 0.003, 0.12)):
        d = (xx * 0.62 + yy - 0.55 - off)
        img += (np.exp(-(d / w) ** 2) * np.exp(-((yy - 0.42) / 0.40) ** 2))[..., None] * (acc * 0.5 + 120) * a
    # perspective floor grid under UI (subtle)
    floor_top = uy1
    fy = yy - floor_top
    m = fy > 0
    depth = np.where(m, 0.02 / np.maximum(fy, 1e-3), 0)
    gx = np.where(m, np.abs(((xx - 0.5) * depth * 6) % 1 - 0.5), 0.5)
    gz = np.where(m, np.abs((depth * 1.5) % 1 - 0.5), 0.5)
    line = (np.exp(-(gx / 0.03) ** 2) + np.exp(-(gz / 0.05) ** 2)) * m * np.clip(fy * 6, 0, 1) * np.clip(1 - fy * 4, 0, 1)
    img += line[..., None] * acc * 0.10
    # film grain
    rng = np.random.default_rng(7)
    img += rng.normal(0, 3.2, (S, S, 1))
    # vignette
    r = np.sqrt((xx - 0.5) ** 2 + (yy - 0.48) ** 2)
    img *= np.clip(1.25 - r * 1.15, 0.25, 1)[..., None]
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB").convert("RGBA")


# ---------------------------------------------------------------- elements
PILL_RED = dict(glow=(255, 46, 64), top=(255, 92, 104), bot=(214, 18, 40), edge=(255, 190, 196), shadow=(110, 0, 16))


def pill(text, accent, h=118, style=None):
    st = style or PILL_RED
    f = font("ArchivoBlack-Regular.ttf", int(h * 0.42))
    tw = ImageDraw.Draw(Image.new("L", (1, 1))).textlength(text, font=f)
    w = int(tw + h * 1.1)
    pad = 60
    W, H = w + pad * 2, h + pad * 2
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    red = st["glow"]
    glow = Image.new("L", (W, H), 0)
    ImageDraw.Draw(glow).rounded_rectangle((pad, pad, pad + w, pad + h), h // 2, fill=200)
    glow = glow.filter(ImageFilter.GaussianBlur(26))
    gl = Image.new("RGBA", (W, H), red + (255,))
    gl.putalpha(glow)
    out.alpha_composite(gl)
    # body gradient
    body = np.zeros((h, w, 4), np.uint8)
    t = np.linspace(0, 1, h)[:, None]
    for i, (a, b) in enumerate(zip(st["top"], st["bot"])):
        body[..., i] = (a * (1 - t) + b * t).astype(np.uint8)
    mk = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mk).rounded_rectangle((0, 0, w - 1, h - 1), h // 2, fill=255)
    body[..., 3] = np.asarray(mk)
    bi = Image.fromarray(body, "RGBA")
    out.alpha_composite(bi, (pad, pad))
    d = ImageDraw.Draw(out)
    d.rounded_rectangle((pad, pad, pad + w - 1, pad + h - 1), h // 2, outline=st["edge"] + (255,), width=4)
    # gloss
    gloss = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(gloss).rounded_rectangle((pad + h * 0.25, pad + 8, pad + w - h * 0.25, pad + h * 0.42),
                                            h // 4, fill=(255, 255, 255, 46))
    out.alpha_composite(gloss)
    bb = d.textbbox((0, 0), text, font=f)
    tx = pad + (w - (bb[2] - bb[0])) / 2 - bb[0]
    ty = pad + (h - (bb[3] - bb[1])) / 2 - bb[1]
    d.text((tx + 2, ty + 4), text, font=f, fill=st["shadow"] + (200,))
    d.text((tx, ty), text, font=f, fill=(255, 255, 255, 255))
    return out, pad


def chip(text, accent, h=96):
    f = font("BarlowCondensed-Bold.ttf", int(h * 0.50))
    tw = ImageDraw.Draw(Image.new("L", (1, 1))).textlength(text, font=f)
    dot = int(h * 0.16)
    w = int(tw + h * 0.9 + dot * 2.4)
    pad = 30
    W, H = w + pad * 2, h + pad * 2
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    g = Image.new("L", (W, H), 0)
    ImageDraw.Draw(g).rounded_rectangle((pad, pad, pad + w, pad + h), h // 2, fill=110)
    g = g.filter(ImageFilter.GaussianBlur(16))
    gl = Image.new("RGBA", (W, H), accent + (255,))
    gl.putalpha(g)
    out.alpha_composite(gl)
    d = ImageDraw.Draw(out)
    d.rounded_rectangle((pad, pad, pad + w, pad + h), h // 2, fill=(10, 12, 20, 235),
                        outline=accent + (255,), width=4)
    cx = pad + h * 0.45 + dot
    cy = pad + h / 2
    d.ellipse((cx - dot, cy - dot, cx + dot, cy + dot), fill=accent + (255,))
    bb = d.textbbox((0, 0), text, font=f)
    d.text((cx + dot * 1.6, cy - (bb[1] + bb[3]) / 2), text, font=f, fill=(255, 255, 255, 255))
    return out, pad


def spaced_text(draw, xy, text, f, fill, tracking, anchor_center=True):
    widths = [draw.textlength(c, font=f) for c in text]
    total = sum(widths) + tracking * (len(text) - 1)
    x, y = xy
    if anchor_center:
        x -= total / 2
    for c, w in zip(text, widths):
        draw.text((x, y), c, font=f, fill=fill)
        x += w + tracking
    return total


def rounded_mask(size, r):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), r, fill=255)
    return m


def framed_ui(path, accent, max_w, max_h):
    ui = Image.open(path).convert("RGBA")
    sc = min(max_w / ui.width, max_h / ui.height)
    w, h = int(ui.width * sc), int(ui.height * sc)
    ui = ui.resize((w, h), Image.LANCZOS)
    r = 22
    mk = rounded_mask((w, h), r)
    a = np.minimum(np.asarray(ui.getchannel("A")), np.asarray(mk))
    ui.putalpha(Image.fromarray(a))
    # flatten onto dark so transparent corners are solid
    flat = Image.new("RGBA", (w, h), (8, 9, 14, 255))
    flat.putalpha(mk)
    flat.alpha_composite(ui)
    ui = flat
    # top sheen across the glass
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    sh = np.exp(-(((xx * 0.5 + yy) - h * 0.15) / (h * 0.10)) ** 2) * 18
    arr = np.asarray(ui).astype(np.float32)
    arr[..., :3] = np.clip(arr[..., :3] + sh[..., None], 0, 255)
    ui = Image.fromarray(arr.astype(np.uint8), "RGBA")
    d = ImageDraw.Draw(ui)
    d.rounded_rectangle((0, 0, w - 1, h - 1), r, outline=mix(accent, (255, 255, 255), 0.55) + (255,), width=4)
    d.rounded_rectangle((5, 5, w - 6, h - 6), r - 4, outline=(0, 0, 0, 160), width=2)
    return ui


# ---------------------------------------------------------------- compose
def make(key):
    cfg = PRODUCTS[key]
    acc = cfg["accent"]
    acc_light = mix(acc, (255, 255, 255), 0.35)

    # ---- layout plan (y coordinates)
    top_y = 70
    logo_h = 210
    title_cap = 270
    chip_h = 96
    footer_y = S - 92
    chips_y = footer_y - 70 - chip_h  # chip top

    # chrome title, fit width
    title = chrome_title(cfg["title"], title_cap, glow=acc, glow_strength=1.6)
    lx0, ly0, lx1, ly1 = text_bbox_offset(title)
    maxw = S - 2 * M - 20
    if lx1 - lx0 > maxw:
        f = maxw / (lx1 - lx0)
        title = chrome_title(cfg["title"], int(title_cap * f), glow=acc, glow_strength=1.6)
        lx0, ly0, lx1, ly1 = text_bbox_offset(title)
    title_top = top_y + logo_h + 45
    title_bot = title_top + (ly1 - ly0)
    sub_y = title_bot + 48
    sub_size = 92
    plat_y = sub_y + sub_size + 18
    plat_size = 54
    ui_top = plat_y + plat_size + 70
    ui_max_h = chips_y - 95 - ui_top
    ui_img = framed_ui(cfg["ui"], acc, S - 2 * M - 60, ui_max_h)
    uw, uh = ui_img.size
    ux, uy = (S - uw) // 2, ui_top + (ui_max_h - uh) // 2

    img = background(cfg, (ux, uy, ux + uw, uy + uh))

    # ---- logo (full, untouched) top-left with soft backing glow
    logo = trapwin_logo(logo_h)
    lg = Image.new("L", (logo.width + 200, logo.height + 200), 0)
    lg.paste(logo.getchannel("A"), (100, 100))
    lg = lg.filter(ImageFilter.GaussianBlur(40))
    lgl = Image.new("RGBA", lg.size, acc + (255,))
    lgl.putalpha(lg.point(lambda v: int(v * 0.35)))
    img = place(img, lgl, M - 100, top_y - 100)
    img = place(img, logo, M, top_y)

    # ---- price pill top-right, vertically centred on the logo
    p, ppad = pill(cfg["price"], acc, style=cfg.get("pill"))
    ph = p.height - 2 * ppad
    img = place(img, p, S - M - (p.width - ppad), top_y + (logo_h - ph) // 2 - ppad)

    # ---- title
    tx = (S - (lx1 - lx0)) // 2
    img = place(img, title, tx - lx0, title_top - ly0)

    # sparkles around title
    for sx, sy, ss in ((tx - 55, title_top + (title_bot - title_top) * 0.25, 90), (tx + (lx1 - lx0) - 40, title_bot - 40, 120),
                       (tx + (lx1 - lx0) * 0.55, title_top - 55, 54)):
        img = place(img, sparkle(ss), sx - ss / 2, sy - ss / 2)

    d = ImageDraw.Draw(img)
    # ---- subtitle + platform line
    fs = font("BarlowCondensed-Bold.ttf", sub_size)
    # subtle shadow
    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    spaced_text(ImageDraw.Draw(shadow), (S / 2, sub_y + 6), cfg["subtitle"], fs, (0, 0, 0, 200), 10)
    img = Image.alpha_composite(img, shadow.filter(ImageFilter.GaussianBlur(6)))
    d = ImageDraw.Draw(img)
    spaced_text(d, (S / 2, sub_y), cfg["subtitle"], fs, (255, 255, 255, 255), 10)
    fp = font("ShareTechMono-Regular.ttf", plat_size)
    tw = spaced_text(d, (S / 2, plat_y + 8), "WINDOWS + macOS  ·  VST3 / AU", fp, acc_light + (255,), 3)
    # accent rules either side
    ry = plat_y + 8 + plat_size * 0.55
    for side in (-1, 1):
        x0 = S / 2 + side * (tw / 2 + 30)
        x1 = S / 2 + side * (tw / 2 + 230)
        d.line((x0, ry, x1, ry), fill=acc + (200,), width=3)
        d.ellipse((x1 - 6, ry - 6, x1 + 6, ry + 6), fill=acc + (255,))

    # drop shadow
    sh = Image.new("L", (uw + 300, uh + 300), 0)
    ImageDraw.Draw(sh).rounded_rectangle((150, 190, 150 + uw, 190 + uh), 30, fill=150)
    sh = sh.filter(ImageFilter.GaussianBlur(40))
    shl = Image.new("RGBA", sh.size, (0, 0, 0, 255))
    shl.putalpha(sh)
    img = place(img, shl, ux - 150, uy - 150)
    # ---- UI: glow, shadow, reflection
    gw = Image.new("L", (uw + 400, uh + 400), 0)
    ImageDraw.Draw(gw).rounded_rectangle((200, 200, 200 + uw, 200 + uh), 30, fill=255)
    for rad, a in ((110, 1.0), (30, 1.0), (8, 0.8)):
        g = gw.filter(ImageFilter.GaussianBlur(rad)).point(lambda v, a=a: int(v * a))
        gl = Image.new("RGBA", gw.size, acc + (255,))
        gl.putalpha(g)
        img = place(img, gl, ux - 200, uy - 200)
    # reflection
    ref_h = min(int(uh * 0.30), chips_y - (uy + uh) - 20)
    if ref_h > 10:
        ref = ui_img.transpose(Image.FLIP_TOP_BOTTOM).crop((0, 0, uw, ref_h))
        ra = np.asarray(ref).copy()
        fade = (np.linspace(0.42, 0, ref_h) ** 1.2)[:, None]
        ra[..., 3] = (ra[..., 3] * fade).astype(np.uint8)
        ref = Image.fromarray(ra, "RGBA").filter(ImageFilter.GaussianBlur(2))
        img = place(img, ref, ux, uy + uh + 14)
    img = place(img, ui_img, ux, uy)
    # corner sparkles on the frame
    for sx, sy, ss in ((ux + uw, uy, 130), (ux, uy + uh, 80)):
        img = place(img, sparkle(ss), sx - ss / 2, sy - ss / 2)

    # ---- chips row, centred
    chips = [chip(c, acc, chip_h) for c in cfg["chips"]]
    gap = 34
    inner = [c.width - 2 * pd for c, pd in chips]
    total = sum(inner) + gap * (len(chips) - 1)
    if total > S - 2 * M:  # shrink if needed
        chips = [chip(c, acc, int(chip_h * (S - 2 * M) / total)) for c in cfg["chips"]]
        inner = [c.width - 2 * pd for c, pd in chips]
        total = sum(inner) + gap * (len(chips) - 1)
    x = (S - total) / 2
    for (c, pd), w in zip(chips, inner):
        img = place(img, c, x - pd, chips_y - pd)
        x += w + gap

    # ---- footer
    d = ImageDraw.Draw(img)
    ff = font("ArchivoBlack-Regular.ttf", 44)
    fw = spaced_text(d, (S / 2, footer_y - 10), "TRAPWIN.COM", ff, (235, 238, 248, 255), 14)
    fy = footer_y + 18
    for side in (-1, 1):
        x0 = S / 2 + side * (fw / 2 + 40)
        x1 = S / 2 + side * (fw / 2 + 360)
        d.line((x0, fy, x1, fy), fill=(255, 255, 255, 70), width=2)
    img = place(img, sparkle(40), S / 2 + side * (fw / 2 + 380) - 20, fy - 20)
    img = place(img, sparkle(40), S / 2 - (fw / 2 + 380) - 20, fy - 20)

    os.makedirs(OUT_DIR, exist_ok=True)
    out = os.path.join(OUT_DIR, cfg["out"])
    img.convert("RGB").save(out, "JPEG", quality=92, subsampling=0)
    print("wrote", out, "ui", (ux, uy, uw, uh), "title w", lx1 - lx0)
    return out


if __name__ == "__main__":
    for k in (sys.argv[1:] or list(PRODUCTS)):
        make(k)
