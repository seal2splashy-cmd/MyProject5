"""
generate_rugged_textures.py - tileable PBR textures for the facility's walls, floors and ceilings,
in the style of the damaged concrete wall panel from Meshy. Plain Python (numpy, scipy, Pillow):

    python Tools/generate_rugged_textures.py

Writes Assets/Textures/Rugged/ (2048 x 2048, every map tiles seamlessly):

  T_AH_RuggedConcrete_D.jpg / _N.png / _MR.jpg  - cast-in-place wall, 4 m x 4 m per tile:
      plywood formwork panels (seams, lips, faint grain, each pour a slightly different tone),
      form-tie holes with rust runs, air pits ("bug holes"), cracks, spalled chunks with exposed
      aggregate and a little rebar, water streaks and damp stains
  T_AH_WornFloor_D.jpg / _N.png / _MR.jpg       - power-floated slab, 3 m x 3 m per tile:
      saw-cut joints, trowel swirl, scuffs and tyre marks, oil spots (glossy), hairline cracks

_N is a DirectX (Unreal) normal map, _MR has roughness in G and metallic in B (like the weapons and
the A-07 kit). Tools/retexture_a07_ue58.py imports them and builds world-aligned materials.
Deterministic (fixed seeds): re-running gives identical files.
"""
import os

import numpy as np
from PIL import Image
from scipy import ndimage

N = 2048
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Assets", "Textures", "Rugged")


# ----------------------------------------------------------------------------- tileable building blocks
def fbm(rng, beta, lo=1.0, hi=None):
    """Periodic 1/f^beta noise (tiles perfectly), 0..1. lo/hi limit the frequency band (cycles per tile)."""
    f = np.sqrt(np.fft.fftfreq(N)[:, None] ** 2 + np.fft.fftfreq(N)[None, :] ** 2) * N
    amp = np.where(f > 0, 1.0 / np.maximum(f, 1e-6) ** (beta / 2.0), 0.0)
    amp[f < lo] = 0.0
    if hi is not None:
        amp *= np.exp(-(f / hi) ** 2)
    spec = np.fft.fft2(rng.standard_normal((N, N))) * amp
    x = np.real(np.fft.ifft2(spec))
    return (x - x.min()) / (x.max() - x.min() + 1e-9)


def stretched(rng, sx, sy, beta=1.6):
    """Anisotropic periodic noise: long along x when sx < sy (wood grain, streaks)."""
    fy = np.fft.fftfreq(N)[:, None] * N
    fx = np.fft.fftfreq(N)[None, :] * N
    f = np.sqrt((fx * sx) ** 2 + (fy * sy) ** 2)
    amp = np.where(f > 0, 1.0 / np.maximum(f, 1e-6) ** (beta / 2.0), 0.0)
    x = np.real(np.fft.ifft2(np.fft.fft2(rng.standard_normal((N, N))) * amp))
    return (x - x.min()) / (x.max() - x.min() + 1e-9)


def blur(x, s):
    return ndimage.gaussian_filter(x, s, mode="wrap")


def stamp(canvas, cx, cy, patch, mode="add"):
    """Add / max a small patch into the canvas at (cx, cy), wrapping round the edges."""
    h, w = patch.shape
    ys = (np.arange(h) + int(cy) - h // 2) % N
    xs = (np.arange(w) + int(cx) - w // 2) % N
    if mode == "max":
        canvas[np.ix_(ys, xs)] = np.maximum(canvas[np.ix_(ys, xs)], patch)
    else:
        canvas[np.ix_(ys, xs)] += patch


def disc(r, soft=1.0):
    k = int(r + 3 * soft + 2)
    y, x = np.mgrid[-k:k + 1, -k:k + 1]
    d = np.sqrt(x * x + y * y)
    return np.clip((r + soft - d) / (2 * soft), 0, 1)


def draw_polyline(mask, pts, width):
    """Thin line through the points (wrapping), anti-aliased by a later blur."""
    for (x0, y0), (x1, y1) in zip(pts[:-1], pts[1:]):
        n = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
        xs = np.linspace(x0, x1, n)
        ys = np.linspace(y0, y1, n)
        for dx in range(-int(width), int(width) + 1):
            mask[(ys.astype(int)) % N, (xs.astype(int) + dx) % N] = 1.0
            mask[(ys.astype(int) + dx) % N, (xs.astype(int)) % N] = 1.0


def crack(rng, mask, x, y, length, angle, width, branch=0.25, depth=0, wander=0.13):
    pts = [(x, y)]
    a = angle
    step = 6.0
    for _ in range(int(length / step)):
        a += rng.normal(0, wander) + (angle - a) * 0.04      # wanders, but keeps its general direction
        x += np.cos(a) * step
        y += np.sin(a) * step
        pts.append((x, y))
        if depth < 2 and rng.random() < branch * 0.08:
            crack(rng, mask, x, y, length * rng.uniform(0.2, 0.45), a + rng.choice([-1, 1]) * rng.uniform(0.5, 1.1),
                  max(1, width - 1), branch, depth + 1, wander)
    draw_polyline(mask, pts, width)


def normal_map(height, strength):
    """DirectX normal (green down) from a height map, wrapping at the edges."""
    gx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5 * strength
    gy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5 * strength
    n = np.stack([-gx, gy, np.ones_like(height)], -1)      # +gy: DirectX convention (image rows go down)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def save(name, rgb, kind):
    os.makedirs(OUT, exist_ok=True)
    img = Image.fromarray((np.clip(rgb, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB")
    path = os.path.join(OUT, name + (".png" if kind == "N" else ".jpg"))
    if kind == "N":
        img.save(path, optimize=True)
    else:
        img.save(path, quality=92, subsampling=0)
    print("  texture", os.path.relpath(path, ROOT))


def srgb(linear):
    return np.where(linear <= 0.0031308, 12.92 * linear, 1.055 * np.power(np.maximum(linear, 0), 1 / 2.4) - 0.055)


# ----------------------------------------------------------------------------- rugged cast concrete wall
def rugged_concrete():
    rng = np.random.default_rng(4107)
    px_cm = N / 400.0                                   # 4 m per tile
    height = 0.35 * fbm(rng, 2.6, 1, 60) + 0.12 * fbm(rng, 1.2, 40)        # undulation + fine grain
    albedo = np.full((N, N), 0.30)                      # linear: old, dirty cast concrete
    albedo *= 0.72 + 0.5 * fbm(rng, 2.4, 1, 30)         # big cloudy tone differences
    albedo *= 0.9 + 0.2 * fbm(rng, 1.6, 8, 120)         # mid blotches
    albedo *= 0.9 + 0.2 * fbm(rng, 0.8, 120)            # sand / aggregate speckle
    warmth = fbm(rng, 2.6, 1, 10) - 0.5                 # some areas warmer (iron), some cooler (damp)
    rough = np.full((N, N), 0.9)
    rust = np.zeros((N, N))

    # plywood formwork: 2 x 3 sheets per tile, seams with a small lip, each pour a different tone
    sx, sy = N // 2, N / 3.0
    grain = stretched(rng, 0.08, 1.0)
    seams = np.zeros((N, N))
    seam_rows = []
    for i in range(2):
        for j in range(3):
            x0, y0 = i * sx, int(round(j * sy))
            y1 = int(round((j + 1) * sy))
            seam_rows.append(y0 % N)
            albedo[y0:y1, x0:x0 + sx] *= rng.uniform(0.85, 1.12)
            height[y0:y1, x0:x0 + sx] += 0.05 * (grain[y0:y1, x0:x0 + sx] - 0.5)
            albedo[y0:y1, x0:x0 + sx] *= 1 + 0.05 * (grain[y0:y1, x0:x0 + sx] - 0.5)
            seams[y0 % N, :] = 1
            seams[:, x0] = 1
            # form ties: 2 x 2 per sheet, plugged holes, rust bleeding down from most of them
            for tx in (0.25, 0.75):
                for ty in (0.22, 0.78):
                    cx = x0 + tx * sx + rng.normal(0, 3)
                    cy = y0 + ty * sy + rng.normal(0, 3)
                    hole = disc(1.3 * px_cm, 1.0)
                    stamp(height, cx, cy, -0.7 * hole)
                    stamp(albedo, cx, cy, -0.12 * hole)
                    if rng.random() < 0.55:
                        length = int(rng.gamma(2.0, 22) * px_cm + 10 * px_cm)
                        w = rng.uniform(1.5, 4.0) * px_cm
                        yy, xx = np.mgrid[0:length, -int(3 * w):int(3 * w) + 1]
                        wob = np.sin(yy / rng.uniform(30, 90) + rng.uniform(0, 6)) * w * 0.6
                        fall = np.exp(-yy / (length * 0.4)) * np.exp(-((xx - wob) / (w * (1 + yy / length))) ** 2)
                        fall *= 0.5 + 0.5 * ndimage.gaussian_filter(rng.random(fall.shape), 3)  # patchy, not a stripe
                        stamp(rust, cx, cy + length / 2, fall * rng.uniform(0.25, 0.6))
    seams = blur(seams, 1.0)
    lip = blur(seams, 4.0)
    height += 0.2 * lip - 0.7 * seams
    albedo *= 1 - 0.3 * seams

    # air pits (bug holes): mostly tiny, a few bigger
    pits = np.zeros((N, N))
    for _ in range(2600):
        r = rng.gamma(1.2, 0.5) * px_cm * 0.35 + 0.5
        stamp(pits, rng.uniform(0, N), rng.uniform(0, N), disc(r, 0.6), "max")
    height -= 0.45 * pits
    albedo *= 1 - 0.3 * pits

    # cracks: fairly straight, branching, with a damp seep and white salt bloom (efflorescence) under them
    cracks = np.zeros((N, N))
    for _ in range(8):
        crack(rng, cracks, rng.uniform(0, N), rng.uniform(0, N), rng.uniform(350, 1100),
              rng.uniform(0.9, 2.2) if rng.random() < 0.6 else rng.uniform(0, 2 * np.pi), 1, branch=0.22)
    cracks = blur(cracks, 0.7)
    seep = blur(np.roll(blur(cracks, 3), 25, axis=0), 8) * 2.5
    bloom = np.clip(blur(cracks, 10) * 6, 0, 1) * fbm(rng, 1.8, 20)
    height -= 0.9 * cracks
    albedo *= (1 - 0.35 * cracks) * (1 - 0.25 * np.clip(seep, 0, 1))
    albedo = albedo * (1 - 0.35 * bloom) + 0.55 * 0.35 * bloom

    # spalls: chunks knocked off - jagged outline, rough exposed aggregate, a rusty bar in two of them
    spall = np.zeros((N, N))
    agg = fbm(rng, 0.6, 120)
    for k in range(5):
        size = int(rng.uniform(25, 80) * px_cm / 2)
        cx, cy = rng.uniform(0, N), rng.uniform(0, N)
        blob = np.full((4 * size + 1, 4 * size + 1), -10.0)          # signed: > 0 inside a lump
        yy, xx = np.mgrid[-2 * size:2 * size + 1, -2 * size:2 * size + 1]
        for _ in range(int(rng.integers(3, 6))):                     # a few overlapping lumps
            ox, oy = rng.normal(0, size * 0.45, 2)
            rx, ry = size * rng.uniform(0.35, 0.8), size * rng.uniform(0.25, 0.6)
            t = rng.uniform(0, np.pi)
            u = (xx - ox) * np.cos(t) + (yy - oy) * np.sin(t)
            v = -(xx - ox) * np.sin(t) + (yy - oy) * np.cos(t)
            blob = np.maximum(blob, 1 - (u / rx) ** 2 - (v / ry) ** 2)
        chip = ndimage.gaussian_filter(rng.random(blob.shape), 1.5)
        chip = (chip - chip.min()) / (chip.max() - chip.min() + 1e-9)
        blob = np.clip((blob + 0.5 * (chip - 0.5)) * 6, 0, 1)       # chipped, sharp edge
        stamp(spall, cx, cy, blob, "max")
        if k < 2:
            bar = np.zeros_like(blob)
            bar[2 * size - 4:2 * size + 5, :] = 1
            bar = ndimage.rotate(bar, rng.uniform(-25, 25), reshape=False, order=1)
            stamp(rust, cx, cy, 1.2 * blur(bar, 1.2) * blob)
    spall = blur(spall, 0.8)
    height -= spall * (1.3 + 1.0 * agg)
    albedo = albedo * (1 - spall) + spall * (0.17 + 0.22 * agg)
    rough = rough * (1 - spall) + 0.97 * spall

    # dirty water streaks: from the top of each pour (seam rows) and from anywhere
    streak = np.zeros((N, N))
    for k in range(80):
        length = int(rng.uniform(0.1, 0.7) * N)
        w = rng.uniform(3, 20)
        yy, xx = np.mgrid[0:length, -int(3 * w):int(3 * w) + 1]
        wob = np.sin(yy / rng.uniform(40, 140) + rng.uniform(0, 6)) * w * 0.8
        s_ = np.exp(-((xx - wob) / w) ** 2) * np.clip(1 - yy / length, 0, 1) ** 1.3
        s_ *= 0.55 + 0.45 * np.sin(yy / rng.uniform(12, 50) + rng.uniform(0, 6)) ** 2
        top = seam_rows[k % len(seam_rows)] + 4 if k < 50 else rng.uniform(0, N)
        stamp(streak, rng.uniform(0, N), top + length / 2, s_ * rng.uniform(0.3, 0.9))
    streak = np.clip(streak, 0, 1)
    damp = np.clip((fbm(rng, 2.8, 1, 12) - 0.5) * 2.5, 0, 1)
    grime = np.clip((fbm(rng, 2.2, 2, 40) - 0.45) * 2.2, 0, 1)
    albedo *= (1 - 0.45 * streak) * (1 - 0.3 * damp) * (1 - 0.3 * grime)
    rough -= 0.2 * streak + 0.12 * damp
    wet = np.clip(streak + damp, 0, 1)

    rust = np.clip(rust, 0, 1)
    colour = np.stack([albedo * (1 + 0.08 * warmth), albedo * 0.985, albedo * (0.95 - 0.08 * warmth)], -1)
    colour = colour * (1 - 0.15 * wet[..., None]) + wet[..., None] * 0.15 * np.array([0.10, 0.12, 0.09])   # damp, greenish
    rust_col = np.array([0.22, 0.11, 0.045])
    colour = colour * (1 - 0.75 * rust[..., None]) + 0.75 * rust[..., None] * rust_col
    rough = np.clip(rough - 0.05 * rust, 0.35, 1.0)

    save("T_AH_RuggedConcrete_D", srgb(colour), "D")
    save("T_AH_RuggedConcrete_N", normal_map(height, 14.0), "N")
    mr = np.stack([np.ones((N, N)), rough, np.zeros((N, N))], -1)
    save("T_AH_RuggedConcrete_MR", mr, "MR")


# ----------------------------------------------------------------------------- worn concrete floor slab
def worn_floor():
    rng = np.random.default_rng(4108)
    px_cm = N / 300.0                                   # 3 m per tile
    height = 0.15 * fbm(rng, 2.4, 1, 50) + 0.06 * fbm(rng, 1.0, 60)
    albedo = np.full((N, N), 0.28)
    albedo *= 0.75 + 0.45 * fbm(rng, 2.4, 1, 30)
    albedo *= 0.88 + 0.24 * fbm(rng, 0.7, 150)          # exposed aggregate speckle
    traffic = np.clip((fbm(rng, 2.6, 1, 8) - 0.5) * 3, 0, 1)     # worn lighter, rougher where people walk
    albedo *= 1 + 0.25 * traffic
    rough = 0.6 + 0.16 * fbm(rng, 2.0, 1, 30) + 0.15 * traffic   # power-floated: semi-gloss, worn dull

    # trowel swirl: faint arcs
    swirl = np.zeros((N, N))
    for _ in range(220):
        r = rng.uniform(25, 70) * px_cm
        a0 = rng.uniform(0, 2 * np.pi)
        t = np.linspace(a0, a0 + rng.uniform(0.6, 1.6), 200)
        cx, cy = rng.uniform(0, N), rng.uniform(0, N)
        pts = list(zip(cx + r * np.cos(t), cy + r * np.sin(t)))
        draw_polyline(swirl, pts, 2)
    swirl = blur(swirl, 3)
    albedo *= 1 + 0.06 * swirl
    rough -= 0.06 * swirl

    # saw-cut joints every 1.5 m
    joints = np.zeros((N, N))
    for k in range(2):
        p = k * N // 2
        joints[:, p] = 1
        joints[p, :] = 1
    joints = blur(joints, 1.3)
    height -= 1.0 * joints
    albedo *= 1 - 0.5 * joints

    # scuffs and tyre marks
    scuff = np.zeros((N, N))
    for _ in range(22):
        length = rng.uniform(15, 90) * px_cm
        a = rng.uniform(0, np.pi)
        cx, cy = rng.uniform(0, N), rng.uniform(0, N)
        pts = [(cx + np.cos(a) * s + rng.normal(0, 2), cy + np.sin(a) * s + rng.normal(0, 2))
               for s in np.linspace(-length / 2, length / 2, 30)]
        draw_polyline(scuff, pts, int(rng.integers(1, 3)))
    for _ in range(3):                                  # pallet-truck tyre marks: long gentle curves
        cx, cy = rng.uniform(0, N), rng.uniform(0, N)
        a, r = rng.uniform(0, 2 * np.pi), rng.uniform(300, 900)
        t = np.linspace(0, rng.uniform(0.8, 1.6), 120)
        for off in (0, 9 * px_cm):
            draw_polyline(scuff, list(zip(cx + (r + off) * np.cos(a + t), cy + (r + off) * np.sin(a + t))), 3)
    scuff = blur(scuff, 2.5)
    albedo *= 1 - 0.16 * np.clip(scuff, 0, 1)
    dust = np.clip((fbm(rng, 2.4, 2, 30) - 0.55) * 2, 0, 1)
    albedo = albedo * (1 - 0.3 * dust) + 0.3 * dust * 0.33                  # dust drifts
    rough += 0.15 * dust

    # oil spots: dark, glossy
    oil = np.zeros((N, N))
    for _ in range(14):
        r = rng.uniform(4, 30) * px_cm
        stamp(oil, rng.uniform(0, N), rng.uniform(0, N), disc(r, r * 0.6) * rng.uniform(0.3, 0.8), "max")
    oil = blur(oil, 6) * (0.7 + 0.3 * fbm(rng, 1.5, 4))
    albedo *= 1 - 0.55 * oil
    rough -= 0.35 * oil

    # hairline cracks
    cracks = np.zeros((N, N))
    for _ in range(5):
        crack(rng, cracks, rng.uniform(0, N), rng.uniform(0, N), rng.uniform(200, 600), rng.uniform(0, 2 * np.pi), 1)
    cracks = blur(cracks, 0.7)
    height -= 0.8 * cracks
    albedo *= 1 - 0.3 * cracks

    colour = np.stack([albedo, albedo * 0.98, albedo * 0.94], -1)
    save("T_AH_WornFloor_D", srgb(colour), "D")
    save("T_AH_WornFloor_N", normal_map(height, 10.0), "N")
    save("T_AH_WornFloor_MR", np.stack([np.ones((N, N)), np.clip(rough, 0.25, 0.95), np.zeros((N, N))], -1), "MR")


if __name__ == "__main__":
    print("Generating rugged facility textures...")
    rugged_concrete()
    worn_floor()
    print("Done.")
