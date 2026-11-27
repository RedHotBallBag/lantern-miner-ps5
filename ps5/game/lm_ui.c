/*
 * Lantern Miner: text, button glyphs, icons and panels.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lm_ui.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------- text */

float
ui_text_w(struct vkr *r, enum vkr_font f, float size, char const *s)
{
    return vkr_text_width(r, f, size, s);
}

void
ui_text(struct vkr *r, enum vkr_font f, float size, float x, float y, uint32_t col, char const *s)
{
    vkr_text(r, f, size, x, y, col, s);
}

void
ui_text_shadow(struct vkr *r, enum vkr_font f, float size, float x, float y, uint32_t col, char const *s)
{
    vkr_text(r, f, size, x + size * 0.04f, y + size * 0.06f, HEXA(0x050308, 0.7), s);
    vkr_text(r, f, size, x, y, col, s);
}

void
ui_text_c(struct vkr *r, enum vkr_font f, float size, float cx, float y, uint32_t col, char const *s)
{
    vkr_text(r, f, size, cx - vkr_text_width(r, f, size, s) * 0.5f, y, col, s);
}

void
ui_text_r(struct vkr *r, enum vkr_font f, float size, float rx, float y, uint32_t col, char const *s)
{
    vkr_text(r, f, size, rx - vkr_text_width(r, f, size, s), y, col, s);
}

void
ui_text_spaced(struct vkr *r, enum vkr_font f, float size, float x, float y, float spacing, uint32_t col, char const *s)
{
    char one[2] = {0, 0};
    for (; *s; s++) {
        one[0] = *s;
        vkr_text(r, f, size, x, y, col, one);
        x += vkr_text_width(r, f, size, one) + spacing;
    }
}

int
ui_paragraph(struct vkr *r, enum vkr_font f, float size, float x, float y, float maxw, float line_h, uint32_t col, char const *s)
{
    char line[200];
    int n = 0, lines = 0;
    char const *p = s;
    line[0] = 0;
    while (*p) {
        char word[80];
        int wl = 0;
        while (*p == ' ')
            p++;
        while (*p && *p != ' ' && wl < 78)
            word[wl++] = *p++;
        word[wl] = 0;
        if (!wl)
            break;
        char trial[260];
        snprintf(trial, sizeof(trial), "%s%s%s", line, n ? " " : "", word);
        if (n && vkr_text_width(r, f, size, trial) > maxw) {
            vkr_text(r, f, size, x, y + lines * line_h, col, line);
            lines++;
            snprintf(line, sizeof(line), "%s", word);
            n = 1;
        } else {
            snprintf(line, sizeof(line), "%s", trial);
            n++;
        }
    }
    if (n) {
        vkr_text(r, f, size, x, y + lines * line_h, col, line);
        lines++;
    }
    return lines;
}

/* ------------------------------------------------------------ controller glyphs */

static void
face_base(struct vkr *r, float x, float y, float size, uint32_t accent)
{
    vkr_circle(r, x + size / 2, y, size / 2, HEXA(0x0c0a10, 0.92));
    vkr_circle_ring(r, x + size / 2, y, size / 2 - 1, 2.0f, accent);
}

float
ui_glyph(struct vkr *r, int id, float x, float y, float size)
{
    float cx = x + size / 2;
    switch (id) {
    case G_CROSS: {
        uint32_t c = HEX(0x7aa7ff);
        float d = size * 0.2f;
        face_base(r, x, y, size, c);
        vkr_line(r, cx - d, y - d, cx + d, y + d, size * 0.09f, c);
        vkr_line(r, cx + d, y - d, cx - d, y + d, size * 0.09f, c);
        return size;
    }
    case G_CIRCLE: {
        uint32_t c = HEX(0xff6f7d);
        face_base(r, x, y, size, c);
        vkr_circle_ring(r, cx, y, size * 0.2f, size * 0.09f, c);
        return size;
    }
    case G_SQUARE: {
        uint32_t c = HEX(0xf595d6);
        face_base(r, x, y, size, c);
        vkr_ring(r, cx - size * 0.19f, y - size * 0.19f, size * 0.38f, size * 0.38f, 1.0f, size * 0.09f, c);
        return size;
    }
    case G_TRIANGLE: {
        uint32_t c = HEX(0x5ee3a5);
        float d = size * 0.22f, t = size * 0.09f;
        face_base(r, x, y, size, c);
        vkr_line(r, cx, y - d * 1.05f, cx + d, y + d * 0.75f, t, c);
        vkr_line(r, cx + d, y + d * 0.75f, cx - d, y + d * 0.75f, t, c);
        vkr_line(r, cx - d, y + d * 0.75f, cx, y - d * 1.05f, t, c);
        return size;
    }
    case G_DPAD:
    case G_DPAD_LR:
    case G_DPAD_UD: {
        float a = size * 0.36f, b = size * 0.98f, arm = (b - a) / 2;
        uint32_t base = id == G_DPAD ? HEX(C_CREAM) : HEXA(C_CREAM, 0.45);
        uint32_t hi = HEX(C_AMBER);
        vkr_rrect(r, cx - b / 2, y - a / 2, b, a, a * 0.3f, base);
        vkr_rrect(r, cx - a / 2, y - b / 2, a, b, a * 0.3f, base);
        if (id == G_DPAD_LR) {
            vkr_rrect(r, cx - b / 2, y - a / 2, arm + a * 0.3f, a, a * 0.3f, hi);
            vkr_rrect(r, cx + b / 2 - arm - a * 0.3f, y - a / 2, arm + a * 0.3f, a, a * 0.3f, hi);
        } else if (id == G_DPAD_UD) {
            vkr_rrect(r, cx - a / 2, y - b / 2, a, arm + a * 0.3f, a * 0.3f, hi);
            vkr_rrect(r, cx - a / 2, y + b / 2 - arm - a * 0.3f, a, arm + a * 0.3f, a * 0.3f, hi);
        }
        vkr_circle(r, cx, y, a * 0.16f, HEXA(0x0c0a10, 0.6));
        return size;
    }
    case G_OPTIONS: {
        float w = size * 1.5f, h = size * 0.74f;
        vkr_rrect(r, x, y - h / 2, w, h, h / 2, HEXA(0x0c0a10, 0.92));
        vkr_ring(r, x, y - h / 2, w, h, h / 2, 2.0f, HEX(C_MUTED));
        for (int i = -1; i <= 1; i++)
            vkr_rect(r, x + w * 0.28f, y + i * h * 0.2f - 1.0f, w * 0.44f, 2.0f, HEX(C_CREAM));
        return w;
    }
    case G_L1:
    case G_R1: {
        float w = size * 1.4f, h = size * 0.74f;
        vkr_rrect(r, x, y - h / 2, w, h, h * 0.42f, HEXA(0x0c0a10, 0.92));
        vkr_ring(r, x, y - h / 2, w, h, h * 0.42f, 2.0f, HEX(C_MUTED));
        ui_text_c(r, VKR_SANS_BOLD, size * 0.5f, x + w / 2, y - size * 0.3f, HEX(C_CREAM), id == G_L1 ? "L1" : "R1");
        return w;
    }
    case G_R3: {
        float w = size * 1.1f;
        vkr_circle(r, x + w / 2, y, w / 2, HEXA(0x0c0a10, 0.92));
        vkr_circle_ring(r, x + w / 2, y, w / 2 - 1, 2.0f, HEX(C_MUTED));
        ui_text_c(r, VKR_SANS_BOLD, size * 0.42f, x + w / 2, y - size * 0.26f, HEX(C_CREAM), "R3");
        return w;
    }
    case G_STICK:
    default: {
        vkr_circle(r, cx, y, size / 2, HEXA(0x0c0a10, 0.92));
        vkr_circle_ring(r, cx, y, size / 2 - 1, 2.0f, HEX(C_MUTED));
        vkr_circle(r, cx, y, size * 0.2f, HEX(C_CREAM));
        return size;
    }
    }
}

float
ui_hint_w(struct vkr *r, int glyph, char const *label, float size)
{
    float gw = glyph == G_OPTIONS ? size * 1.5f : (glyph == G_L1 || glyph == G_R1) ? size * 1.4f : glyph == G_R3 ? size * 1.1f : size;
    return gw + 12 + vkr_text_width(r, VKR_SANS, size * 0.82f, label);
}

float
ui_hint(struct vkr *r, int glyph, float x, float y, char const *label, float size)
{
    float gw = ui_glyph(r, glyph, x, y, size);
    ui_text(r, VKR_SANS, size * 0.82f, x + gw + 12, y - size * 0.5f, HEX(C_CREAM), label);
    return gw + 12 + vkr_text_width(r, VKR_SANS, size * 0.82f, label);
}

/* ------------------------------------------------------------------ icons */

void
ui_heart(struct vkr *r, float cx, float cy, float s, bool filled, double pulse)
{
    float k = s * (1.0f + (float)pulse * 0.08f);
    uint32_t fill = filled ? HEX(C_PINK) : HEXA(0x2a2331, 1.0);
    uint32_t rim = filled ? HEX(0xff8ad0) : HEX(0x5b4f63);
    /* outline pass slightly larger, then the body */
    for (int pass = 0; pass < 2; pass++) {
        float e = pass == 0 ? k * 0.09f : 0.0f;
        uint32_t c = pass == 0 ? rim : fill;
        float rr = k * 0.27f + e;
        vkr_circle(r, cx - k * 0.23f, cy - k * 0.12f, rr, c);
        vkr_circle(r, cx + k * 0.23f, cy - k * 0.12f, rr, c);
        vkr_tri(r, cx - k * 0.5f - e, cy - k * 0.04f, cx + k * 0.5f + e, cy - k * 0.04f, cx, cy + k * 0.52f + e * 1.5f, c);
    }
}

void
ui_key_icon(struct vkr *r, float cx, float cy, float s, bool unlocked)
{
    uint32_t a = unlocked ? HEX(C_GREEN) : HEX(C_PINK);
    uint32_t b = unlocked ? HEX(C_GREEN) : HEX(C_YELLOW);
    vkr_circle_ring(r, cx - s * 0.18f, cy - s * 0.18f, s * 0.26f, s * 0.13f, a);
    vkr_line(r, cx, cy, cx + s * 0.42f, cy + s * 0.42f, s * 0.13f, b);
    vkr_line(r, cx + s * 0.26f, cy + s * 0.26f, cx + s * 0.42f, cy + s * 0.1f, s * 0.13f, b);
}

static void
bez(float *x, float *y, float const *p, float t)
{
    float u = 1 - t;
    *x = u * u * u * p[0] + 3 * u * u * t * p[2] + 3 * u * t * t * p[4] + t * t * t * p[6];
    *y = u * u * u * p[1] + 3 * u * u * t * p[3] + 3 * u * t * t * p[5] + t * t * t * p[7];
}

/* The brass-lantern emblem from the title lockup (SVG 80x80 box), top-left at (x,y), k virtual px per SVG unit. */
void
ui_emblem(struct vkr *r, float x, float y, float k, double time)
{
#define X(v) (x + (v) * k)
#define Y(v) (y + (v) * k)
    float flick = 1.0f + 0.04f * (float)lm_sin(time * 9.0) + 0.02f * (float)lm_sin(time * 23.0);
    vkr_blend(r, true);
    vkr_glow(r, X(40), Y(54), 46 * k * 1.25f, HEXA(0xffe347, 0.55 * flick), 0.41f);
    vkr_blend(r, false);

    /* bail handle: M28 24 C28 11, 52 11, 52 24 */
    {
        float p[8] = {28, 24, 28, 11, 52, 11, 52, 24};
        float px = 0, py = 0, qx, qy;
        for (int i = 0; i <= 12; i++) {
            bez(&qx, &qy, p, i / 12.0f);
            if (i)
                vkr_line(r, X(px), Y(py), X(qx), Y(qy), 3.5f * k, HEX(0xc98e32));
            px = qx;
            py = qy;
        }
    }
    vkr_rrect(r, X(25), Y(23), 30 * k, 6 * k, 2 * k, HEX(0xb77b29));
    vkr_rrect(r, X(29), Y(19), 22 * k, 5 * k, 2 * k, HEX(0xe5a94c));
    vkr_rrect(r, X(26), Y(29), 28 * k, 38 * k, 4 * k, HEX(0x251c20));
    vkr_ring(r, X(26), Y(29), 28 * k, 38 * k, 4 * k, 3 * k, HEX(0xc98e32));

    /* flame: two cubic lobes, fanned from its centre */
    {
        float fl = 1.0f + 0.06f * (float)lm_sin(time * 7.0);
        float pts[64];
        int n = 0;
        pts[n++] = X(40);
        pts[n++] = Y(49);
        float a[8] = {40, 36, 46, 44, 48, 53, 40, 60};
        float b[8] = {40, 60, 32, 53, 34, 44, 40, 36};
        float qx, qy;
        for (int i = 0; i <= 10; i++) {
            bez(&qx, &qy, a, i / 10.0f);
            pts[n++] = X(qx);
            pts[n++] = Y(48 + (qy - 48) * fl);
        }
        for (int i = 1; i <= 10; i++) {
            bez(&qx, &qy, b, i / 10.0f);
            pts[n++] = X(qx);
            pts[n++] = Y(48 + (qy - 48) * fl);
        }
        vkr_poly(r, pts, n / 2, HEX(0xf5a623));
        float c1[8] = {40, 43, 43, 48, 44, 53, 40, 57};
        float c2[8] = {40, 57, 36, 53, 37, 48, 40, 43};
        n = 0;
        pts[n++] = X(40);
        pts[n++] = Y(50);
        for (int i = 0; i <= 8; i++) {
            bez(&qx, &qy, c1, i / 8.0f);
            pts[n++] = X(qx);
            pts[n++] = Y(51 + (qy - 51) * fl);
        }
        for (int i = 1; i <= 8; i++) {
            bez(&qx, &qy, c2, i / 8.0f);
            pts[n++] = X(qx);
            pts[n++] = Y(51 + (qy - 51) * fl);
        }
        vkr_poly(r, pts, n / 2, HEX(0xfff6d1));
    }
    vkr_line(r, X(33), Y(29), X(33), Y(67), 2 * k, HEX(0x9d6822));
    vkr_line(r, X(47), Y(29), X(47), Y(67), 2 * k, HEX(0x9d6822));
    vkr_rrect(r, X(22), Y(67), 36 * k, 8 * k, 2.5f * k, HEX(0xb77b29));
    vkr_rrect(r, X(26), Y(75), 28 * k, 4 * k, 1.5f * k, HEX(0x7c4f1d));

    float t = (float)time;
    /* spectrum crystals */
    vkr_poly(r, (float[]){X(10), Y(36), X(14), Y(41), X(10), Y(46), X(6), Y(41)}, 4, HEXA(0x22e4f5, 0.75 + 0.25 * lm_sin(t * 2.0)));
    vkr_poly(r, (float[]){X(70), Y(28), X(73), Y(32), X(70), Y(36), X(67), Y(32)}, 4, HEXA(0xf43fa5, 0.75 + 0.25 * lm_sin(t * 2.6 + 1)));
    vkr_poly(r, (float[]){X(69), Y(62), X(72), Y(65), X(69), Y(68), X(66), Y(65)}, 4, HEXA(0x2ce87d, 0.75 + 0.25 * lm_sin(t * 2.3 + 2)));
#undef X
#undef Y
}

void
ui_spectrum_rule(struct vkr *r, float x, float y, float w, float h, float alpha)
{
    static uint32_t const stops[4] = {0xf5a623, 0xf43fa5, 0x22e4f5, 0x2ce87d};
    static float const at[4] = {0.0f, 0.35f, 0.70f, 1.0f};
    for (int i = 0; i < 3; i++)
        vkr_rect_gradh(r, x + w * at[i], y, w * (at[i + 1] - at[i]), h, HEXA(stops[i], alpha), HEXA(stops[i + 1], alpha));
}

void
ui_item_icon(struct vkr *r, int item, float cx, float cy, float s, uint32_t col)
{
    uint32_t c = HEX(col);
    float u = s / 24.0f;
    switch (item) {
    case SHOP_KETTLE_BREW: /* mug */
        vkr_rrect(r, cx - 8 * u, cy - 6 * u, 14 * u, 14 * u, 2 * u, c);
        vkr_ring(r, cx + 3 * u, cy - 3 * u, 9 * u, 8 * u, 3 * u, 2.2f * u, c);
        vkr_rect(r, cx - 5 * u, cy - 11 * u, 2 * u, 4 * u, HEXA(C_CREAM, 0.7));
        vkr_rect(r, cx - 1 * u, cy - 12 * u, 2 * u, 4 * u, HEXA(C_CREAM, 0.7));
        break;
    case SHOP_LANTERN_WARD: /* shield */
        vkr_poly(r, (float[]){cx, cy - 11 * u, cx + 9 * u, cy - 6 * u, cx + 7 * u, cy + 5 * u, cx, cy + 11 * u, cx - 7 * u, cy + 5 * u, cx - 9 * u, cy - 6 * u}, 6, c);
        vkr_circle(r, cx, cy - 1 * u, 3 * u, HEX(C_PANEL));
        break;
    case SHOP_CRUMBLE_LIGHTFOOT: /* sparkle */
        vkr_poly(r, (float[]){cx, cy - 11 * u, cx + 3 * u, cy - 3 * u, cx + 11 * u, cy, cx + 3 * u, cy + 3 * u, cx, cy + 11 * u, cx - 3 * u, cy + 3 * u, cx - 11 * u, cy, cx - 3 * u, cy - 3 * u}, 8, c);
        break;
    case SHOP_CONVEYOR_CLEATS: /* boot with cleats */
        vkr_rrect(r, cx - 7 * u, cy - 11 * u, 9 * u, 15 * u, 2 * u, c);
        vkr_rrect(r, cx - 7 * u, cy - 2 * u, 18 * u, 9 * u, 4 * u, c);
        vkr_rect(r, cx - 4 * u, cy - 9 * u, 3 * u, 1.5f * u, HEX(C_PANEL));
        vkr_rect(r, cx - 4 * u, cy - 6 * u, 3 * u, 1.5f * u, HEX(C_PANEL));
        for (int i = 0; i < 4; i++)
            vkr_rect(r, cx - 6 * u + i * 4.5f * u, cy + 7 * u, 2.5f * u, 3 * u, c);
        break;
    case SHOP_BRASS_LANTERN: /* flame */
        vkr_poly(r, (float[]){cx, cy - 12 * u, cx + 7 * u, cy - 1 * u, cx + 6 * u, cy + 7 * u, cx, cy + 11 * u, cx - 6 * u, cy + 7 * u, cx - 7 * u, cy - 1 * u}, 6, c);
        vkr_circle(r, cx, cy + 4 * u, 3 * u, HEX(0xfff6d1));
        break;
    case SHOP_GEM_RESONANCE: /* magnet */
        vkr_rect(r, cx - 9 * u, cy - 9 * u, 5 * u, 14 * u, c);
        vkr_rect(r, cx + 4 * u, cy - 9 * u, 5 * u, 14 * u, c);
        vkr_rect(r, cx - 9 * u, cy + 3 * u, 18 * u, 6 * u, c);
        vkr_rect(r, cx - 9 * u, cy - 9 * u, 5 * u, 4 * u, HEX(C_PINK));
        vkr_rect(r, cx + 4 * u, cy - 9 * u, 5 * u, 4 * u, HEX(C_PINK));
        break;
    case SHOP_FEATHERSTEP: /* feather */
        vkr_poly(r, (float[]){cx - 9 * u, cy + 10 * u, cx - 6 * u, cy - 3 * u, cx + 4 * u, cy - 11 * u, cx + 10 * u, cy - 10 * u, cx + 6 * u, cy + 1 * u, cx - 3 * u, cy + 8 * u}, 6, c);
        vkr_line(r, cx - 9 * u, cy + 11 * u, cx + 6 * u, cy - 8 * u, 1.6f * u, HEX(C_PANEL));
        break;
    case SHOP_EXTRA_HEARTH: /* heart */
        ui_heart(r, cx, cy, s * 0.95f, true, 0);
        break;
    case SHOP_SPRING_BOOTS: /* coil spring under a boot */
        vkr_rrect(r, cx - 8 * u, cy - 11 * u, 16 * u, 7 * u, 3 * u, c);
        for (int i = 0; i < 3; i++)
            vkr_line(r, cx - 7 * u, cy - 2 * u + i * 4.5f * u, cx + 7 * u, cy + 0.5f * u + i * 4.5f * u, 2.2f * u, c);
        vkr_rect(r, cx - 9 * u, cy + 11 * u, 18 * u, 2.5f * u, c);
        break;
    case SHOP_LANTERN_SHIELD: /* golden shield with a flame */
        vkr_poly(r, (float[]){cx, cy - 12 * u, cx + 10 * u, cy - 7 * u, cx + 8 * u, cy + 5 * u, cx, cy + 12 * u, cx - 8 * u, cy + 5 * u, cx - 10 * u, cy - 7 * u}, 6, c);
        vkr_poly(r, (float[]){cx, cy - 7 * u, cx + 4 * u, cy, cx + 3 * u, cy + 5 * u, cx, cy + 7 * u, cx - 3 * u, cy + 5 * u, cx - 4 * u, cy}, 6, HEX(C_PANEL));
        break;
    case SHOP_TEA_THERMOS: /* flask */
        vkr_rrect(r, cx - 6 * u, cy - 8 * u, 12 * u, 20 * u, 3 * u, c);
        vkr_rrect(r, cx - 5 * u, cy - 12 * u, 10 * u, 5 * u, 2 * u, HEX(C_CREAM));
        vkr_rect(r, cx - 6 * u, cy - 1 * u, 12 * u, 2.5f * u, HEX(C_PANEL));
        break;
    case SHOP_STARLIGHT_MAGNET: /* horseshoe magnet */
        vkr_rect(r, cx - 9 * u, cy - 9 * u, 5 * u, 14 * u, c);
        vkr_rect(r, cx + 4 * u, cy - 9 * u, 5 * u, 14 * u, c);
        vkr_rect(r, cx - 9 * u, cy + 3 * u, 18 * u, 6 * u, c);
        vkr_rect(r, cx - 9 * u, cy - 9 * u, 5 * u, 4 * u, HEX(C_CREAM));
        vkr_rect(r, cx + 4 * u, cy - 9 * u, 5 * u, 4 * u, HEX(C_CREAM));
        break;
    case SHOP_CLOCKWORK_SLOW: /* hourglass */
        vkr_poly(r, (float[]){cx - 8 * u, cy - 11 * u, cx + 8 * u, cy - 11 * u, cx + 1.5f * u, cy, cx + 8 * u, cy + 11 * u, cx - 8 * u, cy + 11 * u, cx - 1.5f * u, cy}, 6, c);
        vkr_rect(r, cx - 9 * u, cy - 13 * u, 18 * u, 2.5f * u, HEX(C_CREAM));
        vkr_rect(r, cx - 9 * u, cy + 10.5f * u, 18 * u, 2.5f * u, HEX(C_CREAM));
        break;
    case SHOP_CONTINUE: /* a ring with an arrow head: go again */
        vkr_circle_ring(r, cx, cy, s * 0.42f, s * 0.13f, HEX(col));
        vkr_tri(r, cx + s * 0.18f, cy - s * 0.46f, cx + s * 0.62f, cy - s * 0.30f, cx + s * 0.22f, cy - s * 0.06f, HEX(col));
        vkr_circle(r, cx, cy, s * 0.13f, HEX(0xffe347));
        break;
    case SHOP_EXTRA_LIFE: /* lantern */
        vkr_rrect(r, cx - 6 * u, cy - 6 * u, 12 * u, 16 * u, 3 * u, c);
        vkr_ring(r, cx, cy - 9 * u, 7 * u, 6 * u, 3 * u, 1.8f * u, c);
        vkr_circle(r, cx, cy + 2 * u, 3.5f * u, HEX(0xfff6d1));
        break;
    case SHOP_CHRONOMETER: /* clock */
    default:
        vkr_circle_ring(r, cx, cy, 10 * u, 2.4f * u, c);
        vkr_line(r, cx, cy, cx, cy - 6 * u, 2 * u, c);
        vkr_line(r, cx, cy, cx + 5 * u, cy + 2 * u, 2 * u, c);
        vkr_rect(r, cx - 2 * u, cy - 14 * u, 4 * u, 3 * u, c);
        break;
    }
}

/* ------------------------------------------------------------ panels/buttons */

void
ui_panel(struct vkr *r, float x, float y, float w, float h, float radius, float alpha)
{
    vkr_shadow(r, x, y + 12, w, h, radius, 36, HEXA(0x000000, 0.55));
    vkr_rrect(r, x, y, w, h, radius, HEXA(C_PANEL, alpha));
    vkr_ring(r, x, y, w, h, radius, 1.5f, HEXA(C_CREAM, 0.14));
}

static void
button_impl(struct vkr *r, float x, float y, float w, float h, char const *label, bool selected, bool primary, float anim, double time, bool center)
{
    float dx = anim * 14.0f;
    float rad = h * 0.22f;
    x += dx;
    if (selected) {
        float pulse = 0.5f + 0.5f * (float)lm_sin(time * 4.0);
        vkr_shadow(r, x, y + 6, w, h, rad, 30, HEXA(C_AMBER, 0.28 + 0.1 * pulse));
        vkr_rrect_grad(r, x, y, w, h, rad, HEX(C_AMBER_HI), HEX(0xe8941a));
        vkr_ring(r, x, y, w, h, rad, 2.0f, HEXA(0xffffff, 0.35));
        ui_text(r, VKR_SANS_BOLD, h * 0.46f, center ? x + (w - ui_text_w(r, VKR_SANS_BOLD, h * 0.46f, label)) * 0.5f : x + h * 0.55f, y + h * 0.5f - h * 0.275f, HEX(C_INK), label);
    } else {
        vkr_rrect(r, x, y, w, h, rad, HEXA(C_PANEL, 0.86));
        vkr_ring(r, x, y, w, h, rad, 1.5f, primary ? HEXA(C_AMBER, 0.55) : HEXA(C_CREAM, 0.16));
        ui_text(r, VKR_SANS_BOLD, h * 0.46f, center ? x + (w - ui_text_w(r, VKR_SANS_BOLD, h * 0.46f, label)) * 0.5f : x + h * 0.55f, y + h * 0.5f - h * 0.275f, primary ? HEX(C_AMBER) : HEX(C_CREAM), label);
    }
}

void
ui_button(struct vkr *r, float x, float y, float w, float h, char const *label, bool selected, bool primary, float anim, double time)
{
    button_impl(r, x, y, w, h, label, selected, primary, anim, time, false);
}

void
ui_button_c(struct vkr *r, float x, float y, float w, float h, char const *label, bool selected, bool primary, float anim, double time)
{
    button_impl(r, x, y, w, h, label, selected, primary, anim, time, true);
}
