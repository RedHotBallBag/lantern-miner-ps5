/*
 * Lantern Miner: UI drawing helpers.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "../gfx/vkr.h"
#include "lm.h"

#define HEXA(h, a) VKR_RGBA(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255, (unsigned)(lm_clamp((a), 0.0, 1.0) * 255.0 + 0.5))
#define HEX(h) VKR_RGB(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255)

/* palette (0xRRGGBB) */
enum {
    C_CREAM = 0xf6efe2, C_MUTED = 0xb6aa9c, C_DIM = 0x8d8175, C_AMBER = 0xf5a623, C_AMBER_HI = 0xffba42, C_YELLOW = 0xffe347,
    C_PINK = 0xf43fa5, C_GREEN = 0x2ce87d, C_CYAN = 0x22e4f5, C_PANEL = 0x1b1722, C_PANEL2 = 0x241f2e, C_INK = 0x141118,
    C_BG = 0x110f15
};

enum { G_CROSS, G_CIRCLE, G_SQUARE, G_TRIANGLE, G_DPAD, G_DPAD_LR, G_DPAD_UD, G_OPTIONS, G_L1, G_R1, G_R3, G_STICK };

/* text */
float ui_text_w(struct vkr *r, enum vkr_font f, float size, char const *s);
void ui_text(struct vkr *r, enum vkr_font f, float size, float x, float y, uint32_t col, char const *s);
void ui_text_shadow(struct vkr *r, enum vkr_font f, float size, float x, float y, uint32_t col, char const *s);
void ui_text_c(struct vkr *r, enum vkr_font f, float size, float cx, float y, uint32_t col, char const *s);
void ui_text_r(struct vkr *r, enum vkr_font f, float size, float rx, float y, uint32_t col, char const *s);
/* letter-spaced text */
void ui_text_spaced(struct vkr *r, enum vkr_font f, float size, float x, float y, float spacing, uint32_t col, char const *s);
/* word-wrapped paragraph; returns the number of lines drawn */
int ui_paragraph(struct vkr *r, enum vkr_font f, float size, float x, float y, float maxw, float line_h, uint32_t col, char const *s);

/* controller glyphs: drawn with the left edge at x and the vertical centre at y; returns the width */
float ui_glyph(struct vkr *r, int id, float x, float y, float size);
/* glyph + label; returns the width */
float ui_hint(struct vkr *r, int glyph, float x, float y, char const *label, float size);
float ui_hint_w(struct vkr *r, int glyph, char const *label, float size);

/* shapes and icons */
void ui_heart(struct vkr *r, float cx, float cy, float size, bool filled, double pulse);
void ui_key_icon(struct vkr *r, float cx, float cy, float size, bool unlocked);
void ui_emblem(struct vkr *r, float x, float y, float k, double time);
void ui_item_icon(struct vkr *r, int shop_item, float cx, float cy, float size, uint32_t col);
void ui_spectrum_rule(struct vkr *r, float x, float y, float w, float h, float alpha);

/* panels and buttons */
void ui_panel(struct vkr *r, float x, float y, float w, float h, float radius, float alpha);
void ui_button(struct vkr *r, float x, float y, float w, float h, char const *label, bool selected, bool primary, float anim, double time);
void ui_button_c(struct vkr *r, float x, float y, float w, float h, char const *label, bool selected, bool primary, float anim, double time);
