/*
 * Lantern Miner: draws the cavern. The 960x540 world is shown at `scale` screen units per
 * world pixel (2.0 fills the screen).
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lm.h"
#include "../gfx/vkr.h"

#include <stdio.h>
#include <string.h>

#define RGB(h) VKR_RGB(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255)
#define RGBA(h, a) VKR_RGBA(((h) >> 16) & 255, ((h) >> 8) & 255, (h) & 255, (unsigned)(lm_clamp((a), 0.0, 1.0) * 255.0 + 0.5))

struct wc {
    struct vkr *r;
    float ox, oy, s;
};

static void
fr(struct wc const *c, double x, double y, double w, double h, uint32_t col)
{
    vkr_rect(c->r, c->ox + (float)x * c->s, c->oy + (float)y * c->s, (float)w * c->s, (float)h * c->s, col);
}

/* rectangle clipped to a clip box (world coordinates) */
static void
frc(struct wc const *c, double x, double y, double w, double h, uint32_t col, double cx0, double cy0, double cx1, double cy1)
{
    double x1 = x + w, y1 = y + h;
    if (x < cx0) x = cx0;
    if (y < cy0) y = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;
    if (x1 > x && y1 > y)
        fr(c, x, y, x1 - x, y1 - y, col);
}

static void
ln(struct wc const *c, double x0, double y0, double x1, double y1, double w, uint32_t col)
{
    vkr_line(c->r, c->ox + (float)x0 * c->s, c->oy + (float)y0 * c->s, c->ox + (float)x1 * c->s, c->oy + (float)y1 * c->s,
             (float)w * c->s, col);
}

static void
tri(struct wc const *c, double x0, double y0, double x1, double y1, double x2, double y2, uint32_t col)
{
    vkr_tri(c->r, c->ox + (float)x0 * c->s, c->oy + (float)y0 * c->s, c->ox + (float)x1 * c->s, c->oy + (float)y1 * c->s,
            c->ox + (float)x2 * c->s, c->oy + (float)y2 * c->s, col);
}

static void
disc(struct wc const *c, double x, double y, double rad, uint32_t col)
{
    vkr_circle(c->r, c->ox + (float)x * c->s, c->oy + (float)y * c->s, (float)rad * c->s, col);
}

static void
ring(struct wc const *c, double x, double y, double rad, double thick, uint32_t col)
{
    vkr_circle_ring(c->r, c->ox + (float)x * c->s, c->oy + (float)y * c->s, (float)rad * c->s, (float)thick * c->s, col);
}

/* outline centred on the edge */
static void
stroke_rect(struct wc const *c, double x, double y, double w, double h, double lw, uint32_t col)
{
    double o = lw / 2;
    fr(c, x - o, y - o, w + lw, lw, col);
    fr(c, x - o, y + h - o, w + lw, lw, col);
    fr(c, x - o, y + o, lw, h - lw, col);
    fr(c, x + w - o, y + o, lw, h - lw, col);
}

static void
glow(struct wc const *c, double x, double y, double rad, uint32_t col, double mid)
{
    vkr_glow(c->r, c->ox + (float)x * c->s, c->oy + (float)y * c->s, (float)rad * c->s, col, (float)mid);
}

/* convex/star-shaped polygon from world points */
static void
poly(struct wc const *c, double const *pts, int n, uint32_t col)
{
    float xy[48];
    for (int i = 0; i < n && i < 24; i++) {
        xy[i * 2] = c->ox + (float)pts[i * 2] * c->s;
        xy[i * 2 + 1] = c->oy + (float)pts[i * 2 + 1] * c->s;
    }
    vkr_poly(c->r, xy, n, col);
}

/* --------------------------------------------------------------- backdrop */

static void
draw_background(struct wc const *c, struct lm_cavern const *cv, double time)
{
    struct lm_biome const *b = &lm_biomes[cv->biome];
    uint32_t grid = RGBA(0xfff0d7, 0.045);

    for (int r = 1; r < GRID_ROWS - 1; r++) {
        double y = r * TILE_SIZE;
        fr(c, TILE_SIZE, y - 0.5, CANVAS_W - TILE_SIZE * 2, 1, grid);
        int offset = (r % 2) * 24;
        for (int x = TILE_SIZE + offset; x < CANVAS_W - TILE_SIZE; x += 48)
            fr(c, x - 0.5, y, 1, TILE_SIZE, grid);
    }

    static int const pillars[3] = {150, 465, 780};
    for (int i = 0; i < 3; i++) {
        double px = pillars[i];
        fr(c, px, TILE_SIZE, 16, CANVAS_H - TILE_SIZE * 2, RGBA(0x2a1b16, 0.55));
        fr(c, px + 2, TILE_SIZE, 3, CANVAS_H - TILE_SIZE * 2, RGBA(0x744b30, 0.25));
        ln(c, px + 8, TILE_SIZE + 44, px - 28, TILE_SIZE + 4, 6, RGBA(0x412a1f, 0.52));
        ln(c, px + 8, TILE_SIZE + 44, px + 44, TILE_SIZE + 4, 6, RGBA(0x412a1f, 0.52));
    }

    if (cv->biome == BIOME_FLOODED) {
        /* light from somewhere above, old drain pipes, and a roof that drips */
        for (int i = 0; i < 4; i++) {
            double sx = 110 + i * 235 + lm_sin(time * 0.4 + i * 1.9) * 14;
            tri(c, sx, TILE_SIZE, sx + 26, TILE_SIZE, sx - 74, CANVAS_H - TILE_SIZE, RGBA(0x9fe8ff, 0.028));
            tri(c, sx + 26, TILE_SIZE, sx - 20, CANVAS_H - TILE_SIZE, sx - 74, CANVAS_H - TILE_SIZE, RGBA(0x9fe8ff, 0.028));
        }
        static int const pipe_y[2] = {148, 352};
        for (int i = 0; i < 2; i++) {
            double py = pipe_y[i] + (cv->depth * 31 + i * 17) % 40;
            fr(c, TILE_SIZE, py, CANVAS_W - TILE_SIZE * 2, 12, RGBA(0x14333a, 0.8));
            fr(c, TILE_SIZE, py + 2, CANVAS_W - TILE_SIZE * 2, 2, RGBA(0x6fb5bd, 0.22));
            for (int x = 90 + (cv->depth * 53 + i * 120) % 140; x < CANVAS_W - 60; x += 215) {
                fr(c, x, py - 3, 10, 18, RGBA(0x1d4a52, 0.85));
                fr(c, x + 2, py - 3, 2, 18, RGBA(0x8fd6d0, 0.18));
            }
        }
        for (int i = 0; i < 7; i++) {
            double dx = 70 + lm_fmod(i * 149 + cv->depth * 67, CANVAS_W - 140);
            double k = lm_fmod(time * (0.45 + 0.06 * i) + i * 0.37, 1.0);
            fr(c, lm_round(dx), TILE_SIZE + k * k * 420, 2, 4 + k * 5, RGBA(0xaee9ff, 0.4 * (1 - k)));
        }
    }

    for (int i = 0; i < 14; i++) {
        double vx = 55 + lm_fmod(i * 197 + cv->depth * 43, CANVAS_W - 110);
        double vy = 55 + lm_fmod(i * 113 + cv->depth * 29, CANVAS_H - 110);
        double pulse = lm_max(0, 0.25 + 0.35 * lm_sin(time * 2.1 + i * 1.4));
        uint32_t col = RGBA(i % 2 == 0 ? b->accent_spec : 0xf43fa5, pulse);
        fr(c, vx, vy, 3, 3, col);
        fr(c, vx + 3, vy - 2, 2, 2, col);
    }
}

static void
draw_ambient_lanterns(struct wc const *c, struct lm_cavern const *cv, double time)
{
    for (int i = 0; i < cv->n_lanterns; i++) {
        struct lm_lantern const *l = &cv->lanterns[i];
        double sway = lm_sin(time * 1.4 + l->sway_phase) * 3.5;
        double lx = l->x + sway, ly = l->y;
        ln(c, l->x, TILE_SIZE, lx, ly - 8, 1.5, RGBA(0x82766c, 0.55));
        fr(c, lm_round(lx - 5), lm_round(ly - 8), 10, 3, RGB(0x8b5e34));
        fr(c, lm_round(lx - 4), lm_round(ly + 4), 8, 3, RGB(0x8b5e34));
        double flicker = 0.85 + 0.15 * lm_sin(time * 8.5 + l->sway_phase * 3);
        fr(c, lm_round(lx - 3), lm_round(ly - 5), 6, 9, RGBA(0xffc85f, flicker));
        fr(c, lm_round(lx - 1), lm_round(ly - 3), 2, 5, RGB(0xfff5d6));
    }
}

/* ------------------------------------------------------------------ tiles */

static void
draw_dashed_box(struct wc const *c, double x, double y, double w, double h, uint32_t col)
{
    for (double d = 0; d < w; d += 8) {
        double l = lm_min(4, w - d);
        fr(c, x + d, y - 0.5, l, 1, col);
        fr(c, x + d, y + h - 0.5, l, 1, col);
    }
    for (double d = 0; d < h; d += 8) {
        double l = lm_min(4, h - d);
        fr(c, x - 0.5, y + d, 1, l, col);
        fr(c, x + w - 0.5, y + d, 1, l, col);
    }
}

static void
draw_tiles(struct wc const *c, struct lm_game const *g, double time, bool slow)
{
    struct lm_cavern const *cv = &g->cavern;
    struct lm_biome const *b = &lm_biomes[cv->biome];
    double depth_boost = 1 + lm_min(0.6, (cv->depth - 1) * 0.04);
    double conveyor_speed = (slow ? 14 : 28) * depth_boost;

    for (int r = 0; r < GRID_ROWS; r++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int tile = cv->tiles[r][col];
            if (tile == T_EMPTY)
                continue;
            double x = col * TILE_SIZE, y = r * TILE_SIZE;

            if (tile == T_SOLID) {
                fr(c, x, y, TILE_SIZE, TILE_SIZE, RGB(b->wall_fill));
                /* a touch of per-stone variation so big walls read as stonework */
                int h = (col * 73 + r * 151 + col * r * 7) & 7;
                if (h < 3)
                    fr(c, x, y, TILE_SIZE, TILE_SIZE, RGBA(0xffffff, 0.012 * (h + 1)));
                else if (h == 7)
                    fr(c, x, y, TILE_SIZE, TILE_SIZE, RGBA(0x000000, 0.06));
                uint32_t sh = RGB(b->wall_shadow);
                fr(c, x, y + TILE_SIZE - 3, TILE_SIZE, 3, sh);
                fr(c, x, y + 14, TILE_SIZE, 2, sh);
                int seam = (r + col) % 2 == 0 ? 14 : 7;
                fr(c, x + seam, y, 2, 14, sh);
                fr(c, x + ((seam + 14) % 24), y + 14, 2, 16, sh);
                if (r > 0 && cv->tiles[r - 1][col] == T_EMPTY) {
                    fr(c, x, y, TILE_SIZE, 4, RGB(b->wall_highlight));
                    if ((col * 7 + r * 3) % 4 == 0) {
                        uint32_t moss = cv->biome == BIOME_FROST_LANTERN ? RGB(0x7ce8ff) : RGB(0x4fa86c);
                        fr(c, x + 4, y, 8, 3, moss);
                        fr(c, x + 16, y, 5, 2, moss);
                    }
                }
            } else if (tile == T_ONE_WAY) {
                fr(c, x, y, TILE_SIZE, 11, RGB(b->timber_body));
                fr(c, x, y, TILE_SIZE, 4, RGB(b->timber_top));
                fr(c, x, y + 11, TILE_SIZE, 2, RGB(0x241812));
                fr(c, x + 14, y + 2, 2, 9, RGB(0x3d2417));
                fr(c, x + 4, y + 5, 2, 2, RGB(0xe5a94c));
                fr(c, x + 24, y + 5, 2, 2, RGB(0xe5a94c));
            } else if (tile == T_CONVEYOR_LEFT || tile == T_CONVEYOR_RIGHT) {
                int dir = tile == T_CONVEYOR_RIGHT ? 1 : -1;
                fr(c, x, y, TILE_SIZE, 13, RGB(0x1e2532));
                fr(c, x, y, TILE_SIZE, 2, RGB(0x39465e));
                fr(c, x, y + 11, TILE_SIZE, 2, RGB(0x39465e));
                double shift = lm_fmod(lm_fmod(time * conveyor_speed * dir, 10) + 10, 10);
                uint32_t cy = RGB(0x22e4f5);
                double k0 = x + 1, k1 = y + 2, k2 = x + TILE_SIZE - 1, k3 = y + 11;
                for (int ox = -10; ox <= TILE_SIZE + 10; ox += 10) {
                    double cx = x + ox + shift;
                    if (dir == 1) {
                        frc(c, lm_round(cx), y + 3, 3, 2, cy, k0, k1, k2, k3);
                        frc(c, lm_round(cx + 2), y + 5, 3, 3, cy, k0, k1, k2, k3);
                        frc(c, lm_round(cx), y + 8, 3, 2, cy, k0, k1, k2, k3);
                    } else {
                        frc(c, lm_round(cx + 2), y + 3, 3, 2, cy, k0, k1, k2, k3);
                        frc(c, lm_round(cx), y + 5, 3, 3, cy, k0, k1, k2, k3);
                        frc(c, lm_round(cx + 2), y + 8, 3, 2, cy, k0, k1, k2, k3);
                    }
                }
            } else if (tile == T_CRUMBLE) {
                struct lm_crumble const *cs = &g->crumble[r][col];
                if (cs->state == CR_BROKEN) {
                    draw_dashed_box(c, x + 2, y + 2, TILE_SIZE - 4, 11, RGBA(0xe5b37c, 0.25));
                    continue;
                }
                bool shaking = cs->state == CR_SHAKING;
                double jx = shaking ? lm_sin(time * 58 + col) * 2.2 : 0;
                double jy = shaking ? lm_cos(time * 47 + r) * 1.1 : 0;
                double dx = lm_round(x + jx), dy = lm_round(y + jy);
                fr(c, dx, dy, TILE_SIZE, 13, RGB(0x8b6a4f));
                fr(c, dx + 1, dy, TILE_SIZE - 2, 3, RGB(0xbfa07e));
                fr(c, dx, dy + 11, TILE_SIZE, 2, RGB(0x473325));
                uint32_t crack = shaking ? RGB(0x1a1110) : RGBA(0x2a1c14, 0.7);
                double lw = shaking ? 2 : 1.2;
                ln(c, dx + 8, dy + 1, dx + 13, dy + 7, lw, crack);
                ln(c, dx + 13, dy + 7, dx + 10, dy + 12, lw, crack);
                ln(c, dx + 22, dy + 2, dx + 18, dy + 9, lw, crack);
            } else if (tile == T_PIT && cv->tide.on) {
                continue; /* the flood is drawn over the top, all in one piece */
            } else if (tile == T_PIT && cv->biome == BIOME_FLOODED) {
                for (int k = 0; k < TILE_SIZE; k += 6) {
                    double sx = x + k, wave = lm_sin(time * 2.6 + sx * 0.045) * 1.8 + lm_sin(time * 1.7 - sx * 0.021) * 1.1;
                    double top = y + WATER_LIP + wave;
                    fr(c, sx, top, 6, y + TILE_SIZE - top, RGBA(0x25c4ea, 0.34));
                    fr(c, sx, top, 6, 2, RGBA(0xd6f8ff, 0.92));
                    fr(c, sx, top + 2, 6, 3, RGBA(0x4fd8f5, 0.45));
                }
                double rise = lm_fmod(time * 9 + col * 11, 18);
                fr(c, x + 6 + (col * 7) % 17, y + TILE_SIZE - rise, 3, 3, RGBA(0xdff8ff, 0.55 * (1 - rise / 18)));
            } else if (tile == T_PIT) {
                /* a glowing pool far below, in the cavern's accent colour */
                double wave = lm_sin(time * 2.6 + col * 0.9) * 2.5;
                fr(c, x, y + 9 + wave, TILE_SIZE, TILE_SIZE - 9 - wave, RGBA(b->accent_spec, 0.30));
                fr(c, x, y + 9 + wave, TILE_SIZE, 3, RGBA(b->accent_spec, 0.95));
                fr(c, x, y + 20, TILE_SIZE, 10, RGBA(0x000000, 0.35));
                double rise = lm_fmod(time * 9 + col * 11, 22);
                fr(c, x + 6 + (col * 7) % 17, y + TILE_SIZE - rise, 3, 3, RGBA(0xffffff, 0.55 * (1 - rise / 22)));
            } else if (tile == T_BRAMBLE) {
                double shimmer = 0.8 + 0.2 * lm_sin(time * 5 + col);
                fr(c, x + 2, y + TILE_SIZE - 6, TILE_SIZE - 4, 6, RGB(0x5e1a3a));
                static double const sp[3][4] = {{4, 8, 11, 8}, {11, 15, 6, 9}, {19, 23, 12, 7}};
                for (int i = 0; i < 3; i++) {
                    double bx = x + sp[i][0], tipx = x + sp[i][1], tipy = y + sp[i][2], w = sp[i][3];
                    tri(c, bx, y + TILE_SIZE - 3, tipx, tipy, bx + w, y + TILE_SIZE - 3, RGBA(0xf43fa5, shimmer));
                    fr(c, lm_round(tipx - 1), lm_round(tipy + 2), 2, 5, RGB(0xffd1ec));
                }
            }
        }
    }
}

/* ------------------------------------------------- ladders and platforms */

static void
draw_ladders(struct wc const *c, struct lm_cavern const *cv)
{
    for (int col = 0; col < GRID_COLS; col++) {
        for (int r = 0; r < GRID_ROWS; r++) {
            if (!cv->ladders[r][col])
                continue;
            bool top = r == 0 || !cv->ladders[r - 1][col];
            double x = col * TILE_SIZE, y = r * TILE_SIZE;
            double y0 = top ? y - 9 : y, h = y + TILE_SIZE - y0; /* the rails poke up above the ledge */
            fr(c, x + 5, y0, 4, h, RGB(0x6e4526));
            fr(c, x + 21, y0, 4, h, RGB(0x6e4526));
            fr(c, x + 5, y0, 1, h, RGB(0xd9a05b));
            fr(c, x + 21, y0, 1, h, RGB(0xd9a05b));
            for (int k = 4; k < TILE_SIZE; k += 8) {
                fr(c, x + 9, y + k, 12, 3, RGB(0xc08a4a));
                fr(c, x + 9, y + k + 3, 12, 1, RGB(0x2a1b12));
            }
        }
    }
}

static void
draw_movers(struct wc const *c, struct lm_game const *g, double time)
{
    for (int i = 0; i < g->n_movers; i++) {
        struct lm_mover const *m = &g->movers[i];
        double half = m->width * 0.5;
        if (m->raft) { /* lashed logs that shudder and bubble as they go under */
            double shake = m->sink > 0.35 ? lm_sin(time * 44 + i) * 1.6 * m->sink : 0;
            double x = lm_round(m->x + shake), y = lm_round(m->y + (m->sink > 0 ? 0 : lm_sin(time * 2.4 + i * 1.7) * 1.2));
            fr(c, x, y, m->width, MOVER_THICKNESS, RGB(0x6b4426));
            fr(c, x, y, m->width, 3, RGB(0xd29a58));
            fr(c, x, y + MOVER_THICKNESS - 3, m->width, 3, RGB(0x39230f));
            for (double k = 14; k < m->width - 6; k += 15)
                fr(c, x + k, y, 1, MOVER_THICKNESS, RGB(0x2c1a0c));
            for (int end = 0; end < 2; end++) {
                double ex = end ? x + m->width - 9 : x + 5;
                fr(c, ex, y - 1, 4, MOVER_THICKNESS + 2, RGB(0xe8d29a));
                fr(c, ex + 1, y - 1, 1, MOVER_THICKNESS + 2, RGB(0x8f7a48));
                disc(c, end ? x + m->width : x, y + 5, 4.5, RGB(0x9a6a3a));
                disc(c, end ? x + m->width : x, y + 5, 2, RGB(0x5b3a1e));
            }
            if (m->sink > 0.35)
                for (int k = 0; k < 6; k++) {
                    double rise = lm_fmod(time * 1.8 + k * 0.17, 1.0);
                    fr(c, lm_round(x + 3 + k * (m->width - 6) / 5.0), lm_round(y + 6 - rise * 16), 3, 3,
                       RGBA(0xdff8ff, (1 - rise) * m->sink));
                }
            continue;
        }
        /* a faint dotted track shows where it travels */
        if (m->vertical) {
            for (double y = m->min_y; y <= m->max_y + MOVER_THICKNESS; y += 9)
                fr(c, m->x + half - 1, y, 2, 3, RGBA(0xf5d076, 0.28));
        } else {
            for (double x = m->min_x; x <= m->max_x + m->width; x += 9)
                fr(c, x, m->y + MOVER_THICKNESS * 0.5 - 1, 3, 2, RGBA(0xf5d076, 0.28));
        }
        double x = lm_round(m->x), y = lm_round(m->y);
        fr(c, x, y, m->width, MOVER_THICKNESS, RGB(0x8a6a2f));
        fr(c, x, y, m->width, 3, RGB(0xf5d076));
        fr(c, x, y + MOVER_THICKNESS - 2, m->width, 2, RGB(0x3a2a12));
        for (double k = 5; k < m->width - 4; k += 16)
            fr(c, x + k, y + 5, 2, 2, RGB(0x2a1c0c));
        double blink = 0.55 + 0.45 * lm_sin(time * 6 + i * 2);
        fr(c, x + 1, y + 3, 3, 3, RGBA(0x22e4f5, blink));
        fr(c, x + m->width - 4, y + 3, 3, 3, RGBA(0x22e4f5, blink));
    }
}

/* ------------------------------------------------------- portal, items, foes */

static void
draw_portal(struct wc const *c, struct lm_game const *g, double time)
{
    struct lm_cavern const *cv = &g->cavern;
    double px = cv->portal.x, py = cv->portal.y;
    double w = TILE_SIZE * 2, h = TILE_SIZE * 2;
    bool unlocked = g->portal_unlocked;

    fr(c, px + 2, py + 4, w - 4, h - 4, RGB(0x332a38));
    uint32_t frame = unlocked ? RGB(0xc9933c) : RGB(0x6c5a52);
    fr(c, px + 4, py + 6, w - 8, 5, frame);
    fr(c, px + 4, py + 6, 5, h - 6, frame);
    fr(c, px + w - 9, py + 6, 5, h - 6, frame);

    double ix = px + 9, iy = py + 11, iw = w - 18, ih = h - 11;
    if (unlocked) {
        double pulse = 0.78 + 0.22 * lm_sin(time * 5.2);
        /* three-stop vertical gradient */
        fr(c, ix, iy, iw, ih * 0.45, RGB(0x2ce87d));
        vkr_rect_grad(c->r, c->ox + (float)ix * c->s, c->oy + (float)iy * c->s, (float)iw * c->s, (float)(ih * 0.45) * c->s, RGB(0xa8ffcb), RGB(0x2ce87d));
        vkr_rect_grad(c->r, c->ox + (float)ix * c->s, c->oy + (float)(iy + ih * 0.45) * c->s, (float)iw * c->s, (float)(ih * 0.55) * c->s, RGB(0x2ce87d), RGB(0x107a49));
        double band = iy + lm_fmod(time * 38, ih - 6);
        fr(c, ix + 2, lm_round(band), iw - 4, 4, RGBA(0xffffff, 0.35 * pulse));
        fr(c, px + w / 2 - 4, py + 1, 8, 5, RGB(0x2ce87d));
    } else {
        fr(c, ix, iy, iw, ih, RGB(0x1b1621));
        stroke_rect(c, ix + 3, iy + 3, iw - 6, ih - 6, 1.5, RGBA(0xf5a623, 0.28));
        int total = cv->keys_total > 1 ? cv->keys_total : 1;
        int spacing = (int)lm_min(7, lm_floor((w - 10) / total));
        double start = px + w / 2 - ((total - 1) * spacing) / 2.0;
        for (int i = 0; i < total; i++)
            fr(c, lm_round(start + i * spacing - 2), py + 2, 4, 4, i < g->keys_collected ? RGB(0xf43fa5) : RGB(0x4a3f52));
        fr(c, lm_round(px + w / 2 - 3), lm_round(py + h * 0.55), 6, 6, RGB(0x5c4b59));
        fr(c, lm_round(px + w / 2 - 1), lm_round(py + h * 0.55 + 5), 2, 6, RGB(0x5c4b59));
    }
}

static void
draw_chest(struct wc const *c, double x, double y, bool open, double time)
{
    double top = open ? y - 12 : y - 5;
    fr(c, x - 11, y - 1, 22, 12, RGB(0x7a4a22));
    fr(c, x - 11, y - 1, 22, 2, RGB(0x2a1a0e));
    fr(c, x - 11, top, 22, open ? 4 : 5, RGB(0x9a6230));
    fr(c, x - 11, top, 22, 2, RGB(0xd9a05b));
    fr(c, x - 8, y - 1, 2, 12, RGB(0xf5c542));
    fr(c, x + 6, y - 1, 2, 12, RGB(0xf5c542));
    if (open) {
        fr(c, x - 9, y - 1, 18, 3, RGB(0x140c06));
        return;
    }
    fr(c, x - 2, y - 2, 4, 6, RGB(0xffe347));
    double glint = lm_sin(time * 3.2);
    if (glint > 0.75)
        fr(c, x + 7, y - 11, 3, 3, RGB(0xffffff));
}

static void
draw_collectable(struct wc const *c, struct lm_collectable const *it, double time)
{
    double bob = lm_sin(time * 3.8 + it->phase) * 3.5;
    double x = lm_round(it->x), y = lm_round(it->y + bob);

    if (it->kind == COL_PLUG) { /* a great red bung on a chain, begging to be pulled */
        double py = lm_round(it->base_y), tug = lm_sin(time * 3) * 1.2;
        for (int k = 0; k < 5; k++)
            ring(c, x + (k % 2 ? 1 : -1), py - 12 - k * 5 + tug * (4 - k) * 0.2, 3, 1.4, RGB(0xc9b37a));
        ring(c, x, py - 8 + tug, 5, 2.5, RGB(0xffe347));
        poly(c, (double const[]){x - 11, py - 5, x + 11, py - 5, x + 8, py + 9, x - 8, py + 9}, 4, RGB(0xd23a4e));
        fr(c, x - 11, py - 5, 22, 3, RGB(0xff7a8c));
        fr(c, x - 8, py + 7, 16, 2, RGB(0x7c1c2c));
        fr(c, x - 6, py - 1, 3, 6, RGBA(0xffffff, 0.45));
        double blink = lm_sin(time * 5);
        if (blink > 0.4)
            fr(c, x + 11, py - 16, 3, 3, RGB(0xffffff));
    } else if (it->kind == COL_KEY) {
        bool flash = lm_sin(time * 8 + it->phase) > 0;
        uint32_t primary = flash ? RGB(0xf43fa5) : RGB(0xffe347);
        uint32_t rim = flash ? RGB(0xffd1ec) : RGB(0xfff8c4);
        fr(c, x - 6, y - 9, 12, 9, primary);
        fr(c, x - 3, y - 6, 6, 3, RGB(0x16121b));
        fr(c, x - 2, y, 4, 10, primary);
        fr(c, x + 2, y + 4, 4, 2, primary);
        fr(c, x + 2, y + 8, 5, 2, primary);
        fr(c, x - 5, y - 8, 3, 2, rim);
    } else if (it->kind == COL_RELIC) {
        if (it->relic_variant == 0) {
            fr(c, x - 6, y - 2, 11, 8, RGB(0xffe347));
            fr(c, x - 5, y + 5, 9, 2, RGB(0xf5a623));
            stroke_rect(c, x + 5, y - 1, 3, 5, 2, RGB(0xffe347));
            double steam = lm_sin(time * 5 + it->phase) * 1.5;
            fr(c, lm_round(x - 2 + steam), y - 7, 2, 3, RGBA(0xf6efe2, 0.7));
            fr(c, lm_round(x + 2 - steam), y - 9, 2, 3, RGBA(0xf6efe2, 0.7));
        } else {
            uint32_t col = it->relic_variant == 1 ? RGB(0x22e4f5) : RGB(0x2ce87d);
            double d[8] = {x, y - 8, x + 7, y, x, y + 8, x - 7, y};
            poly(c, d, 4, col);
            fr(c, x - 2, y - 4, 3, 3, RGB(0xffffff));
        }
    } else if (it->kind == COL_AIR) {
        /* a little corked bottle of air */
        fr(c, x - 3, y - 11, 6, 3, RGB(0xa76e42));
        fr(c, x - 2, y - 8, 4, 3, RGB(0xd8fbff));
        fr(c, x - 6, y - 5, 12, 14, RGB(0xd8fbff));
        fr(c, x - 5, y - 4, 10, 12, RGB(0x1b6f86));
        fr(c, x - 5, y - 1, 10, 9, RGB(0x22e4f5));
        fr(c, x - 4, y - 3, 2, 9, RGBA(0xffffff, 0.85));
        double rise = time * 1.6 + it->phase;
        rise -= lm_floor(rise);
        fr(c, x + 1, lm_round(y + 5 - rise * 8), 2, 2, RGB(0xffffff));
    } else {
        draw_chest(c, lm_round(it->x), lm_round(it->base_y), false, time + it->phase);
    }
}

static void
draw_enemy(struct wc const *c, struct lm_enemy const *e, double time, bool slow)
{
    double x = lm_round(e->s.x), y = lm_round(e->s.y), w = e->width, h = e->height;
    bool tele = e->telegraph > 0;
    double phase = e->s.phase;

    switch (e->s.kind) {
    case EN_BEETLE: {
        double bob = lm_fabs(lm_sin(time * 9 + phase)) * 1.5;
        double kw = 3 + lm_fabs(lm_sin(time * (slow ? 3 : 8))) * 5;
        fr(c, lm_round(x + w / 2 - kw / 2), lm_round(y - 4 - bob), lm_round(kw), 4, RGB(0xffe347));
        fr(c, lm_round(x + w / 2 - 1), lm_round(y - 1 - bob), 2, 3, RGB(0xffe347));
        fr(c, x + 2, lm_round(y + 3 - bob), w - 4, h - 7, tele ? RGB(0xff79c6) : RGB(0xc95c38));
        fr(c, x + 4, lm_round(y + 2 - bob), w - 8, 4, RGB(0xf5a623));
        double eye = e->s.direction == 1 ? x + w - 7 : x + 3;
        fr(c, eye, lm_round(y + 8 - bob), 4, 4, RGB(0x22e4f5));
        double leg = lm_sin(time * 16 + phase) > 0 ? 0 : 2;
        fr(c, x + 3 + leg, y + h - 4, 3, 4, RGB(0xe5a94c));
        fr(c, x + w - 7 - leg, y + h - 4, 3, 4, RGB(0xe5a94c));
        break;
    }
    case EN_CHARGER: {
        double jitter = tele ? lm_sin(time * 60) * 2 : 0;
        double dx = lm_round(x + jitter);
        if (e->is_charging)
            fr(c, dx - e->s.direction * 12, y + 4, 12, h - 8, RGBA(0xffe347, 0.35));
        fr(c, dx + 2, y + 3, w - 4, h - 6, tele ? RGB(0xffe347) : e->is_charging ? RGB(0xff4f8b) : RGB(0x8a235a));
        fr(c, dx + 4, y + 2, w - 8, 3, RGB(0xf5a623));
        if (e->s.direction == 1)
            tri(c, dx + w - 2, y + 5, dx + w + 7, y + h * 0.5, dx + w - 2, y + h - 5, RGB(0xffe347));
        else
            tri(c, dx + 2, y + 5, dx - 7, y + h * 0.5, dx + 2, y + h - 5, RGB(0xffe347));
        double eye = e->s.direction == 1 ? dx + w - 8 : dx + 4;
        fr(c, eye, y + 7, 4, 4, RGB(0xffffff));
        if (tele) {
            fr(c, dx + w / 2 - 1, y - 12, 3, 7, RGB(0xffe347));
            fr(c, dx + w / 2 - 1, y - 3, 3, 2, RGB(0xffe347));
        }
        break;
    }
    case EN_SPITTER: {
        double cg = tele ? 0.95 : 0.45 + 0.25 * lm_sin(time * 4 + phase);
        fr(c, x + 2, y + h - 6, w - 4, 6, RGB(0x8b5e34));
        fr(c, x + 4, y + h - 8, w - 8, 3, RGB(0xe5a94c));
        tri(c, x + w / 2, y, x + w - 3, y + h - 6, x + 3, y + h - 6, RGB(0x6c2d8c));
        double orb = tele ? 6 : 3.5;
        disc(c, x + w / 2, y + h * 0.45, orb, RGBA(0xf43fa5, cg));
        fr(c, lm_round(x + w / 2 - 1.5), lm_round(y + h * 0.45 - 1.5), 3, 3, RGB(0xffffff));
        break;
    }
    case EN_STALKER: {
        if (e->cooldown > 0 && (int)(time * 12) % 2)
            break; /* flickering into being */
        double bob = lm_sin(time * 5 + phase) * 2.5;
        double dy = lm_round(y + bob);
        disc(c, x + w / 2, dy + h / 2, 16, RGBA(0x22e4f5, 0.16));
        /* hooded cloak: half circle on top, ragged hem below */
        double pts[48];
        int n = 0;
        double cx = x + w / 2;
        pts[n * 2] = cx;
        pts[n * 2 + 1] = dy + 8;
        n++;
        for (int i = 0; i <= 8; i++) {
            double a = LM_PI + LM_PI * i / 8.0;
            pts[n * 2] = cx + lm_cos(a) * 9;
            pts[n * 2 + 1] = dy + 8 + lm_sin(a) * 9;
            n++;
        }
        double hem[5][2] = {{x + w - 2, dy + h - 1}, {x + w * 0.72, dy + h - 5}, {x + w * 0.5, dy + h}, {x + w * 0.28, dy + h - 5}, {x + 2, dy + h - 1}};
        for (int i = 0; i < 5; i++) {
            pts[n * 2] = hem[i][0];
            pts[n * 2 + 1] = hem[i][1];
            n++;
        }
        poly(c, pts, n, RGB(0x1b1d3a));
        uint32_t edge = RGB(0x22e4f5);
        for (int i = 1; i < n; i++) {
            int j = i + 1 < n ? i + 1 : 1;
            ln(c, pts[i * 2], pts[i * 2 + 1], pts[j * 2], pts[j * 2 + 1], 1.5, edge);
        }
        fr(c, x + 6, dy + 7, 3, 3, RGB(0xf43fa5));
        fr(c, x + w - 9, dy + 7, 3, 3, RGB(0xf43fa5));
        break;
    }
    case EN_BOUNCER: {
        double squat = tele ? 5 : 0;
        double dY = y + squat, dH = h - squat;
        ln(c, x + 6, dY + dH - 5, x + w - 6, dY + dH - 2, 2, RGB(0xffe347));
        ln(c, x + w - 6, dY + dH - 2, x + 6, y + h, 2, RGB(0xffe347));
        fr(c, x + 2, dY + 2, w - 4, lm_max(8, dH - 7), RGB(0xf6efe2));
        fr(c, x + 2, dY + 5, w - 4, 3, RGB(0xf43fa5));
        fr(c, x + 6, dY + 9, 3, 3, RGB(0x1b1722));
        fr(c, x + w - 9, dY + 9, 3, 3, RGB(0x1b1722));
        break;
    }
    case EN_GLOW_BAT: {
        bool up = lm_sin(time * (slow ? 6 : 14) + phase) > 0;
        fr(c, x + 6, y + 6, 10, 9, RGB(0x4e356b));
        if (up) {
            fr(c, x - 3, y + 1, 9, 6, RGB(0x7b4fa8));
            fr(c, x + 16, y + 1, 9, 6, RGB(0x7b4fa8));
            fr(c, x - 3, y + 1, 3, 3, RGB(0xf43fa5));
            fr(c, x + 22, y + 1, 3, 3, RGB(0xf43fa5));
        } else {
            fr(c, x - 3, y + 10, 9, 6, RGB(0x7b4fa8));
            fr(c, x + 16, y + 10, 9, 6, RGB(0x7b4fa8));
            fr(c, x - 3, y + 13, 3, 3, RGB(0xf43fa5));
            fr(c, x + 22, y + 13, 3, 3, RGB(0xf43fa5));
        }
        fr(c, x + 8, y + 8, 2, 2, RGB(0xffe347));
        fr(c, x + 12, y + 8, 2, 2, RGB(0xffe347));
        break;
    }
    case EN_PISTON:
        fr(c, x + 3, y + 2, w - 6, h - 4, tele ? RGB(0xffe347) : RGB(0x22e4f5));
        fr(c, x + 6, y + 6, w - 12, h - 12, RGB(0x1b2639));
        fr(c, x + 7, y + 8, 3, 4, RGB(0xf43fa5));
        fr(c, x + w - 10, y + 8, 3, 4, RGB(0xf43fa5));
        fr(c, x + 1, y, w - 2, 3, RGB(0xf5a623));
        fr(c, x + 1, y + h - 3, w - 2, 3, RGB(0xf5a623));
        break;
    case EN_FISH: {
        double cx = x + w / 2;
        if (!e->is_charging) {
            if (tele) { /* rings and bubbles where it is about to come up */
                double base = lm_round(e->vx);
                for (int k = 0; k < 3; k++) {
                    double grow = lm_fmod(time * 2.2 + k / 3.0, 1.0);
                    fr(c, cx - 5 - grow * 14, base - 1, 6, 2, RGBA(0xffffff, 1 - grow));
                    fr(c, cx - 1 + grow * 14, base - 1, 6, 2, RGBA(0xffffff, 1 - grow));
                    fr(c, lm_round(cx - 8 + k * 7), base - 3 - grow * 14, 3, 3, RGBA(0xdff8ff, 1 - grow));
                }
                fr(c, cx - 1, base - 26, 3, 8, RGB(0xffe347));
                fr(c, cx - 1, base - 15, 3, 3, RGB(0xffe347));
            }
            break;
        }
        double cy = y + h / 2, d = e->vy < 0 ? -1 : 1; /* d points the way the nose goes */
        double flick = lm_sin(time * 26 + phase) * 3;
        tri(c, cx, cy - d * 7, cx - 9 + flick, cy - d * 18, cx + 9 + flick, cy - d * 18, RGB(0xff7b54));
        tri(c, cx - 7, cy - d * 2, cx - 15, cy - d * 8, cx - 5, cy + d * 4, RGB(0xe2562f));
        tri(c, cx + 7, cy - d * 2, cx + 15, cy - d * 8, cx + 5, cy + d * 4, RGB(0xe2562f));
        disc(c, cx, cy - d * 3, 8, RGB(0xff9f43));
        disc(c, cx, cy + d * 3, 9, RGB(0xff9f43));
        disc(c, cx, cy + d * 1, 5, RGB(0xffd08a));
        fr(c, lm_round(cx - 7), lm_round(cy + d * 3 - 2), 5, 5, RGB(0xffffff));
        fr(c, lm_round(cx + 2), lm_round(cy + d * 3 - 2), 5, 5, RGB(0xffffff));
        fr(c, lm_round(cx - 5), lm_round(cy + d * 3 - 1 + d), 2, 2, RGB(0x1b1010));
        fr(c, lm_round(cx + 3), lm_round(cy + d * 3 - 1 + d), 2, 2, RGB(0x1b1010));
        for (int k = -1; k <= 1; k++) /* teeth */
            tri(c, cx + k * 5 - 2, cy + d * 9, cx + k * 5 + 2, cy + d * 9, cx + k * 5, cy + d * 14, RGB(0xffffff));
        break;
    }
    case EN_SPIDER: {
        double cx = x + w / 2 + (tele ? lm_round(lm_sin(time * 55) * 2) : 0), cy = y + h / 2;
        ln(c, x + w / 2, lm_round(e->s.min_y), cx, cy - 6, 1.2, RGBA(0xe6dccb, 0.7));
        double kick = tele || e->is_charging ? lm_sin(time * 30) * 2 : lm_sin(time * 4 + phase);
        for (int i = 0; i < 4; i++) {
            double ky = cy - 6 + i * 4, bend = i % 2 ? kick : -kick;
            for (int side = -1; side <= 1; side += 2) {
                ln(c, cx + side * 4, cy - 1 + i, cx + side * 11, ky + bend, 1.6, RGB(0xff9f43));
                ln(c, cx + side * 11, ky + bend, cx + side * 13, ky + 5 + bend, 1.6, RGB(0xff9f43));
            }
        }
        disc(c, cx, cy - 2, 7, RGB(0x8c2f39));
        disc(c, cx, cy + 5, 5, tele ? RGB(0xffe347) : RGB(0xc2453f));
        fr(c, lm_round(cx - 4), lm_round(cy + 4), 3, 3, RGB(0xfff6d1));
        fr(c, lm_round(cx + 1), lm_round(cy + 4), 3, 3, RGB(0xfff6d1));
        break;
    }
    default: {
        double pulse = 1 + 0.16 * lm_sin(time * 7 + phase);
        double cx = x + w / 2, cy = y + h / 2;
        disc(c, cx, cy, 12 * pulse, RGBA(0x22e4f5, 0.32));
        disc(c, cx, cy, 7 * pulse, RGB(0x22e4f5));
        fr(c, lm_round(cx - 2), lm_round(cy - 2), 4, 4, RGB(0xffffff));
        break;
    }
    }
}

/* ------------------------------------------------------------------ player */

struct ptr { struct wc const *c; double ox, oy, sx, sy; };

static void
pr(struct ptr const *t, double lx, double ly, double w, double h, uint32_t col)
{
    double x0 = t->ox + t->sx * lx, x1 = t->ox + t->sx * (lx + w);
    double y0 = t->oy + t->sy * ly, y1 = t->oy + t->sy * (ly + h);
    double x = x0 < x1 ? x0 : x1, y = y0 < y1 ? y0 : y1;
    fr(t->c, x, y, lm_fabs(x1 - x0), lm_fabs(y1 - y0), col);
}

/* A running power blinks through its last few seconds. */
static bool
showing(double left, double time)
{
    return left > 0 && (left > POWER_WARNING || (long)lm_floor(time * 8) % 2 == 0);
}

/* What the miner is under the influence of, drawn on and around them, with the time left underneath. */
static void
draw_player(struct wc const *c, struct lm_player const *p, double time, uint32_t lantern)
{
    if (p->invuln > 0 && (long)lm_floor(p->invuln * 14) % 2 == 0)
        return;

    bool spring = showing(p->spring, time), magnet = showing(p->magnet, time), slow = showing(p->slow, time);
    uint32_t scarf = slow ? RGB(0x22e4f5) : RGB(0xe04f70);
    for (int i = 1; i < 4; i++)
        ln(c, p->scarf[i - 1].x, p->scarf[i - 1].y, p->scarf[i].x, p->scarf[i].y, slow ? 5 : 4, scarf);

    double cx = p->x + p->width * 0.5, by = p->y + p->height;
    struct ptr t = {c, lm_round(cx), lm_round(by), p->facing * p->squash_x, p->squash_y};

    if (p->has_shield) {
        double aura = 19 + lm_sin(time * 6) * 1.5;
        double avg = (lm_fabs(t.sx) + t.sy) * 0.5;
        disc(c, t.ox, t.oy - 13 * t.sy, aura * avg, RGBA(0xf5a623, 0.14));
        ring(c, t.ox, t.oy - 13 * t.sy, aura * avg, 2, RGB(0xf5a623));
    }
    if (magnet)
        for (int i = 0; i < 2; i++) { /* rings drawing inwards */
            double k = time * 1.6 + i * 0.5;
            k -= lm_floor(k);
            ring(c, t.ox, t.oy - 13, 34 - 24 * k, 1.5, RGBA(0xffe347, 0.25 + 0.6 * k));
        }
    if (spring)
        disc(c, t.ox, t.oy - 3, 13, RGBA(0x2ce87d, 0.28));

    double stride = (p->grounded && lm_fabs(p->vx) > 15) || p->climbing ? lm_sin(p->walk_phase * 12) * 3.5 : 0;
    uint32_t boots = RGB(0x4a3225);
    pr(&t, lm_round(-7 + stride), -5, 6, 5, boots);
    pr(&t, lm_round(1 - stride), -5, 6, 5, boots);
    pr(&t, -7, -15, 14, 10, RGB(0x3c6e8f));
    pr(&t, -5, -14, 10, 6, RGB(0xd98a4e));
    pr(&t, 1, -12, 2, 2, RGB(0xffe347));
    pr(&t, -7, -16, 14, 3, scarf);
    pr(&t, -5, -22, 11, 6, RGB(0xf5cba7));
    pr(&t, -4, -18, 9, 3, RGB(0x8b5a3c));
    pr(&t, 2, -21, 2, 2, RGB(0x1b1722));
    pr(&t, -7, -26, 14, 5, RGB(0xc98e32));
    pr(&t, -5, -27, 10, 2, RGB(0xf5d076));
    pr(&t, 5, -25, 3, 3, RGB(0xfff9df));
    pr(&t, 6, -24, 2, 2, lantern);

    if (showing(p->lead, time)) { /* great iron clogs */
        for (int i = 0; i < 2; i++) {
            double bx = lm_round(i ? 1 - stride : -10 + stride);
            pr(&t, bx, -7, 9, 7, RGB(0x8d93a3));
            pr(&t, bx, -2, 9, 2, RGB(0x3a3e4a));
            pr(&t, bx + 3, -6, 2, 2, RGB(0xe8ecf5));
        }
    } else if (spring) { /* big green boots, shedding sparks */
        pr(&t, lm_round(-9 + stride), -7, 8, 7, RGB(0x2ce87d));
        pr(&t, lm_round(1 - stride), -7, 8, 7, RGB(0x2ce87d));
        for (int i = 0; i < 4; i++) {
            double k = time * 2.2 + i * 0.25;
            k -= lm_floor(k);
            fr(c, lm_round(t.ox - 13 + i * 8), lm_round(t.oy - 2 - k * 14), 2, 2, RGBA(0xa8ffcb, 1 - k));
        }
    }
    if (slow) { /* a little clock over the head */
        double ky = t.oy - 39;
        disc(c, t.ox, ky, 7, RGB(0x0c2a33));
        ring(c, t.ox, ky, 7, 1.5, RGB(0x22e4f5));
        ln(c, t.ox, ky, t.ox, ky - 5, 1.2, RGB(0xe8fdff));
        ln(c, t.ox, ky, t.ox + lm_cos(time * 3) * 4, ky + lm_sin(time * 3) * 4, 1.2, RGB(0xe8fdff));
    }
    if (showing(p->muddle, time)) /* stars going round */
        for (int i = 0; i < 3; i++) {
            double a = time * 6 + i * 2.094;
            double sx = lm_round(t.ox + lm_cos(a) * 12), sy = lm_round(t.oy - 31 + lm_sin(a) * 4);
            fr(c, sx - 1, sy - 3, 2, 6, RGB(0xd08cff));
            fr(c, sx - 3, sy - 1, 6, 2, RGB(0xd08cff));
        }

    /* time bars over the head: under the feet they hide behind the room name on the bottom floor */
    struct { double left, full; uint32_t col; } const bars[5] = {
        {p->spring, lm_powerups[PU_SPRING_BOOTS].duration, 0x2ce87d},
        {p->magnet, lm_powerups[PU_STARLIGHT_MAGNET].duration, 0xffe347},
        {p->slow, lm_powerups[PU_CLOCKWORK_SLOW].duration, 0x22e4f5},
        {p->lead, LEAD_TIME, 0xb45cff},
        {p->muddle, MUDDLE_TIME, 0xb45cff}};
    double y = lm_round(by) - 54;
    for (int i = 0; i < 5; i++) {
        if (bars[i].left <= 0)
            continue;
        fr(c, t.ox - 13, y, 26, 4, RGBA(0x0a080d, 0.85));
        if (showing(bars[i].left, time))
            fr(c, t.ox - 12, y + 1, lm_max(1, 24 * lm_min(1, bars[i].left / bars[i].full)), 2, RGB(bars[i].col));
        y -= 4;
    }
}

/* ------------------------------------------------------------------- water */

static void
draw_water(struct wc const *c, struct lm_game const *g, double time)
{
    struct lm_tide const *t = &g->cavern.tide;
    if (!t->on)
        return;
    double floor = CANVAS_H - TILE_SIZE, y = g->water_y;
    double x0 = TILE_SIZE, span = CANVAS_W - TILE_SIZE * 2;
    bool warn = g->tide_phase == TIDE_WARN && g->drain_to < t->low_y;
    bool will_rise = t->high_y < t->low_y && g->drain_to < t->low_y;

    /* the high-water mark, so there are no surprises; it flashes when the water is about to move */
    if (will_rise && g->tide_phase != TIDE_HIGH) {
        double mark = lm_max(t->high_y, g->drain_to);
        double a = warn ? 0.55 + 0.45 * lm_sin(time * 14) : 0.22;
        if (mark < y - 4)
            for (double x = x0 + 4; x < x0 + span - 8; x += 18)
                fr(c, x, mark - 1, 9, 2, RGBA(warn ? 0xffffff : 0x8fe6ff, a));
        if (warn)
            for (int k = 0; k < 8; k++) {
                double ax = 70 + k * 117, lift = lm_fmod(time * 26 + k * 7, 18);
                tri(c, ax - 6, y - 8 - lift, ax + 6, y - 8 - lift, ax, y - 18 - lift, RGBA(0x8fe6ff, 0.8 - lift / 24));
            }
    }
    if (y >= floor)
        return;

    float sx = c->ox + (float)x0 * c->s, sw = (float)span * c->s;
    vkr_rect_grad(c->r, sx, c->oy + (float)(y + 3) * c->s, sw, (float)(floor - y - 3) * c->s, RGBA(0x25c4ea, 0.30),
                  RGBA(0x0a3b66, 0.62));
    double swell = warn ? 3.4 : 1.8;
    for (double x = x0; x < x0 + span; x += 6) {
        double wave = lm_sin(time * (warn ? 7 : 2.6) + x * 0.045) * swell + lm_sin(time * 1.7 - x * 0.021) * swell * 0.6;
        double top = y + wave, w = lm_min(6, x0 + span - x);
        fr(c, x, top, w, 3 - wave > 0 ? 3 - wave : 0, RGBA(0x25c4ea, 0.30));
        fr(c, x, top, w, 2, RGBA(0xd6f8ff, 0.92));
        fr(c, x, top + 2, w, 3, RGBA(0x4fd8f5, 0.45));
    }
    /* wavering light under the surface */
    for (int i = 0; i < 18; i++) {
        double cx = x0 + lm_fmod(i * 131 + time * (7 + i % 4), span - 30);
        double cy = y + 12 + (i * 47) % 150;
        if (cy < floor - 4)
            fr(c, cx + lm_sin(time * 1.9 + i) * 8, cy, 14 + (i % 3) * 8, 2, RGBA(0xbff2ff, 0.10 + 0.06 * lm_sin(time * 2.3 + i)));
    }
    int bubbles = warn || g->tide_phase == TIDE_RISING ? 44 : 20;
    for (int i = 0; i < bubbles; i++) {
        double depth = floor - y;
        double k = lm_fmod(time * (14 + i % 7 * 4) + i * 53, lm_max(8, depth));
        double bx = x0 + 6 + (i * 89) % (int)(span - 12) + lm_sin(time * 3 + i) * 3;
        fr(c, lm_round(bx), lm_round(floor - k), 2 + i % 2, 2 + i % 2, RGBA(0xdff8ff, 0.5));
    }
}

/* ---------------------------------------------------------------- lighting */

static void
draw_lighting(struct wc const *c, struct lm_game const *g, bool bloom)
{
    struct lm_cavern const *cv = &g->cavern;
    struct lm_player const *p = &g->player;
    double time = g->elapsed;
    double bonus = g->perks[PERK_BRASS_LANTERN] * 34;
    double k = bloom ? 1.35 : 1.0;

    vkr_blend(c->r, true);

    double px = p->x + p->width * 0.5 + p->facing * 8, py = p->y + 7;
    double prad = 98 + bonus + lm_sin(time * 7) * 3;
    glow(c, px, py, prad, RGBA(0xf5b041, 0.24), 0.08 / 0.24);
    if (bloom)
        glow(c, px, py, prad * 1.9, RGBA(0xf59a2d, 0.07), 0.45);

    for (int i = 0; i < cv->n_lanterns; i++) {
        struct lm_lantern const *l = &cv->lanterns[i];
        double sway = lm_sin(time * 1.4 + l->sway_phase) * 3.5;
        double lx = l->x + sway, ly = l->y;
        double rad = 68 + lm_sin(time * 6 + l->sway_phase) * 3;
        glow(c, lx, ly, rad, RGBA(0xf5a623, 0.2 * k), 0.3);
        if (bloom)
            glow(c, lx, ly, rad * 1.8, RGBA(0xf58c23, 0.05), 0.4);
    }

    if (g->portal_unlocked) {
        double cx = cv->portal.x + TILE_SIZE, cy = cv->portal.y + TILE_SIZE;
        double rad = 84 + lm_sin(time * 4.5) * 6;
        glow(c, cx, cy, rad, RGBA(0x2ce87d, 0.3 * k), 0.1 / 0.3);
        if (bloom)
            glow(c, cx, cy, rad * 1.7, RGBA(0x2ce87d, 0.08), 0.4);
    }

    for (int i = 0; i < cv->n_collect; i++) {
        struct lm_collectable const *it = &cv->collect[i];
        if (it->collected || it->kind == COL_RELIC)
            continue;
        double rad = it->kind == COL_KEY ? 28 : 34;
        uint32_t col = it->kind == COL_KEY   ? RGBA(0xf43fa5, 0.18 * k)
                       : it->kind == COL_AIR ? RGBA(0x22e4f5, 0.18 * k)
                       : it->kind == COL_PLUG ? RGBA(0xff4f6a, 0.24 * k)
                                             : RGBA(0xffe347, 0.16 * k);
        glow(c, it->x, it->y, rad, col, 0.5);
    }
    vkr_blend(c->r, false);
}

/* ------------------------------------------------------------------- icons */

void
lm_draw_icon(struct vkr *r, int icon, float x, float y, float scale, double time)
{
    static struct lm_game g; /* a scratch cavern holding just the thing being shown */
    struct wc c = {r, x, y, scale};
    memset(&g.cavern, 0, sizeof(g.cavern));
    memset(g.crumble, 0, sizeof(g.crumble));
    g.n_movers = 0;
    g.keys_collected = 0;
    g.portal_unlocked = false;
    g.cavern.depth = 1;

    if (icon == ICON_MINER || icon == ICON_FALL) {
        struct lm_player p = {0};
        p.x = -10;
        p.y = -13;
        p.width = 20;
        p.height = 26;
        p.facing = 1;
        p.grounded = icon == ICON_MINER;
        p.squash_x = icon == ICON_FALL ? 0.85 : 1;
        p.squash_y = icon == ICON_FALL ? 1.15 : 1;
        for (int i = 0; i < 4; i++) {
            p.scarf[i].x = -i * 4;
            p.scarf[i].y = icon == ICON_FALL ? -2 - i * 5 : -2 + i;
        }
        draw_player(&c, &p, time, RGB(lm_biomes[0].lantern_glow));
    } else if (icon >= ICON_KEY && icon <= ICON_CHEST) {
        struct lm_collectable it = {0};
        it.kind = icon == ICON_KEY ? COL_KEY : icon == ICON_AIR ? COL_AIR : icon == ICON_CHEST ? COL_CHEST : COL_RELIC;
        it.relic_variant = icon == ICON_GEM ? 1 : 0;
        draw_collectable(&c, &it, time);
    } else if (icon == ICON_PORTAL_SHUT || icon == ICON_PORTAL_OPEN) {
        c.s = scale * 0.62f;
        g.cavern.portal.x = -TILE_SIZE;
        g.cavern.portal.y = -TILE_SIZE - 2;
        g.cavern.keys_total = 4;
        g.keys_collected = 2;
        g.portal_unlocked = icon == ICON_PORTAL_OPEN;
        draw_portal(&c, &g, time);
    } else if (icon >= ICON_BEETLE && icon <= ICON_STALKER) {
        static int const kind[] = {EN_BEETLE, EN_CHARGER, EN_BOUNCER, EN_GLOW_BAT, EN_PISTON, EN_WISP, EN_SPITTER, EN_STALKER};
        struct lm_enemy e = {0};
        e.s.kind = kind[icon - ICON_BEETLE];
        e.s.direction = 1;
        e.width = e.s.kind == EN_BEETLE || e.s.kind == EN_CHARGER ? 24 : 22;
        e.height = e.s.kind == EN_BEETLE || e.s.kind == EN_CHARGER ? 20 : 22;
        e.s.x = -e.width / 2;
        e.s.y = -e.height / 2;
        draw_enemy(&c, &e, time, false);
    } else if (icon == ICON_FISH) {
        struct lm_enemy e = {0};
        e.s.kind = EN_FISH;
        e.width = e.height = 22;
        e.s.x = -11;
        e.s.y = -9;
        e.vy = -1;
        e.is_charging = true;
        draw_enemy(&c, &e, time, false);
    } else if (icon == ICON_PLUG) {
        struct lm_collectable it = {0};
        it.kind = COL_PLUG;
        it.base_y = 6;
        draw_collectable(&c, &it, time);
    } else if (icon == ICON_RAFT) {
        struct lm_mover *m = &g.movers[0];
        memset(m, 0, sizeof(*m));
        m->raft = true;
        m->width = 34;
        m->x = -17;
        m->y = -4;
        g.n_movers = 1;
        draw_movers(&c, &g, time);
        for (int i = 0; i < 8; i++)
            fr(&c, -22 + i * 6, 8 + lm_sin(time * 3 + i) * 1.5, 6, 2, RGB(0x25c4ea));
    } else if (icon == ICON_TIDE) {
        double lift = lm_sin(time * 1.6) * 7;
        fr(&c, -20, 2 - lift, 40, 18 + lift, RGBA(0x25c4ea, 0.45));
        fr(&c, -20, 2 - lift, 40, 2, RGB(0xd6f8ff));
        for (int x = -20; x < 20; x += 9)
            fr(&c, x, -13, 5, 2, RGB(0x8fe6ff));
        tri(&c, -5, -2 - lift, 5, -2 - lift, 0, -10 - lift, RGB(0xffffff));
    } else if (icon == ICON_SPIDER) {
        struct lm_enemy e = {0};
        e.s.kind = EN_SPIDER;
        e.width = e.height = 22;
        e.s.x = -11;
        e.s.y = -8;
        e.s.min_y = -20;
        draw_enemy(&c, &e, time, false);
    } else if (icon == ICON_SPARK) {
        double pulse = 1 + 0.2 * lm_sin(time * 18);
        disc(&c, 0, 0, 11 * pulse, RGBA(0xf43fa5, 0.32));
        disc(&c, 0, 0, 6, RGB(0xf43fa5));
        fr(&c, -2, -2, 4, 4, RGB(0xffffff));
    } else if (icon == ICON_LIFT) {
        c.s = scale * 0.7f;
        struct lm_mover *m = &g.movers[0];
        memset(m, 0, sizeof(*m));
        m->width = 40;
        m->min_x = -28;
        m->max_x = -12;
        m->x = -20;
        m->y = m->min_y = m->max_y = -5;
        g.n_movers = 1;
        draw_movers(&c, &g, time);
    } else {
        /* one tile (or a short run of them), drawn by the cavern's own tile code */
        int tile = icon == ICON_SPIKES ? T_BRAMBLE : icon == ICON_POOL ? T_PIT : icon == ICON_CRUMBLE ? T_CRUMBLE : T_CONVEYOR_RIGHT;
        if (icon == ICON_LADDER) {
            g.cavern.ladders[0][0] = g.cavern.ladders[1][0] = 1;
            c.s = scale * 0.65f;
            c.ox = x - TILE_SIZE * 0.5f * c.s;
            c.oy = y - (TILE_SIZE - 4) * c.s;
            draw_ladders(&c, &g.cavern);
        } else {
            g.cavern.tiles[0][0] = (uint8_t)tile;
            c.s = scale * 1.1f;
            c.ox = x - TILE_SIZE * 0.5f * c.s;
            c.oy = y - (tile == T_BRAMBLE || tile == T_PIT ? TILE_SIZE * 0.65f : 7) * c.s;
            draw_tiles(&c, &g, time, false);
        }
    }
}

/* ------------------------------------------------------------------ frame */

void
lm_draw_world(struct vkr *r, struct lm_game const *g, struct lm_world_opts const *o)
{
    struct lm_cavern const *cv = &g->cavern;
    double time = g->elapsed;
    bool slow = g->player.slow > 0;
    struct wc c = {r, o->ox, o->oy, o->scale};
    if (g->shake_enabled) {
        c.ox += (float)lm_round(g->shake_x) * o->scale;
        c.oy += (float)lm_round(g->shake_y) * o->scale;
    }
    struct lm_biome const *b = &lm_biomes[cv->biome];

    /* 1. gradient backdrop (a little oversize so screen shake never shows the edge) */
    vkr_rect_grad(r, c.ox - 10 * c.s, c.oy - 10 * c.s, (CANVAS_W + 20) * c.s, (CANVAS_H + 20) * c.s, RGB(b->bg_top), RGB(b->bg_bottom));
    draw_background(&c, cv, time);
    draw_ambient_lanterns(&c, cv, time);
    if (slow)
        fr(&c, 0, 0, CANVAS_W, CANVAS_H, RGBA(0x22e4f5, 0.05));
    draw_tiles(&c, g, time, slow);
    draw_ladders(&c, cv);
    draw_movers(&c, g, time);
    draw_portal(&c, g, time);
    for (int i = 0; i < cv->n_collect; i++) {
        struct lm_collectable const *it = &cv->collect[i];
        if (!it->collected)
            draw_collectable(&c, it, time);
        else if (it->kind == COL_CHEST)
            draw_chest(&c, lm_round(it->x), lm_round(it->base_y), true, time);
    }
    for (int i = 0; i < g->n_enemies; i++) {
        draw_enemy(&c, &g->enemies[i], time, slow);
        struct lm_enemy const *e = &g->enemies[i];
        if (e->stun > 0) { /* stunned by a shield knock: three stars circle its head */
            double cx = e->s.x + e->width * 0.5, cy = e->s.y - 6;
            for (int k = 0; k < 3; k++) {
                double a = time * 7 + k * 2.094;
                fr(&c, lm_round(cx + lm_cos(a) * 9) - 1.5, lm_round(cy + lm_sin(a) * 3) - 1.5, 3, 3, RGB(0xffe347));
            }
        }
    }

    for (int i = 0; i < g->n_proj; i++) {
        struct lm_projectile const *p = &g->proj[i];
        double x = lm_round(p->x), y = lm_round(p->y), pulse = 1 + 0.2 * lm_sin(time * 18);
        disc(&c, x, y, p->radius * 1.75 * pulse, RGBA(0xf43fa5, 0.32));
        disc(&c, x, y, p->radius, RGB(p->color));
        fr(&c, x - 2, y - 2, 4, 4, RGB(0xffffff));
    }

    draw_player(&c, &g->player, time, RGB(b->lantern_glow));
    draw_water(&c, g, time);

    for (int i = 0; i < LM_MOTES; i++) {
        struct lm_mote const *m = &g->motes[i];
        double tw = 0.45 + 0.55 * lm_sin(time * 2.4 + m->phase);
        fr(&c, lm_round(m->x), lm_round(m->y), m->radius, m->radius, RGBA(b->dust, m->alpha * tw));
    }

    draw_lighting(&c, g, o->bloom);

    for (int i = 0; i < g->n_particles; i++) {
        struct lm_particle const *p = &g->particles[i];
        fr(&c, lm_round(p->x - p->size * 0.5), lm_round(p->y - p->size * 0.5), p->size, p->size, RGBA(p->color, lm_max(0, p->alpha)));
    }

    for (int i = 0; i < g->n_floats; i++) {
        struct lm_float_text const *f = &g->floats[i];
        double progress = 1 - f->life / f->max_life;
        double a = lm_max(0, 1 - progress * progress);
        float size = 26 * o->scale / 2.0f;
        float tw = vkr_text_width(r, VKR_MONO_BOLD, size, f->text);
        float tx = c.ox + (float)lm_round(f->x) * c.s - tw / 2, ty = c.oy + (float)lm_round(f->y) * c.s - size * 0.82f;
        vkr_text(r, VKR_MONO_BOLD, size, tx + c.s, ty + c.s, RGBA(0x110f15, 0.85 * a), f->text);
        vkr_text(r, VKR_MONO_BOLD, size, tx, ty, RGBA(f->color, a), f->text);
    }

    if (g->hit_flash > 0.01)
        fr(&c, 0, 0, CANVAS_W, CANVAS_H, RGBA(0xf43fa5, g->hit_flash * 0.28));

    vkr_vignette(r, c.ox, c.oy, CANVAS_W * c.s, CANVAS_H * c.s, c.ox + CANVAS_W * 0.5f * c.s, c.oy + CANVAS_H * 0.5f * c.s,
                 (float)(CANVAS_W * 0.34) * c.s, (float)(CANVAS_W * 0.64) * c.s, VKR_RGB(10, 8, 14), 0.42f);

    if (g->banner.active && g->banner.remaining > 0) {
        double progress = 1 - g->banner.remaining / g->banner.duration;
        double alpha = progress < 0.15 ? progress / 0.15 : progress > 0.82 ? (1 - progress) / 0.18 : 1;
        alpha = lm_clamp(alpha, 0, 1);
        double bw = 520, bh = 96;
        double bx = (CANVAS_W - bw) / 2, by = (CANVAS_H - bh) / 2 - 12;
        fr(&c, bx - 6, by - 6, bw + 12, bh + 12, RGBA(0x000000, 0.35 * alpha));
        fr(&c, bx, by, bw, bh, RGBA(0x14101a, 0.94 * alpha));
        stroke_rect(&c, bx + 1, by + 1, bw - 2, bh - 2, 2, RGBA(0xf5a623, alpha));
        {
            float gx = c.ox + (float)(bx + 24) * c.s, gy = c.oy + (float)(by + 52) * c.s, gw = (float)(bw - 48) * c.s, gh = 2 * c.s;
            vkr_rect_gradh(r, gx, gy, gw * 0.5f, gh, RGBA(0xf5a623, alpha), RGBA(0xf43fa5, alpha));
            vkr_rect_gradh(r, gx + gw * 0.5f, gy, gw * 0.5f, gh, RGBA(0xf43fa5, alpha), RGBA(0x2ce87d, alpha));
        }
        float ts = 25.0f * c.s, ss = 13.5f * c.s;
        float w1 = vkr_text_width(r, VKR_DISPLAY, ts, g->banner.title);
        float w2 = vkr_text_width(r, VKR_MONO_BOLD, ss, g->banner.subtitle);
        float cxs = c.ox + CANVAS_W * 0.5f * c.s;
        vkr_text(r, VKR_DISPLAY, ts, cxs - w1 / 2, c.oy + (float)(by + 10) * c.s, RGBA(0xf6efe2, alpha), g->banner.title);
        vkr_text(r, VKR_MONO_BOLD, ss, cxs - w2 / 2, c.oy + (float)(by + 62) * c.s, RGBA(0xffe347, alpha), g->banner.subtitle);
    }
}
