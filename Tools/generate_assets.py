"""
generate_assets.py - procedurally builds MyProject5's placeholder sounds and textures.

Everything here is synthesised from noise and maths (no samples, no licences to worry
about). Re-run any time to regenerate:   python Tools/generate_assets.py
Needs: numpy, scipy, pillow   (pip install numpy scipy pillow)

Output:
  Assets/Audio/*.wav      44.1 kHz, 16-bit mono
  Assets/Textures/*.png   512x512, seamlessly tileable
Then import them into Unreal with Tools/ue_setup_project.py (see README_ASSETS.md).
"""

import os
import numpy as np
from scipy import signal
from scipy.io import wavfile
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUDIO_DIR = os.path.join(ROOT, "Assets", "Audio")
TEX_DIR = os.path.join(ROOT, "Assets", "Textures")
SR = 44100
rng = np.random.default_rng(2026)


# ============================================================ audio helpers

def t_axis(seconds):
    return np.arange(int(SR * seconds)) / SR


def noise(seconds):
    return rng.standard_normal(int(SR * seconds))


def band(x, lo, hi, order=4):
    sos = signal.butter(order, [lo, hi], btype="bandpass", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def lowpass(x, hz, order=4):
    sos = signal.butter(order, hz, btype="lowpass", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def highpass(x, hz, order=2):
    sos = signal.butter(order, hz, btype="highpass", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def env_ad(n, attack, decay_tau):
    """Linear attack then exponential decay, lengths in seconds."""
    t = np.arange(n) / SR
    a = np.clip(t / max(attack, 1e-4), 0, 1)
    d = np.exp(-np.maximum(t - attack, 0) / decay_tau)
    return a * d


def pad_to(x, seconds):
    n = int(SR * seconds)
    out = np.zeros(n)
    out[: min(n, len(x))] = x[:n]
    return out


def room_tail(x, seconds=0.6, tau=0.18, mix=0.25):
    """Cheap concrete-room reverb: convolve with decaying, darkened noise."""
    ir = noise(seconds) * np.exp(-t_axis(seconds) / tau)
    ir = lowpass(ir, 4000)
    ir /= np.max(np.abs(ir)) + 1e-9
    wet = signal.fftconvolve(x, ir)[: len(x) + int(SR * seconds)]
    dry = np.concatenate([x, np.zeros(len(wet) - len(x))])
    wet /= np.max(np.abs(wet)) + 1e-9
    return dry + mix * wet * np.max(np.abs(x))


def save_wav(name, x, peak=0.89):
    x = np.asarray(x, dtype=np.float64)
    x = x - np.mean(x)
    # 3 ms fades so nothing clicks
    f = int(SR * 0.003)
    x[:f] *= np.linspace(0, 1, f)
    x[-f:] *= np.linspace(1, 0, f)
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    wavfile.write(os.path.join(AUDIO_DIR, name + ".wav"), SR, (x * 32767).astype(np.int16))
    print("  audio", name)


# ============================================================ sounds

def whoosh(seconds, centre, width, seed_shift=0.0):
    """Air displaced by a swung object: bandpassed noise, swelling then dying,
    with the centre frequency drifting up then down like a doppler pass."""
    n = int(SR * seconds)
    x = noise(seconds)
    # process in short overlapping blocks with a moving band centre
    block, hop = 1024, 512
    out = np.zeros(n + block)
    win = np.hanning(block)
    for i, start in enumerate(range(0, n - block, hop)):
        p = start / n
        c = centre * (0.75 + 0.6 * np.sin(np.pi * p) + seed_shift)
        seg = band(x[start:start + block], max(c - width, 80), c + width, order=2)
        out[start:start + block] += seg * win
    out = out[:n]
    t = t_axis(seconds)
    shape = np.sin(np.pi * np.clip(t / seconds, 0, 1)) ** 1.6
    return out * shape


def flesh_hit(variant):
    seconds = 0.45
    t = t_axis(seconds)
    f0 = 85 - 10 * variant
    thump = np.sin(2 * np.pi * np.cumsum(f0 * np.exp(-t * 9) + 38) / SR) * np.exp(-t / 0.09)
    slap = band(noise(seconds), 300, 1800) * env_ad(len(t), 0.002, 0.035)
    squelch = band(noise(seconds), 500 + 150 * variant, 1400) * env_ad(len(t), 0.02, 0.07) * 0.5
    crunch = highpass(noise(seconds), 2500) * env_ad(len(t), 0.001, 0.012) * (0.3 + 0.2 * variant)
    return lowpass(thump * 1.2 + slap * 0.6 + squelch + crunch, 6000)


def metal_clang(variant):
    seconds = 1.4
    t = t_axis(seconds)
    base = 380 + 90 * variant
    ratios = [1.0, 2.76, 5.40, 8.93, 13.34, 18.64]
    decays = [0.55, 0.38, 0.22, 0.15, 0.09, 0.06]
    x = np.zeros_like(t)
    for r, d in zip(ratios, decays):
        f = base * r * (1 + rng.uniform(-0.01, 0.01))
        x += np.sin(2 * np.pi * f * t + rng.uniform(0, 6.28)) * np.exp(-t / d) / r ** 0.5
    click = highpass(noise(seconds), 3000) * env_ad(len(t), 0.0005, 0.006) * 1.5
    thud = lowpass(noise(seconds), 400) * env_ad(len(t), 0.001, 0.04) * 2.0
    return x + click + thud


def gunshot():
    seconds = 0.9
    t = t_axis(seconds)
    crack = highpass(noise(seconds), 1500) * env_ad(len(t), 0.0005, 0.018) * 2.0
    body = band(noise(seconds), 150, 2500) * env_ad(len(t), 0.001, 0.07)
    boom = np.sin(2 * np.pi * np.cumsum(60 + 90 * np.exp(-t * 30)) / SR) * np.exp(-t / 0.12) * 1.4
    mech = band(noise(seconds), 2000, 6000) * env_ad(len(t), 0.06, 0.01) * 0.3   # slide cycling
    x = np.tanh(2.5 * (crack + body + boom + mech))  # soft clip for punch
    return room_tail(x, seconds=0.9, tau=0.25, mix=0.45)


def click(seconds=0.12, hz=3500, tau=0.004):
    t = t_axis(seconds)
    x = band(noise(seconds), hz * 0.6, min(hz * 1.6, 18000)) * env_ad(len(t), 0.0003, tau)
    x += np.sin(2 * np.pi * hz * 0.5 * t) * np.exp(-t / 0.01) * 0.3
    return x


def reload_sound():
    seconds = 1.1
    out = np.zeros(int(SR * seconds))
    def place(snd, at):
        i = int(SR * at)
        out[i:i + len(snd)] += snd[: len(out) - i]
    place(click(0.15, 2200, 0.008) * 0.8, 0.05)               # mag release
    slide = band(noise(0.25), 800, 4000) * np.hanning(int(SR * 0.25)) * 0.35
    place(slide, 0.25)                                         # mag out/in scrape
    place(click(0.15, 1800, 0.012), 0.55)                      # mag seated
    place(band(noise(0.12), 1500, 6000) * np.hanning(int(SR * 0.12)) * 0.5, 0.8)
    place(click(0.15, 2600, 0.01) * 1.2, 0.93)                 # slide release
    return out


def footstep(variant):
    seconds = 0.35
    t = t_axis(seconds)
    heel = lowpass(noise(seconds), 260 + 40 * variant) * env_ad(len(t), 0.002, 0.03) * 2.5
    grit = band(noise(seconds), 1500, 7000) * env_ad(len(t), 0.004, 0.045 + 0.01 * variant) * 0.35
    toe = lowpass(noise(seconds), 500) * env_ad(len(t), 0.002, 0.02)
    toe = np.roll(toe, int(SR * (0.045 + 0.01 * variant)))
    return room_tail(heel + grit + toe * 0.8, seconds=0.35, tau=0.08, mix=0.15)


def bodycam_hiss_loop(seconds=12.0):
    t = t_axis(seconds)
    # pinkish noise: white noise with a -3 dB/oct tilt
    white = noise(seconds)
    spec = np.fft.rfft(white)
    freqs = np.fft.rfftfreq(len(white), 1 / SR)
    spec /= np.sqrt(np.maximum(freqs, 20))
    pink = np.fft.irfft(spec, len(white))
    pink = band(pink, 120, 9000, order=2)
    pink /= np.max(np.abs(pink))
    hum = (np.sin(2 * np.pi * 50 * t) + 0.35 * np.sin(2 * np.pi * 150 * t)) * 0.05
    # slow "wind/handling" wobble
    wobble = lowpass(noise(seconds), 2) ; wobble /= np.max(np.abs(wobble))
    x = pink * (0.55 + 0.25 * wobble) + hum
    # random crackles
    for _ in range(int(seconds * 3)):
        i = rng.integers(0, len(t) - 400)
        x[i:i + 400] += highpass(noise(400 / SR), 3000) * np.exp(-np.arange(400) / 60) * rng.uniform(0.3, 0.9)
    # make it loop seamlessly: crossfade the last second into the first
    xf = SR
    fade = np.linspace(0, 1, xf)
    x[:xf] = x[:xf] * fade + x[-xf:] * (1 - fade)
    return x[:-xf]


def player_hurt():
    seconds = 0.6
    t = t_axis(seconds)
    thump = np.sin(2 * np.pi * np.cumsum(55 + 40 * np.exp(-t * 12)) / SR) * np.exp(-t / 0.15)
    ring = np.sin(2 * np.pi * 3100 * t) * np.exp(-t / 0.25) * 0.12   # ear ring
    muffled = lowpass(noise(seconds), 600) * env_ad(len(t), 0.002, 0.06)
    return thump * 1.5 + ring + muffled


def build_audio():
    os.makedirs(AUDIO_DIR, exist_ok=True)
    save_wav("SW_Melee_Whoosh_01", whoosh(0.38, 900, 450))
    save_wav("SW_Melee_Whoosh_02", whoosh(0.32, 1300, 600, 0.1))
    save_wav("SW_Melee_Whoosh_Heavy", whoosh(0.55, 500, 300, -0.1))
    for v in range(3):
        save_wav(f"SW_Melee_HitFlesh_0{v + 1}", flesh_hit(v))
    for v in range(3):
        save_wav(f"SW_Melee_HitMetal_0{v + 1}", metal_clang(v))
    save_wav("SW_Gun_Shot", gunshot())
    save_wav("SW_Gun_DryFire", click(0.15, 2800, 0.006))
    save_wav("SW_Gun_Reload", reload_sound())
    for v in range(4):
        save_wav(f"SW_Footstep_Concrete_0{v + 1}", footstep(v))
    save_wav("SW_Player_Hurt", player_hurt())
    save_wav("SW_Ambience_BodycamHiss_Loop", bodycam_hiss_loop(), peak=0.35)


# ============================================================ texture helpers

N = 512


def tile_noise(octaves=6, persistence=0.55, base=4, seed=0):
    """Fractal value-ish noise that tiles perfectly (built on periodic FFT noise)."""
    r = np.random.default_rng(seed)
    out = np.zeros((N, N))
    amp, total = 1.0, 0.0
    fy = np.fft.fftfreq(N)[:, None]
    fx = np.fft.fftfreq(N)[None, :]
    rad = np.sqrt(fx ** 2 + fy ** 2)
    for o in range(octaves):
        freq = base * 2 ** o / N
        spec = np.fft.fft2(r.standard_normal((N, N)))
        spec *= np.exp(-((rad - freq) ** 2) / (2 * (freq * 0.5) ** 2))
        layer = np.real(np.fft.ifft2(spec))
        layer /= np.std(layer) + 1e-9
        out += layer * amp
        total += amp
        amp *= persistence
    out /= total
    return (out - out.min()) / (out.max() - out.min())


def to_img(arr):
    return Image.fromarray(np.clip(arr * 255, 0, 255).astype(np.uint8))


def normal_from_height(h, strength=4.0):
    # wrap-around gradients keep the normal map tileable
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * strength
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * strength
    nz = np.ones_like(h)
    n = np.stack([-dx, -dy, nz], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * 0.5 + 0.5


def save_png(name, arr):
    to_img(arr).save(os.path.join(TEX_DIR, name + ".png"))
    print("  texture", name)


def build_textures():
    os.makedirs(TEX_DIR, exist_ok=True)

    # --- Concrete: mottled grey, darker stains, pitting
    base = tile_noise(7, 0.6, 3, seed=1)
    fine = tile_noise(4, 0.5, 64, seed=2)
    stains = np.clip((tile_noise(5, 0.5, 2, seed=3) - 0.55) * 3, 0, 1)
    pits = (tile_noise(2, 0.5, 96, seed=4) > 0.82).astype(float)
    grey = 0.42 + 0.18 * (base - 0.5) + 0.08 * (fine - 0.5) - 0.18 * stains - 0.15 * pits
    concrete = np.stack([grey * 0.98, grey * 0.97, grey * 0.95], -1)
    save_png("T_Concrete_D", concrete)
    save_png("T_Concrete_N", normal_from_height(0.6 * fine + 0.4 * base - 0.5 * pits, 3.0))

    # --- Rusted metal: dark steel with orange-brown rust blooms
    steel = 0.22 + 0.06 * (tile_noise(5, 0.5, 8, seed=5) - 0.5)
    rust_mask = np.clip((tile_noise(6, 0.6, 3, seed=6) - 0.45) * 3.5, 0, 1)
    rust_var = tile_noise(4, 0.5, 32, seed=7)
    rust = np.stack([0.42 + 0.15 * rust_var, 0.20 + 0.07 * rust_var, 0.08 + 0.03 * rust_var], -1)
    metal = np.stack([steel, steel * 1.02, steel * 1.06], -1)
    rusted = metal * (1 - rust_mask[..., None]) + rust * rust_mask[..., None]
    save_png("T_RustMetal_D", rusted)
    save_png("T_RustMetal_N", normal_from_height(rust_mask * 0.6 + 0.4 * rust_var, 2.0))
    # roughness: shiny steel, rough rust
    save_png("T_RustMetal_R", np.repeat((0.35 + 0.6 * rust_mask)[..., None], 3, -1))

    # --- Wood: grain along Y, tileable because the stripe count is an integer
    yy, xx = np.meshgrid(np.arange(N) / N, np.arange(N) / N, indexing="ij")
    warp = tile_noise(4, 0.5, 2, seed=8)
    grain = np.sin(2 * np.pi * (xx * 18 + warp * 3.0)) * 0.5 + 0.5
    fibres = tile_noise(3, 0.5, 128, seed=9)
    tone = 0.75 + 0.25 * grain ** 3 - 0.1 * fibres
    wood = np.stack([0.45 * tone, 0.29 * tone, 0.16 * tone], -1)
    save_png("T_Wood_D", wood)

    # --- Dirty floor tiles: 4x4 grid of worn tiles with grout
    tile = np.ones((N, N))
    g = N // 4
    for k in range(5):
        tile[max(k * g - 3, 0):k * g + 3, :] = 0.0
        tile[:, max(k * g - 3, 0):k * g + 3] = 0.0
    grime = tile_noise(6, 0.55, 4, seed=10)
    t_col = (0.55 + 0.1 * (tile_noise(3, 0.5, 16, seed=11) - 0.5)) * tile + 0.12 * (1 - tile)
    t_col = t_col - 0.25 * np.clip(grime - 0.5, 0, 1)
    tiles = np.stack([t_col * 0.92, t_col * 0.95, t_col * 0.90], -1)
    save_png("T_FloorTiles_D", tiles)
    save_png("T_FloorTiles_N", normal_from_height(tile * 0.8 + 0.2 * grime, 2.5))


if __name__ == "__main__":
    print("Generating audio...")
    build_audio()
    print("Generating textures...")
    build_textures()
    print("Done.")
