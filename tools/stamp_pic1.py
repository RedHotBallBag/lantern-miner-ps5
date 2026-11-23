"""Stamps the game version (from ps5/game/lm.h) onto the console launch background.
   assets/brand/pic1_base.png (clean master) -> assets/brand/pic1.png (what the build ships).
   Run on Windows after changing LM_VERSION / LM_BUILD:  python tools/stamp_pic1.py"""
import os, re
from PIL import Image, ImageDraw, ImageFont
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
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
img.convert("RGB").save(os.path.join(ROOT, "assets/brand/pic1.png"))
print("stamped", text)
