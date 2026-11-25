"""
generate_blood_decals.py - RGBA blood decal textures for the facility's story beats (numpy, scipy, Pillow):

    python Tools/generate_blood_decals.py

Writes Assets/Textures/Blood/ (PNG with alpha = coverage; colour is sRGB):
  T_AH_Blood_Pool     1024 x 1024  pool with a darker thick middle, a dried "coffee ring" edge, satellite drops
  T_AH_Blood_Smear    2048 x 512   crawl / drag smear running along the image's width (U): finger and cloth
                                   striations, broken where the body lifted, ends fade so pieces overlap
  T_AH_Blood_Spatter  1024 x 1024  impact spatter: droplets flung outwards (elongated by speed), drips running
                                   down towards the bottom of the image
  T_AH_Blood_Boot(B)  1024 x 512   boot print, toe to the right, patchy tread transfer (B = the other foot)
  T_AH_Blood_Drops    512 x 512    a few round drops with crowns, from someone bleeding as they walk
Tools/place_story_ue58.py turns them into decal materials. Deterministic (fixed seeds).
"""
import os

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Assets", "Textures", "Blood")

THICK = np.array([0.075, 0.003, 0.004])     # linear albedo: pooled, nearly black-red
THIN = np.array([0.30, 0.025, 0.02])        # thin film / spray: brighter red
DRIED = np.array([0.11, 0.025, 0.012])      # dried edge: brown


def noise(rng, h, w, sigma):
    n = ndimage.gaussian_filter(rng.standard_normal((h, w)), sigma, mode="wrap")
    return (n - n.min()) / (n.max() - n.min() + 1e-9)


def srgb(c):
    return np.where(c <= 0.0031308, 12.92 * c, 1.055 * np.power(np.maximum(c, 0), 1 / 2.4) - 0.055)


def save(name, alpha, thickness, dried=None):
    """alpha: coverage 0..1; thickness 0..1 (thin -> thick colour); dried: 0..1 brown edge amount."""
    t = np.clip(thickness, 0, 1)[..., None]
    col = THIN * (1 - t) + THICK * t
    if dried is not None:
        d = np.clip(dried, 0, 1)[..., None]
        col = col * (1 - d) + DRIED * d
    rgba = np.concatenate([srgb(col), np.clip(alpha, 0, 1)[..., None]], -1)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".png")
    Image.fromarray((rgba * 255 + 0.5).astype(np.uint8), "RGBA").save(path, optimize=True)
    print("  decal", os.path.relpath(path, ROOT))


def ellipse(h, w, cx, cy, rx, ry, angle=0.0):
    yy, xx = np.mgrid[0:h, 0:w].astype(float)
    u = (xx - cx) * np.cos(angle) + (yy - cy) * np.sin(angle)
    v = -(xx - cx) * np.sin(angle) + (yy - cy) * np.cos(angle)
    return 1 - (u / rx) ** 2 - (v / ry) ** 2           # > 0 inside


def edge(field, sharp):
    return np.clip(field * sharp, 0, 1)


def pool():
    rng = np.random.default_rng(66)
    n = 1024
    f = np.full((n, n), -10.0)
    for _ in range(7):
        f = np.maximum(f, ellipse(n, n, n / 2 + rng.normal(0, 70), n / 2 + rng.normal(0, 70),
                                  rng.uniform(110, 260), rng.uniform(90, 200), rng.uniform(0, np.pi)))
    f += 0.35 * (noise(rng, n, n, 18) - 0.5) + 0.12 * (noise(rng, n, n, 4) - 0.5)
    alpha = edge(f, 25)
    thick = np.clip(f * 2.2, 0, 1)
    ring = np.clip(1 - np.abs(f - 0.03) * 40, 0, 1) * alpha            # dried rim
    for _ in range(40):                                                 # satellite drops
        a = rng.uniform(0, 2 * np.pi)
        r = rng.uniform(300, 480)
        d = edge(ellipse(n, n, n / 2 + np.cos(a) * r, n / 2 + np.sin(a) * r, *(rng.uniform(4, 14, 2)), a), 6)
        alpha = np.maximum(alpha, d)
    save("T_AH_Blood_Pool", alpha * 0.97, thick, ring * 0.8)


def smear():
    rng = np.random.default_rng(67)
    h, w = 512, 2048
    yy = np.arange(h)[:, None].astype(float)
    xx = np.arange(w)[None, :].astype(float)
    band = np.exp(-((yy - h / 2) / (h * 0.28)) ** 4)                    # the body's width
    lanes = np.zeros((h, w))
    for _ in range(26):                                                 # striations along the drag
        c = rng.uniform(0.15, 0.85) * h + 8 * np.sin(xx / rng.uniform(80, 300) + rng.uniform(0, 6))
        width = rng.uniform(3, 22)
        strength = rng.uniform(0.3, 1.0)
        breaks = np.clip((noise(rng, 1, w, rng.uniform(10, 40))[0] - rng.uniform(0.2, 0.45)) * 5, 0, 1)
        lanes = np.maximum(lanes, strength * np.exp(-((yy - c) / width) ** 2) * breaks[None, :])
    lift = np.clip((noise(rng, 1, w, 60)[0] - 0.25) * 3, 0, 1)[None, :]  # where the body came off the floor
    alpha = np.clip(lanes * band * (0.55 + 0.45 * lift) * 1.3, 0, 1)
    alpha *= 0.75 + 0.25 * noise(rng, h, w, 3)
    ends = np.clip(np.minimum(xx, w - xx) / (w * 0.15), 0, 1)
    alpha *= ends
    thick = np.clip(lanes * 0.8 * lift, 0, 1)
    save("T_AH_Blood_Smear", alpha, thick, (1 - lift[0][None, :].repeat(h, 0)) * 0.6)


def spatter():
    rng = np.random.default_rng(68)
    n = 1024
    alpha = edge(ellipse(n, n, n / 2, n * 0.42, 70, 60) + 0.3 * (noise(rng, n, n, 10) - 0.5), 12)
    thick = alpha * 0.6
    for _ in range(260):                                                # droplets flung outwards
        a = rng.uniform(0, 2 * np.pi)
        r = rng.gamma(2.0, 90) + 60
        size = max(2.0, 26 * np.exp(-r / 220) * rng.uniform(0.5, 1.2))
        cx, cy = n / 2 + np.cos(a) * r, n * 0.42 + np.sin(a) * r
        if not (0 < cx < n and 0 < cy < n):
            continue
        d = edge(ellipse(n, n, cx, cy, size * (1 + r / 250), size, a), 5)   # stretched along its flight
        alpha = np.maximum(alpha, d)
        thick = np.maximum(thick, d * 0.4)
    yy = np.arange(n)[:, None].astype(float)
    xx = np.arange(n)[None, :].astype(float)
    for _ in range(7):                                                  # drips running down
        x0 = n / 2 + rng.normal(0, 55)
        y0 = n * 0.42 + rng.uniform(-10, 40)
        length = rng.uniform(120, 420)
        width = rng.uniform(4, 9)
        run = np.exp(-((xx - x0) / width) ** 2) * (yy > y0) * (yy < y0 + length)
        bead = edge(ellipse(n, n, x0, y0 + length, width * 1.6, width * 2.0), 4)
        alpha = np.maximum(alpha, np.clip(run * 1.5, 0, 1))
        alpha = np.maximum(alpha, bead)
        thick = np.maximum(thick, bead * 0.9)
    save("T_AH_Blood_Spatter", alpha, thick)


def boot():
    rng = np.random.default_rng(69)
    h, w = 512, 1024
    toe = ellipse(h, w, w * 0.68, h * 0.5, w * 0.24, h * 0.36)
    heel = ellipse(h, w, w * 0.22, h * 0.52, w * 0.13, h * 0.3)
    waist = ellipse(h, w, w * 0.45, h * 0.55, w * 0.2, h * 0.22)           # arch on one side
    sole = edge(np.maximum(np.maximum(toe, heel), waist), 30)
    yy, xx = np.mgrid[0:h, 0:w].astype(float)
    # tread: ~1 cm rubber lugs (the print is ~30 cm long = 34 px/cm), chevrons under the forefoot,
    # a block grid under the heel, a smooth arch
    u = xx + 0.7 * np.abs(yy - h / 2)
    fore = ((u % 38) < 26) & ((yy % 30) < 22)
    rear = ((xx % 32) < 22) & (((yy + 8 * ((xx // 32) % 2)) % 28) < 19)
    lugs = np.where(xx > w * 0.45, fore, np.where(xx < w * 0.34, rear, 0)).astype(float)
    lugs = ndimage.gaussian_filter(lugs, 1.6)
    patchy = np.clip((noise(rng, h, w, 14) - 0.3) * 2.5, 0, 1)            # blood didn't transfer evenly
    alpha = sole * lugs * patchy
    alpha = ndimage.gaussian_filter(alpha, 0.8)
    save("T_AH_Blood_Boot", alpha * 0.9, alpha * 0.3, np.full((h, w), 0.35))
    save("T_AH_Blood_BootB", alpha[::-1] * 0.9, alpha[::-1] * 0.3, np.full((h, w), 0.35))   # the other foot (mirrored)


def drops():
    rng = np.random.default_rng(70)
    n = 512
    alpha = np.zeros((n, n))
    thick = np.zeros((n, n))
    for _ in range(7):
        cx, cy, r = rng.uniform(60, n - 60), rng.uniform(60, n - 60), rng.uniform(8, 26)
        d = edge(ellipse(n, n, cx, cy, r, r) + 0.25 * (noise(rng, n, n, 3) - 0.5), 6)
        for k in range(int(rng.integers(5, 12))):                       # crown spikes
            a = rng.uniform(0, 2 * np.pi)
            d = np.maximum(d, edge(ellipse(n, n, cx + np.cos(a) * r * 1.3, cy + np.sin(a) * r * 1.3,
                                           r * 0.45, r * 0.12, a), 5))
        alpha = np.maximum(alpha, d)
        thick = np.maximum(thick, d * 0.7)
    save("T_AH_Blood_Drops", alpha, thick)


if __name__ == "__main__":
    print("Generating blood decals...")
    pool()
    smear()
    spatter()
    boot()
    drops()
    print("Done.")
