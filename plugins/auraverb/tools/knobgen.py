"""Photoreal knob renderer (numpy). Renders a static knob image; the editor rotates only the indicator.

Lathe-turned (concentric) brushing is rotation-invariant, so the anisotropic highlight lobes stay fixed
to the light like on real hardware, while the pointer is drawn on top at the current angle.

render_knob(size, style) -> RGBA PIL image (size x size px, knob centred, includes soft shadow)
styles: "alu" (brushed aluminium), "black" (black anodised with metal top), "bakelite" (cream glossy)
"""
import math

import numpy as np
from PIL import Image, ImageFilter


def _norm(v):
    return v / np.maximum(1e-6, np.linalg.norm(v, axis=-1, keepdims=True))


def render_knob(size, style="alu", light_deg=125.0, seed=1):
    rng = np.random.default_rng(seed)
    S = size
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    c = (S - 1) / 2.0
    dx, dy = (xx - c) / (S * 0.5), (yy - c) / (S * 0.5)
    r = np.sqrt(dx * dx + dy * dy)
    th = np.arctan2(dy, dx)
    la = math.radians(light_deg)
    L = _norm(np.array([math.cos(la) * -1.0, math.sin(la) * -1.0, 1.6], np.float32))

    R_body = 0.80          # outer radius of the knob (rest is shadow room)
    R_top = 0.62 if style != "bakelite" else 0.66
    bevel = 0.05

    rgb = np.zeros((S, S, 3), np.float32)
    alpha = np.zeros((S, S), np.float32)

    # ---------- skirt (side wall seen from above as a ring with knurling) ----------
    ring = (r <= R_body) & (r > R_top)
    t = np.clip((r - R_top) / (R_body - R_top), 0, 1)
    knurl = 0.5 + 0.5 * np.sign(np.sin(th * (60 if S > 160 else 40)))
    if style == "bakelite":
        side = np.array([150, 132, 104], np.float32)
        knurl = 0.5 + 0.5 * np.sin(th * 36)
    elif style == "black":
        side = np.array([34, 34, 38], np.float32)
    else:
        side = np.array([120, 122, 128], np.float32)
    # the wall faces outwards: lit where it faces the light
    face = 0.55 + 0.45 * np.cos(th - (la + math.pi))
    shade = (0.35 + 0.65 * face)[..., None] * (0.80 + 0.20 * knurl)[..., None] * (1.0 - 0.35 * t)[..., None]
    rgb = np.where(ring[..., None], side * shade, rgb)
    alpha = np.where(ring, 1.0, alpha)

    # ---------- top face ----------
    top = r <= R_top
    # bevel normal near the top edge
    e = np.clip((r - (R_top - bevel)) / bevel, 0, 1)
    nz = np.where(top, 1.0 - 0.75 * e, 1.0)
    nx = np.where(top, dx / np.maximum(r, 1e-6) * e * 0.9, 0.0)
    ny = np.where(top, dy / np.maximum(r, 1e-6) * e * 0.9, 0.0)
    N = _norm(np.stack([nx, ny, nz], -1))
    diffuse = np.clip((N * L).sum(-1), 0, 1)

    if style == "bakelite":
        base = np.array([232, 222, 198], np.float32)
        col = base * (0.55 + 0.5 * diffuse)[..., None]
        # glossy dome highlight
        hl = np.exp(-(((dx + 0.22) / 0.20) ** 2 + ((dy + 0.26) / 0.14) ** 2))
        col += 255 * 0.55 * hl[..., None]
        col *= (1.0 - 0.18 * (r / R_top) ** 3)[..., None]
    else:
        # lathe brushing: concentric micro-grooves -> anisotropic highlight lobes along the light axis
        rings = rng.normal(0, 1, 4096)
        idx = np.clip((r / R_top * 4095).astype(int), 0, 4095)
        groove = np.convolve(rings, np.ones(3) / 3, mode="same")[idx]
        aniso = np.abs(np.cos(th - la)) ** 6 + 0.55 * np.abs(np.cos(th - la)) ** 2
        if style == "black":
            base = np.array([46, 46, 52], np.float32)
            spec = 70
        else:
            base = np.array([150, 152, 158], np.float32)
            spec = 105
        col = base * (0.55 + 0.55 * diffuse)[..., None]
        col += (spec * aniso * (0.35 + 0.65 * (r / R_top)))[..., None] * (top & (e < 0.98))[..., None]
        col += (groove * (10 if style == "alu" else 6))[..., None]
        # bevel catches a sharp highlight on the lit side
        rim = np.exp(-((r - (R_top - bevel * 0.5)) / (bevel * 0.35)) ** 2) * np.clip(np.cos(th - (la + math.pi)), 0, 1)
        col += (rim * 120)[..., None]
        # centre dimple
        dimple = np.exp(-(r / 0.10) ** 2)
        col *= (1.0 - 0.10 * dimple)[..., None]
    rgb = np.where(top[..., None], col, rgb)
    alpha = np.where(top, 1.0, alpha)

    # edge line between top and skirt
    edge = np.exp(-((r - R_top) / 0.006) ** 2)
    rgb *= (1.0 - 0.45 * edge)[..., None]

    # anti-aliased outer edge
    aa = np.clip((R_body - r) * S * 0.5, 0, 1)
    alpha = np.minimum(alpha, aa) if False else alpha * aa

    knob = np.dstack([np.clip(rgb, 0, 255), alpha * 255]).astype(np.uint8)
    knob_img = Image.fromarray(knob, "RGBA")

    # soft contact shadow, offset away from the light
    sh = Image.new("L", (S, S), 0)
    sh_a = np.clip((R_body * 1.02 - r) * S * 0.5, 0, 1) * 200
    sh = Image.fromarray(sh_a.astype(np.uint8)).filter(ImageFilter.GaussianBlur(S * 0.035))
    off = int(S * 0.035)
    shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    shadow.paste(Image.new("RGBA", (S, S), (0, 0, 0, 255)), (off // 2, off), sh)
    return Image.alpha_composite(shadow, knob_img)


if __name__ == "__main__":
    import sys
    for st in ("alu", "black", "bakelite"):
        render_knob(320, st).save(f"knob_{st}.png")
    print("ok")
