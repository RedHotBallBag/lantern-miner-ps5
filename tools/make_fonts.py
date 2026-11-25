# Copyright (C) 2026 RedHotBallBag
# SPDX-License-Identifier: GPL-3.0-or-later
#!/usr/bin/env python3
"""Instantiates static weights from the variable Google fonts (stb_truetype only reads the default master).
Run on Windows (fontTools): python tools/make_fonts.py"""
import os
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, "..", "assets", "fonts")


def inst(src, dst, axes):
    f = TTFont(os.path.join(FONTS, src))
    out = instancer.instantiateVariableFont(f, axes, inplace=False)
    out.save(os.path.join(FONTS, dst))
    print("wrote", dst, axes)


inst("Fraunces-var.ttf", "Fraunces-Bold.ttf", {"wght": 700, "opsz": 72, "SOFT": 50, "WONK": 0})
inst("Fraunces-var.ttf", "Fraunces-SemiBold.ttf", {"wght": 600, "opsz": 36, "SOFT": 50, "WONK": 0})
inst("DMSans-var.ttf", "DMSans-Medium.ttf", {"wght": 500, "opsz": 14})
inst("DMSans-var.ttf", "DMSans-Bold.ttf", {"wght": 700, "opsz": 14})
for v in ("Fraunces-var.ttf", "DMSans-var.ttf"):
    os.remove(os.path.join(FONTS, v))
