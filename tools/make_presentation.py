# Copyright (C) 2026 RedHotBallBag
# SPDX-License-Identifier: GPL-3.0-or-later
"""Makes the PS5 home screen presentation files in assets/brand. Run on Windows:

  python tools/make_presentation.py [path to at9tool.exe]

  pic1_base.png + version from ps5/game/lm.h -> pic1.png
                                             -> pic.dds   (3840x2160 BC7, shipped as pic0.dds and pic1.dds)
  assets/music/theme.wav                     -> snd0.at9  (home screen music; needs Sony's at9tool, which is not
                                                           included: pass its path or set the AT9TOOL variable)

Needs Pillow, numpy and ffmpeg."""
import os
import re
import struct
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
W, H = 3840, 2160
CLIP = 60  # seconds of the theme on the home screen; the file must stay under 2 MiB
WEIGHTS = np.array([0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64], dtype=np.int32)


def stamp():
    """pic1_base.png (clean master) plus the version from lm.h -> pic1.png."""
    h = open(os.path.join(ROOT, "ps5/game/lm.h"), encoding="utf-8").read()
    ver = re.search(r'#define LM_VERSION "([^"]+)"', h).group(1)
    build = int(re.search(r'#define LM_BUILD (\d+)', h).group(1))
    text = f"v{ver}  ·  Build {build:02d}"
    img = Image.open(os.path.join(ROOT, "assets/brand/pic1_base.png")).convert("RGBA")
    font = ImageFont.truetype(os.path.join(ROOT, "assets/fonts/JetBrainsMono-Bold.ttf"), 34)
    d = ImageDraw.Draw(img)
    w = d.textlength(text, font=font)
    x, y = img.width - w - 56, img.height - 84
    pad = 18
    band = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(band).rounded_rectangle([x - pad, y - pad + 6, x + w + pad, y + 34 + pad], 16, fill=(10, 8, 13, 170))
    img = Image.alpha_composite(img, band)
    ImageDraw.Draw(img).text((x, y), text, font=font, fill=(245, 166, 35, 255))
    img = img.convert("RGB")
    img.save(os.path.join(ROOT, "assets/brand/pic1.png"))
    print("pic1.png stamped", text)
    return img


def put(bits: np.ndarray, start: int, width: int, values: np.ndarray) -> None:
    for k in range(width):
        bits[:, start + k] = (values >> k) & 1


def encode_bc7_mode6(rgb: np.ndarray) -> bytes:
    """rgb: (H, W, 3) uint8, H and W multiples of 4. Returns the BC7 blocks, row by row."""
    h, w, _ = rgb.shape
    bh, bw = h // 4, w // 4
    # (blocks, 16, 3) with pixels in raster order inside each block
    blocks = rgb.reshape(bh, 4, bw, 4, 3).transpose(0, 2, 1, 3, 4).reshape(bh * bw, 16, 3).astype(np.float32)
    n = blocks.shape[0]

    mean = blocks.mean(axis=1, keepdims=True)
    centred = blocks - mean
    cov = np.einsum("npi,npj->nij", centred, centred)
    _, vecs = np.linalg.eigh(cov)
    axis = vecs[:, :, 2]  # largest eigenvalue
    proj = np.einsum("npi,ni->np", centred, axis)
    lo = proj.argmin(axis=1)
    hi = proj.argmax(axis=1)
    idx = np.arange(n)
    e0 = blocks[idx, lo]
    e1 = blocks[idx, hi]

    def quantise(c: np.ndarray) -> np.ndarray:
        # value8 = v7 * 2 + 1 (p-bit 1), so alpha 255 is exact and colour error is at most one level
        return np.clip(np.rint((c - 1.0) / 2.0), 0, 127).astype(np.int32)

    q0, q1 = quantise(e0), quantise(e1)
    d0 = (q0 * 2 + 1).astype(np.float32)
    d1 = (q1 * 2 + 1).astype(np.float32)

    span = d1 - d0
    denom = np.maximum((span * span).sum(axis=1), 1e-6)
    t = np.einsum("npi,ni->np", blocks - d0[:, None, :], span) / denom[:, None]
    target = np.clip(t, 0.0, 1.0) * 64.0
    index = np.abs(target[:, :, None] - WEIGHTS[None, None, :]).argmin(axis=2).astype(np.int32)

    # the first pixel's index has one bit less: its top bit must be 0, so swap the endpoints when it is not
    swap = index[:, 0] >= 8
    index[swap] = 15 - index[swap]
    q0s = np.where(swap[:, None], q1, q0)
    q1s = np.where(swap[:, None], q0, q1)

    bits = np.zeros((n, 128), dtype=np.uint8)
    bits[:, 6] = 1  # mode 6: six zero bits then a one
    pos = 7
    for channel in range(3):
        put(bits, pos, 7, q0s[:, channel])
        pos += 7
        put(bits, pos, 7, q1s[:, channel])
        pos += 7
    put(bits, pos, 7, np.full(n, 127, dtype=np.int32))  # alpha 0
    pos += 7
    put(bits, pos, 7, np.full(n, 127, dtype=np.int32))  # alpha 1
    pos += 7
    bits[:, pos] = 1  # p-bit 0
    bits[:, pos + 1] = 1  # p-bit 1
    pos += 2
    put(bits, pos, 3, index[:, 0])
    pos += 3
    for p in range(1, 16):
        put(bits, pos, 4, index[:, p])
        pos += 4
    assert pos == 128
    return np.packbits(bits, axis=1, bitorder="little").tobytes()


def decode_bc7_mode6(data: bytes, w: int, h: int) -> np.ndarray:
    n = (w // 4) * (h // 4)
    raw = np.frombuffer(data, dtype=np.uint8).reshape(n, 16)
    bits = np.unpackbits(raw, axis=1, bitorder="little").astype(np.int32)

    def get(start: int, width: int) -> np.ndarray:
        return sum(bits[:, start + k] << k for k in range(width))

    assert (bits[:, :7] == np.array([0, 0, 0, 0, 0, 0, 1])).all()
    pos = 7
    ends = []
    for _ in range(3):
        a = get(pos, 7)
        b = get(pos + 7, 7)
        pos += 14
        ends.append((a, b))
    pos += 14  # alpha
    p0, p1 = get(pos, 1), get(pos + 1, 1)
    pos += 2
    index = np.zeros((n, 16), dtype=np.int32)
    index[:, 0] = get(pos, 3)
    pos += 3
    for p in range(1, 16):
        index[:, p] = get(pos, 4)
        pos += 4
    weights = WEIGHTS[index]
    out = np.zeros((n, 16, 3), dtype=np.int32)
    for channel, (a, b) in enumerate(ends):
        c0 = (a * 2 + p0)[:, None]
        c1 = (b * 2 + p1)[:, None]
        out[:, :, channel] = (c0 * (64 - weights) + c1 * weights + 32) >> 6
    bh, bw = h // 4, w // 4
    return out.reshape(bh, bw, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h, w, 3).astype(np.uint8)


def dds_header(w: int, h: int) -> bytes:
    linear = (w // 4) * (h // 4) * 16
    header = bytearray(128)
    header[0:4] = b"DDS "
    struct.pack_into("<I", header, 4, 124)
    struct.pack_into("<I", header, 8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000)  # caps, height, width, pixel format, linear size
    struct.pack_into("<II", header, 12, h, w)
    struct.pack_into("<I", header, 20, linear)
    struct.pack_into("<I", header, 28, 1)  # one mip level
    struct.pack_into("<I", header, 76, 32)  # pixel format size
    struct.pack_into("<I", header, 80, 0x4)  # DDPF_FOURCC
    header[84:88] = b"DX10"
    struct.pack_into("<I", header, 108, 0x1000)  # DDSCAPS_TEXTURE
    ext = struct.pack("<IIIII", 98, 3, 0, 1, 0)  # BC7_UNORM, 2D texture, no flags, one array slice
    return bytes(header) + ext


def background(img):
    rgb = np.asarray(img.resize((W, H), Image.LANCZOS))
    blocks = encode_bc7_mode6(rgb)
    mse = np.mean((decode_bc7_mode6(blocks, W, H).astype(np.float64) - rgb.astype(np.float64)) ** 2)
    psnr = 10 * np.log10(255.0 * 255.0 / max(mse, 1e-9))
    assert psnr > 30, psnr
    with open(os.path.join(ROOT, "assets/brand/pic.dds"), "wb") as f:
        f.write(dds_header(W, H) + blocks)
    print("pic.dds %dx%d, PSNR %.1f dB" % (W, H, psnr))


def sound(tool):
    if not tool or not os.path.isfile(tool):
        print("snd0.at9 NOT remade: no at9tool (pass its path or set AT9TOOL)")
        return
    with tempfile.TemporaryDirectory() as work:
        wav = os.path.join(work, "snd0.wav")
        out = os.path.join(work, "snd0.at9")
        # quiet (-28 LUFS, matching loudnessSnd0 in param.json) with one fixed gain, so the dynamics are untouched;
        # short fades so the loop point is clean
        theme = os.path.join(ROOT, "assets/music/theme.wav")
        meter = subprocess.run(["ffmpeg", "-hide_banner", "-t", str(CLIP), "-i", theme, "-map", "0:a", "-af", "ebur128", "-f", "null", "-"],
                               check=True, capture_output=True, text=True, errors="replace").stderr
        gain = -28.0 - float(re.findall(r"I:\s+(-?[\d.]+) LUFS", meter)[-1])
        fades = "volume=%.2fdB,afade=t=in:d=0.05,afade=t=out:st=%d:d=3" % (gain, CLIP - 3)
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", theme, "-t", str(CLIP),
                        "-map_metadata", "-1", "-map", "0:a", "-af", fades, "-ar", "48000", "-ac", "2", "-c:a", "pcm_s16le", wav],
                       check=True)
        subprocess.run([tool, "-e", "-br", "192", "-wholeloop", wav, out], check=True)
        with open(out, "rb") as f:
            data = f.read()
    assert len(data) <= 2 * 1024 * 1024, len(data)
    with open(os.path.join(ROOT, "assets/brand/snd0.at9"), "wb") as f:
        f.write(data)
    print("snd0.at9", len(data), "bytes")


if __name__ == "__main__":
    background(stamp())
    sound(sys.argv[1] if len(sys.argv) > 1 else os.environ.get("AT9TOOL"))
