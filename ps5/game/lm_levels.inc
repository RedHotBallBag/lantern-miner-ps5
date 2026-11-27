/*
 * The hand-built caverns, included by lm_gen.c. The table at the bottom is the playing order.
 *
 * The grid is 32 x 18 tiles, with the walls, ceiling and floor added for you. Columns 1..30, rows 1..16.
 * Things you stand on are given by the row they occupy; items, foes, start and exit by the row they are
 * IN (one above the ledge). A jump climbs 3 rows (so 4 rows apart needs a ladder or lift) and clears
 * about 5 tiles (8 or more needs a moving platform). Foes are listed most basic first, and early caverns
 * only use the first few. Run tools/check-levels.sh after changing a map.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define BLOCK(c0, r0, c1, r1) {OP_FILL, T_SOLID, c0, r0, c1, r1}
#define LEDGE(r, c0, c1) {OP_FILL, T_ONE_WAY, c0, r, c1, r}
#define BELT_L(r, c0, c1) {OP_FILL, T_CONVEYOR_LEFT, c0, r, c1, r}
#define BELT_R(r, c0, c1) {OP_FILL, T_CONVEYOR_RIGHT, c0, r, c1, r}
#define CRUMBLE(r, c0, c1) {OP_FILL, T_CRUMBLE, c0, r, c1, r}
#define SPIKES(r, c0, c1) {OP_FILL, T_BRAMBLE, c0, r, c1, r}
#define PIT(c0, c1) {OP_FILL, T_PIT, c0, 16, c1, 16}
#define LADDER(c, top, bottom) {OP_LADDER, 0, c, top, c, bottom}
#define SLIDER(r, left0, left1, w) {OP_MOVER, w, left0, r, left1, r}   /* left edge travels left0..left1 */
#define LIFT(c, top, bottom, w) {OP_MOVER, w, c, top, c, bottom}       /* surface travels top..bottom */
#define START(c, r) {OP_START, 0, c, r, 0, 0}
#define EXIT(c, r) {OP_EXIT, 0, c, r, 0, 0} /* two tiles wide, c is the left one */
#define KEY(c, r) {OP_KEY, 0, c, r, 0, 0}
#define GEM(c, r) {OP_GEM, 0, c, r, 0, 0}
#define CHEST(c, r) {OP_CHEST, 0, c, r, 0, 0}
#define FOE(kind, c, r) {OP_FOE, kind, c, r, 0, 0}
/* water. A surface at row r leaves things stood on rows r and above dry. Row 17 is a dry floor. */
#define TIDE(low, high, low_secs, high_secs) {OP_TIDE, low_secs, low, high, high_secs, 0}
#define FLOOD(r) TIDE(r, r, 1, 1)                            /* still water, until a plug is pulled */
#define RAFT(r, left0, left1, w) {OP_RAFT, w, left0, r, left1, r} /* drifts if left0 != left1 */
#define PLUG(c, r, to_row) {OP_PLUG, to_row, c, r, 0, 0}

/* ------------------------------------------------------------- tier 1: learn one thing at a time */

static struct op const lv_hearth[] = {
    LEDGE(14, 10, 15), LEDGE(11, 17, 23), LEDGE(8, 24, 29), LEDGE(12, 2, 8), LEDGE(7, 3, 9),
    LADDER(5, 12, 16), LADDER(8, 7, 11), SLIDER(7, 11, 18, 3),
    START(3, 16), EXIT(27, 7),
    KEY(3, 11), KEY(4, 6), KEY(20, 10), KEY(27, 16),
    GEM(14, 13), GEM(15, 5), GEM(22, 16), CHEST(11, 13),
    FOE(EN_BEETLE, 20, 16), FOE(EN_GLOW_BAT, 16, 4), FOE(EN_BEETLE, 20, 10), FOE(EN_BOUNCER, 13, 13),
};

static struct op const lv_crossing[] = {
    BLOCK(1, 13, 8, 16), BLOCK(23, 13, 30, 16), PIT(9, 22),
    SLIDER(13, 9, 20, 3), LEDGE(10, 3, 7), LEDGE(10, 24, 28),
    START(2, 12), EXIT(27, 12),
    KEY(5, 9), KEY(15, 10), KEY(26, 9), KEY(30, 12),
    GEM(12, 11), GEM(19, 11), GEM(3, 9), CHEST(7, 9),
    FOE(EN_BEETLE, 24, 12), FOE(EN_GLOW_BAT, 16, 6), FOE(EN_BEETLE, 25, 9), FOE(EN_GLOW_BAT, 13, 4), FOE(EN_WISP, 16, 7),
};

static struct op const lv_loft[] = {
    LEDGE(13, 3, 14), LEDGE(13, 18, 28), LEDGE(9, 2, 10), LEDGE(9, 14, 29), LEDGE(5, 4, 20), LEDGE(5, 24, 29),
    LADDER(12, 13, 16), LADDER(26, 13, 16), LADDER(5, 9, 12), LADDER(20, 9, 12), LADDER(16, 5, 8), LADDER(27, 5, 8),
    START(3, 16), EXIT(24, 4),
    KEY(8, 12), KEY(28, 8), KEY(6, 4), KEY(2, 8), KEY(22, 16),
    GEM(23, 12), GEM(12, 4), GEM(16, 16), CHEST(4, 12),
    FOE(EN_BEETLE, 20, 16), FOE(EN_BEETLE, 23, 12), FOE(EN_BEETLE, 23, 8), FOE(EN_GLOW_BAT, 12, 7), FOE(EN_BEETLE, 11, 4),
    FOE(EN_CHARGER, 8, 8),
};

static struct op const lv_stones[] = {
    BLOCK(1, 15, 5, 16), PIT(6, 30),
    LEDGE(14, 8, 10), LEDGE(12, 13, 15), LEDGE(14, 18, 20), LEDGE(12, 23, 25), LEDGE(10, 27, 30),
    LEDGE(8, 21, 23), LEDGE(7, 15, 17), LEDGE(8, 9, 11), LEDGE(6, 3, 6),
    START(2, 14), EXIT(3, 5),
    KEY(14, 11), KEY(29, 9), KEY(22, 7), KEY(10, 7), KEY(19, 13),
    GEM(24, 11), GEM(16, 6), GEM(9, 13), CHEST(15, 6),
    FOE(EN_GLOW_BAT, 17, 10), FOE(EN_GLOW_BAT, 13, 4), FOE(EN_BEETLE, 28, 9), FOE(EN_WISP, 24, 5), FOE(EN_PISTON, 21, 11),
    FOE(EN_GLOW_BAT, 22, 3),
};

static struct op const lv_dumbwaiter[] = {
    LEDGE(8, 5, 11), LEDGE(8, 22, 26), LEDGE(4, 12, 17),
    LIFT(2, 8, 16, 2), LIFT(28, 8, 16, 2), LIFT(19, 4, 8, 2),
    START(8, 16), EXIT(13, 3),
    KEY(6, 7), KEY(25, 7), KEY(16, 3), KEY(15, 16), KEY(30, 16),
    GEM(10, 7), GEM(23, 16), GEM(20, 2), CHEST(23, 7),
    FOE(EN_BEETLE, 20, 16), FOE(EN_BEETLE, 24, 7), FOE(EN_SPIDER, 15, 6), FOE(EN_BEETLE, 9, 7), FOE(EN_WISP, 13, 11),
    FOE(EN_PISTON, 25, 12), FOE(EN_GLOW_BAT, 8, 3),
};

/* ------------------------------------------------------------- tier 2: bigger ideas */

static struct op const lv_zigzag[] = {
    LEDGE(13, 1, 30), LEDGE(9, 1, 30), LEDGE(5, 1, 30),
    LADDER(28, 13, 16), LADDER(3, 9, 12), LADDER(28, 5, 8),
    SPIKES(16, 13, 13), SPIKES(12, 16, 16), SPIKES(8, 20, 20),
    START(3, 16), EXIT(2, 4),
    KEY(18, 16), KEY(24, 12), KEY(8, 12), KEY(13, 8), KEY(23, 4),
    GEM(9, 16), GEM(30, 8), GEM(12, 4), CHEST(1, 12),
    FOE(EN_BEETLE, 20, 16), FOE(EN_BEETLE, 10, 12), FOE(EN_CHARGER, 12, 8), FOE(EN_BEETLE, 18, 4),
    FOE(EN_BOUNCER, 22, 12), FOE(EN_BEETLE, 25, 8), FOE(EN_CHARGER, 10, 4), FOE(EN_BOUNCER, 24, 16),
};

static struct op const lv_works[] = {
    BELT_R(14, 3, 13), BELT_L(14, 18, 28), BELT_L(11, 8, 23), BELT_R(8, 2, 12), BELT_L(8, 19, 29), BELT_R(5, 9, 22),
    SPIKES(16, 14, 17), BLOCK(15, 14, 16, 14),
    START(2, 16), EXIT(18, 4),
    KEY(6, 13), KEY(26, 13), KEY(12, 10), KEY(3, 7), KEY(28, 7),
    GEM(15, 13), GEM(15, 7), GEM(20, 10), CHEST(10, 4),
    FOE(EN_BEETLE, 22, 16), FOE(EN_BEETLE, 18, 10), FOE(EN_BOUNCER, 8, 7), FOE(EN_PISTON, 16, 8),
    FOE(EN_CHARGER, 23, 7), FOE(EN_BEETLE, 23, 13), FOE(EN_GLOW_BAT, 8, 3), FOE(EN_SPITTER, 29, 7),
};

/* ------------------------------------------------------------- tier 3: hard */

static struct op const lv_towers[] = {
    BLOCK(15, 7, 16, 16), LEDGE(14, 2, 6), LEDGE(11, 8, 13), LEDGE(7, 12, 14), LEDGE(7, 17, 19),
    LEDGE(11, 18, 23), LEDGE(14, 25, 29), LEDGE(4, 3, 9), LEDGE(4, 22, 28),
    LADDER(12, 7, 10), LADDER(19, 7, 10),
    START(2, 16), EXIT(25, 3),
    KEY(4, 3), KEY(15, 6), KEY(9, 10), KEY(27, 13), KEY(28, 16), KEY(11, 16),
    GEM(2, 13), GEM(20, 16), GEM(16, 3), CHEST(8, 3),
    FOE(EN_BEETLE, 22, 16), FOE(EN_BEETLE, 11, 10), FOE(EN_CHARGER, 22, 10), FOE(EN_SPIDER, 10, 5),
    FOE(EN_SPITTER, 29, 13), FOE(EN_BOUNCER, 5, 3), FOE(EN_PISTON, 24, 9), FOE(EN_WISP, 7, 8), FOE(EN_SPIDER, 27, 7),
};

static struct op const lv_gallery[] = {
    SPIKES(16, 8, 11), SPIKES(16, 15, 18), SPIKES(16, 22, 25),
    LEDGE(14, 5, 6), LEDGE(14, 12, 14), LEDGE(14, 19, 21), LEDGE(14, 26, 29),
    LEDGE(11, 2, 5), LEDGE(11, 8, 23), SPIKES(10, 12, 12), SPIKES(10, 19, 19),
    LEDGE(8, 26, 29), LEDGE(8, 2, 4), LEDGE(5, 7, 24), SPIKES(4, 15, 16),
    START(2, 16), EXIT(22, 4),
    KEY(13, 16), KEY(28, 13), KEY(15, 10), KEY(3, 7), KEY(28, 7), KEY(8, 4),
    GEM(20, 16), GEM(3, 10), GEM(29, 16), CHEST(10, 4),
    FOE(EN_BEETLE, 28, 16), FOE(EN_CHARGER, 16, 10), FOE(EN_BEETLE, 20, 4), FOE(EN_GLOW_BAT, 13, 7),
    FOE(EN_SPITTER, 29, 7), FOE(EN_CHARGER, 11, 4), FOE(EN_PISTON, 24, 10), FOE(EN_WISP, 17, 13), FOE(EN_SPIDER, 15, 6),
};

static struct op const lv_rails[] = {
    BLOCK(1, 12, 3, 16), PIT(4, 30),
    SLIDER(12, 4, 12, 3), LIFT(16, 6, 12, 2), SLIDER(6, 19, 25, 3), LIFT(28, 6, 15, 2), SLIDER(15, 18, 24, 3),
    LEDGE(4, 22, 23), LEDGE(9, 1, 2),
    START(2, 11), EXIT(22, 3),
    KEY(9, 10), KEY(16, 4), KEY(23, 13), KEY(29, 4), KEY(19, 13),
    GEM(6, 10), GEM(28, 11), GEM(20, 4), CHEST(1, 8),
    FOE(EN_GLOW_BAT, 10, 7), FOE(EN_GLOW_BAT, 22, 10), FOE(EN_WISP, 11, 15), FOE(EN_GLOW_BAT, 12, 3),
    FOE(EN_WISP, 22, 9), FOE(EN_PISTON, 26, 10), FOE(EN_GLOW_BAT, 8, 5), FOE(EN_SPIDER, 16, 2),
};

/* ------------------------------------------------------------- tier 4: everything at once */

static struct op const lv_brokenladders[] = {
    BLOCK(1, 15, 4, 16), PIT(5, 30),
    LEDGE(14, 6, 10), LEDGE(10, 8, 13), LEDGE(10, 17, 21), LEDGE(6, 15, 23), LEDGE(14, 24, 29), LEDGE(10, 25, 29),
    LEDGE(6, 11, 12), LEDGE(6, 3, 8),
    LADDER(9, 10, 13), LADDER(19, 6, 9), LADDER(27, 10, 13),
    START(2, 14), EXIT(3, 5),
    KEY(13, 9), KEY(24, 13), KEY(29, 9), KEY(22, 5), KEY(7, 5),
    GEM(12, 5), GEM(18, 9), GEM(29, 13), CHEST(15, 5),
    FOE(EN_BEETLE, 12, 9), FOE(EN_CHARGER, 20, 5), FOE(EN_GLOW_BAT, 15, 12), FOE(EN_BEETLE, 26, 13),
    FOE(EN_SPITTER, 29, 9), FOE(EN_GLOW_BAT, 9, 3), FOE(EN_WISP, 23, 8), FOE(EN_PISTON, 15, 9), FOE(EN_SPIDER, 22, 2),
};

static struct op const lv_gauntlet[] = {
    BLOCK(1, 13, 24, 13), BLOCK(7, 9, 30, 9), BLOCK(1, 5, 24, 5),
    LEDGE(13, 25, 30), LEDGE(9, 1, 6), LEDGE(5, 25, 30),
    LADDER(28, 13, 16), LADDER(3, 9, 12), LADDER(28, 5, 8),
    SPIKES(16, 13, 13), SPIKES(12, 17, 17), SPIKES(8, 14, 14),
    START(3, 16), EXIT(2, 4),
    KEY(20, 16), KEY(8, 12), KEY(22, 8), KEY(14, 4), KEY(30, 4),
    GEM(10, 16), GEM(11, 8), GEM(20, 4), CHEST(30, 12),
    FOE(EN_BEETLE, 18, 16), FOE(EN_CHARGER, 12, 12), FOE(EN_CHARGER, 20, 8), FOE(EN_BEETLE, 10, 4),
    FOE(EN_SPITTER, 1, 12), FOE(EN_BOUNCER, 23, 16), FOE(EN_CHARGER, 18, 4), FOE(EN_BEETLE, 23, 12), FOE(EN_SPITTER, 30, 8),
    FOE(EN_SPIDER, 14, 1),
};

static struct op const lv_heart[] = {
    BLOCK(1, 15, 3, 16), BLOCK(28, 15, 30, 16), PIT(4, 27), BLOCK(14, 8, 17, 11),
    BELT_R(14, 6, 11), BELT_L(14, 20, 25), LEDGE(11, 2, 6), LEDGE(11, 25, 29),
    BELT_L(8, 5, 9), BELT_R(8, 22, 26), SLIDER(5, 6, 23, 3), SLIDER(15, 12, 17, 3),
    START(2, 14), EXIT(15, 7),
    KEY(2, 10), KEY(29, 10), KEY(7, 7), KEY(24, 7), KEY(15, 3),
    GEM(15, 14), GEM(8, 13), GEM(23, 13), CHEST(30, 14),
    FOE(EN_BEETLE, 22, 13), FOE(EN_GLOW_BAT, 10, 3), FOE(EN_BEETLE, 27, 10), FOE(EN_PISTON, 12, 10),
    FOE(EN_PISTON, 19, 10), FOE(EN_SPITTER, 29, 14), FOE(EN_WISP, 16, 13), FOE(EN_CHARGER, 24, 7), FOE(EN_SPIDER, 15, 1),
};

static struct op const lv_spire[] = {
    BLOCK(1, 15, 4, 16), PIT(5, 30),
    CRUMBLE(14, 7, 10), CRUMBLE(14, 13, 16), LEDGE(12, 19, 23), CRUMBLE(10, 26, 29), LEDGE(8, 19, 22),
    CRUMBLE(8, 12, 15), LEDGE(6, 6, 9), CRUMBLE(4, 12, 18), LEDGE(4, 22, 27),
    START(2, 14), EXIT(25, 3),
    KEY(14, 13), KEY(28, 9), KEY(21, 7), KEY(7, 5), KEY(15, 3),
    GEM(9, 13), GEM(13, 7), GEM(22, 11), CHEST(20, 11),
    FOE(EN_GLOW_BAT, 11, 11), FOE(EN_WISP, 25, 6), FOE(EN_GLOW_BAT, 17, 10), FOE(EN_BEETLE, 8, 5),
    FOE(EN_SPITTER, 23, 11), FOE(EN_WISP, 10, 10), FOE(EN_PISTON, 24, 9), FOE(EN_GLOW_BAT, 20, 2), FOE(EN_SPIDER, 21, 2),
};

static struct op const lv_abyss[] = {
    BLOCK(1, 14, 4, 16), BLOCK(27, 14, 30, 16), PIT(5, 26),
    SLIDER(14, 5, 12, 3), LEDGE(14, 16, 19), SLIDER(14, 21, 24, 3), LADDER(18, 10, 13), LEDGE(10, 16, 21),
    SLIDER(10, 8, 12, 3), LEDGE(10, 4, 6), LIFT(1, 5, 10, 2), LEDGE(5, 4, 10), SLIDER(5, 12, 21, 3), LEDGE(5, 25, 29),
    START(2, 13), EXIT(27, 4),
    KEY(29, 13), KEY(10, 8), KEY(1, 3), KEY(17, 3), KEY(21, 9),
    GEM(13, 12), GEM(23, 12), GEM(23, 3), CHEST(5, 4),
    FOE(EN_BEETLE, 28, 13), FOE(EN_GLOW_BAT, 10, 12), FOE(EN_BEETLE, 20, 9), FOE(EN_CHARGER, 8, 4),
    FOE(EN_SPITTER, 29, 4), FOE(EN_WISP, 23, 8), FOE(EN_PISTON, 14, 7), FOE(EN_GLOW_BAT, 20, 2), FOE(EN_WISP, 8, 15),
    FOE(EN_SPIDER, 17, 1),
};

/* ------------------------------------------------------------- World 1 additions */

static struct op const lv_stair[] = {
    BLOCK(8, 14, 11, 16), BLOCK(12, 11, 15, 16), BLOCK(16, 8, 19, 16), BLOCK(20, 11, 23, 16), BLOCK(24, 14, 26, 16),
    LEDGE(5, 2, 12), LEDGE(5, 22, 28), LADDER(12, 5, 10), LADDER(22, 5, 10), SPIKES(16, 28, 28),
    START(2, 16), EXIT(3, 4),
    KEY(5, 16), KEY(14, 10), KEY(18, 7), KEY(27, 4), KEY(30, 16), KEY(9, 4),
    GEM(10, 13), GEM(21, 10), GEM(17, 3), CHEST(25, 4),
    FOE(EN_BEETLE, 25, 13), FOE(EN_GLOW_BAT, 17, 2), FOE(EN_BEETLE, 26, 4), FOE(EN_SPIDER, 6, 7),
    FOE(EN_BEETLE, 9, 4), FOE(EN_BOUNCER, 29, 16), FOE(EN_WISP, 24, 8),
};

static struct op const lv_pulley[] = {
    BLOCK(1, 15, 4, 16), PIT(5, 30),
    LIFT(6, 9, 15, 2), LEDGE(9, 9, 13), LADDER(11, 5, 8), LEDGE(5, 10, 16), LIFT(18, 5, 12, 2),
    LEDGE(12, 21, 25), LADDER(24, 8, 11), LEDGE(8, 22, 28), LIFT(29, 4, 8, 2), LEDGE(4, 22, 26),
    START(2, 14), EXIT(22, 3),
    KEY(12, 8), KEY(15, 4), KEY(22, 11), KEY(27, 7), KEY(30, 2),
    GEM(7, 7), GEM(18, 3), GEM(25, 3), CHEST(10, 4),
    FOE(EN_GLOW_BAT, 14, 12), FOE(EN_GLOW_BAT, 23, 15), FOE(EN_BEETLE, 14, 4), FOE(EN_WISP, 17, 10),
    FOE(EN_BEETLE, 25, 7), FOE(EN_GLOW_BAT, 8, 2), FOE(EN_PISTON, 27, 12),
};

static struct op const lv_greathearth[] = {
    BLOCK(14, 10, 17, 16), PIT(18, 30),
    LEDGE(13, 3, 11), LEDGE(9, 2, 7), LEDGE(5, 4, 11), LADDER(5, 13, 16), LADDER(3, 9, 12), LADDER(6, 5, 8),
    SLIDER(5, 13, 19, 3), LEDGE(5, 24, 29), SLIDER(13, 19, 25, 3), LEDGE(13, 29, 30), LEDGE(9, 22, 26),
    START(2, 16), EXIT(26, 4),
    KEY(10, 12), KEY(2, 8), KEY(15, 9), KEY(30, 12), KEY(24, 8), KEY(17, 3),
    GEM(10, 16), GEM(21, 11), GEM(13, 2), CHEST(9, 4),
    FOE(EN_BEETLE, 11, 16), FOE(EN_BEETLE, 24, 8), FOE(EN_GLOW_BAT, 11, 7), FOE(EN_BOUNCER, 9, 12),
    FOE(EN_BEETLE, 11, 4), FOE(EN_SPIDER, 22, 2), FOE(EN_WISP, 21, 11), FOE(EN_CHARGER, 28, 4),
};

/* ------------------------------------------------------------- World 2: the Flooded Hollow */

/* 11: meet the tide, with somewhere dry always a hop away */
static struct op const lv_paddling[] = {
    TIDE(17, 13, 4, 3),
    BLOCK(1, 13, 4, 16),
    LEDGE(14, 7, 9), LEDGE(11, 10, 12), LEDGE(14, 15, 17), LEDGE(11, 19, 21), LEDGE(14, 23, 25), LEDGE(11, 26, 29),
    LEDGE(8, 22, 24), LEDGE(5, 15, 20), LEDGE(5, 2, 11),
    START(2, 12), EXIT(2, 4),
    KEY(8, 16), KEY(16, 16), KEY(24, 16), KEY(28, 10), KEY(18, 4),
    GEM(11, 10), GEM(23, 7), GEM(12, 16), CHEST(26, 10),
    FOE(EN_BEETLE, 20, 16), FOE(EN_GLOW_BAT, 13, 7), FOE(EN_FISH, 13, 12), FOE(EN_BEETLE, 20, 10),
    FOE(EN_FISH, 22, 12), FOE(EN_WISP, 7, 8), FOE(EN_GLOW_BAT, 26, 3),
};

/* 12: meet rafts */
static struct op const lv_sinking[] = {
    BLOCK(1, 14, 4, 16), BLOCK(14, 14, 17, 16), BLOCK(27, 14, 30, 16), PIT(5, 26),
    RAFT(16, 6, 6, 3), RAFT(16, 11, 11, 2), RAFT(16, 19, 19, 2), RAFT(16, 23, 23, 3),
    LEDGE(11, 3, 6), LEDGE(11, 14, 17), LEDGE(11, 26, 29), LEDGE(8, 8, 11), LEDGE(8, 20, 23), LEDGE(5, 24, 29),
    START(2, 13), EXIT(27, 4),
    KEY(7, 15), KEY(12, 15), KEY(15, 10), KEY(24, 15), KEY(9, 7),
    GEM(20, 15), GEM(4, 10), GEM(15, 13), CHEST(29, 10),
    FOE(EN_FISH, 9, 12), FOE(EN_GLOW_BAT, 15, 6), FOE(EN_FISH, 22, 12), FOE(EN_BEETLE, 22, 7),
    FOE(EN_GLOW_BAT, 12, 3), FOE(EN_BEETLE, 28, 10), FOE(EN_WISP, 18, 5), FOE(EN_GLOW_BAT, 5, 6),
};

/* 13: ride the water up on a raft */
static struct op const lv_chasm[] = {
    TIDE(16, 13, 3, 4),
    BLOCK(1, 12, 5, 16), BLOCK(14, 12, 17, 16), BLOCK(26, 12, 30, 16), PIT(6, 13), PIT(18, 25),
    RAFT(16, 8, 8, 3), SLIDER(12, 18, 23, 3), SLIDER(7, 7, 21, 3),
    LEDGE(9, 15, 16), LEDGE(9, 2, 4), LEDGE(4, 25, 29),
    START(2, 11), EXIT(28, 11),
    KEY(3, 8), KEY(16, 8), KEY(9, 12), KEY(22, 10), KEY(26, 3),
    GEM(14, 11), GEM(12, 5), GEM(19, 5), CHEST(29, 3),
    FOE(EN_SPIDER, 16, 2), FOE(EN_BEETLE, 16, 11), FOE(EN_FISH, 12, 10), FOE(EN_BEETLE, 27, 11),
    FOE(EN_WISP, 21, 4), FOE(EN_FISH, 21, 10), FOE(EN_SPITTER, 28, 3), FOE(EN_GLOW_BAT, 10, 4),
};

/* 14: crumbling bridges, and the water comes over them */
static struct op const lv_bridges[] = {
    TIDE(16, 12, 4, 3),
    BLOCK(1, 11, 4, 16), BLOCK(27, 11, 30, 16), PIT(5, 26),
    CRUMBLE(13, 6, 12), LEDGE(13, 14, 17), CRUMBLE(13, 19, 25),
    LEDGE(10, 9, 11), LEDGE(10, 20, 22), CRUMBLE(8, 13, 18), LEDGE(6, 6, 9), LEDGE(6, 22, 25),
    START(2, 10), EXIT(28, 10),
    KEY(15, 12), KEY(16, 7), KEY(7, 5), KEY(24, 5), KEY(10, 9),
    GEM(8, 12), GEM(23, 12), GEM(14, 5), CHEST(21, 9),
    FOE(EN_FISH, 13, 9), FOE(EN_GLOW_BAT, 10, 3), FOE(EN_BEETLE, 16, 12), FOE(EN_FISH, 18, 9),
    FOE(EN_GLOW_BAT, 20, 3), FOE(EN_WISP, 9, 8), FOE(EN_SPITTER, 23, 5), FOE(EN_SPIDER, 16, 2),
};

/* 15: lifts that dip into the tide */
static struct op const lv_lifthall[] = {
    TIDE(16, 12, 4, 3),
    BLOCK(1, 12, 4, 16), PIT(5, 30),
    BLOCK(9, 12, 10, 16), BLOCK(15, 8, 16, 16), BLOCK(21, 5, 22, 16), LEDGE(12, 28, 30),
    LIFT(6, 12, 15, 2), LIFT(12, 8, 12, 2), LIFT(18, 5, 8, 2), LIFT(25, 5, 13, 2),
    START(2, 11), EXIT(29, 11),
    KEY(9, 11), KEY(15, 7), KEY(21, 4), KEY(13, 5), KEY(28, 8),
    GEM(7, 14), GEM(19, 3), GEM(26, 3), CHEST(22, 4),
    FOE(EN_GLOW_BAT, 12, 4), FOE(EN_FISH, 14, 8), FOE(EN_WISP, 19, 12), FOE(EN_PISTON, 24, 8),
    FOE(EN_SPITTER, 16, 7), FOE(EN_FISH, 23, 8), FOE(EN_PISTON, 14, 5), FOE(EN_SPIDER, 7, 2), FOE(EN_GLOW_BAT, 17, 2),
};

/* 16: one long slow flood, and the only way is up */
static struct op const lv_honeycomb[] = {
    TIDE(17, 6, 10, 3),
    LEDGE(14, 4, 6), LEDGE(14, 10, 12), LEDGE(14, 16, 18), LEDGE(14, 22, 24), LEDGE(14, 27, 29),
    LEDGE(11, 7, 9), LEDGE(11, 13, 15), LEDGE(11, 19, 21), LEDGE(11, 25, 27),
    LEDGE(8, 4, 6), LEDGE(8, 10, 12), LEDGE(8, 16, 18), LEDGE(8, 22, 24), LEDGE(8, 28, 30),
    LEDGE(5, 7, 9), LEDGE(5, 13, 15), LEDGE(5, 19, 21), LEDGE(5, 25, 27),
    START(2, 16), EXIT(25, 4),
    KEY(11, 13), KEY(28, 13), KEY(4, 7), KEY(20, 10), KEY(14, 4), KEY(29, 7),
    GEM(17, 13), GEM(10, 7), GEM(19, 4), CHEST(7, 4),
    FOE(EN_GLOW_BAT, 13, 13), FOE(EN_GLOW_BAT, 19, 7), FOE(EN_WISP, 9, 5), FOE(EN_FISH, 15, 7),
    FOE(EN_PISTON, 24, 10), FOE(EN_FISH, 8, 7), FOE(EN_GLOW_BAT, 11, 2), FOE(EN_SPIDER, 23, 2), FOE(EN_WISP, 22, 11),
};

/* 17: stepping from raft to raft, and no waiting about */
static struct op const lv_ferries[] = {
    BLOCK(1, 15, 4, 16), PIT(5, 30),
    RAFT(16, 6, 6, 2), RAFT(16, 10, 14, 3), RAFT(16, 19, 19, 2), RAFT(16, 23, 23, 2),
    LEDGE(14, 26, 27), LEDGE(12, 28, 30), SLIDER(10, 15, 22, 4), LEDGE(10, 7, 10),
    LEDGE(8, 1, 4), SLIDER(6, 8, 14, 3), LEDGE(6, 18, 22), LEDGE(4, 27, 30),
    START(2, 14), EXIT(28, 3),
    KEY(12, 15), KEY(29, 11), KEY(9, 9), KEY(2, 7), KEY(20, 5),
    GEM(24, 15), GEM(17, 9), GEM(10, 4), CHEST(4, 7),
    FOE(EN_FISH, 8, 11), FOE(EN_GLOW_BAT, 18, 8), FOE(EN_FISH, 17, 12), FOE(EN_WISP, 9, 6),
    FOE(EN_SPITTER, 30, 11), FOE(EN_FISH, 25, 11), FOE(EN_GLOW_BAT, 18, 3), FOE(EN_BEETLE, 21, 5), FOE(EN_SPIDER, 14, 2),
};

/* 18: belts and crumbling ledges over a flood */
static struct op const lv_plumber[] = {
    TIDE(17, 13, 5, 3),
    BLOCK(1, 11, 3, 16),
    BELT_R(14, 6, 11), CRUMBLE(14, 15, 19), BELT_L(14, 23, 28),
    BELT_L(11, 7, 13), CRUMBLE(11, 17, 20), BELT_R(11, 24, 28),
    BELT_R(8, 10, 15), LEDGE(8, 21, 25),
    LEDGE(5, 4, 10), CRUMBLE(5, 15, 19), LEDGE(5, 23, 29),
    START(2, 10), EXIT(5, 4),
    KEY(9, 16), KEY(21, 16), KEY(26, 13), KEY(14, 7), KEY(28, 4),
    GEM(17, 13), GEM(12, 10), GEM(5, 16), CHEST(24, 4),
    FOE(EN_FISH, 13, 11), FOE(EN_BEETLE, 27, 10), FOE(EN_FISH, 22, 9), FOE(EN_PISTON, 22, 12),
    FOE(EN_CHARGER, 27, 4), FOE(EN_SPIDER, 12, 2), FOE(EN_GLOW_BAT, 17, 2), FOE(EN_BEETLE, 23, 7), FOE(EN_BOUNCER, 20, 16),
};

/* 19: the twist. It is flooded to the rafters until the plugs come out */
static struct op const lv_plug[] = {
    FLOOD(7),
    LEDGE(7, 1, 6), LEDGE(7, 10, 13), LEDGE(5, 17, 21), LEDGE(7, 25, 30),
    LEDGE(12, 2, 5), LEDGE(10, 8, 12), LEDGE(12, 15, 19), LEDGE(10, 22, 27),
    LEDGE(14, 8, 11), LEDGE(14, 13, 15), LEDGE(14, 23, 25),
    PLUG(29, 6, 12), PLUG(3, 11, 17),
    START(2, 6), EXIT(18, 4),
    KEY(12, 6), KEY(24, 9), KEY(17, 11), KEY(10, 16), KEY(27, 16),
    GEM(21, 4), GEM(10, 9), GEM(18, 16), CHEST(4, 16),
    FOE(EN_FISH, 8, 4), FOE(EN_GLOW_BAT, 14, 3), FOE(EN_FISH, 23, 4), FOE(EN_BEETLE, 25, 9),
    FOE(EN_BEETLE, 20, 16), FOE(EN_WISP, 8, 3), FOE(EN_BOUNCER, 16, 11), FOE(EN_SPIDER, 26, 2),
};

/* 20: tide, rafts, fish and one plug */
static struct op const lv_reservoir[] = {
    TIDE(16, 9, 5, 3),
    BLOCK(1, 9, 3, 16), BLOCK(14, 12, 17, 16), PIT(4, 13), PIT(18, 30),
    RAFT(16, 5, 5, 3), RAFT(16, 24, 24, 3),
    LEDGE(14, 9, 11), LEDGE(11, 9, 11), LEDGE(14, 20, 22), LEDGE(11, 20, 22),
    LEDGE(9, 7, 11), LEDGE(9, 20, 24), LEDGE(6, 12, 19), LEDGE(6, 26, 29), LEDGE(5, 2, 6),
    PLUG(15, 5, 13),
    START(2, 8), EXIT(2, 4),
    KEY(6, 15), KEY(15, 11), KEY(25, 15), KEY(10, 13), KEY(28, 5), KEY(21, 13),
    GEM(10, 8), GEM(23, 8), GEM(13, 5), CHEST(29, 5),
    FOE(EN_FISH, 13, 7), FOE(EN_GLOW_BAT, 22, 3), FOE(EN_FISH, 19, 7), FOE(EN_BEETLE, 21, 8),
    FOE(EN_SPIDER, 9, 2), FOE(EN_WISP, 24, 12), FOE(EN_SPITTER, 29, 5), FOE(EN_FISH, 28, 9),
    FOE(EN_CHARGER, 18, 5), FOE(EN_PISTON, 7, 12), FOE(EN_GLOW_BAT, 8, 5), FOE(EN_SPIDER, 25, 2),
};

/* `air` is how many seconds a full tank lasts, set from the route times tools/check-levels.sh prints. */
#define LV(map, title, hint, biome, air) {title, hint, biome, air, map, (int)(sizeof(map) / sizeof(map[0]))}

/* The campaign, in order: ten caverns to a world. Worlds 1 and 2 are finished; the caverns after them are earlier
   maps waiting to be reworked into their own worlds. */
static struct level const k_levels[] = {
    LV(lv_hearth, "The Vestibule of Mild Peril", "Grab the keys. Bottles top up air", BIOME_TIMBER_HEARTH, 42),
    LV(lv_loft, "Up the Ladder to Mrs Nuttall's", "Climb with Up and Down", BIOME_TIMBER_HEARTH, 37),
    LV(lv_crossing, "Is This the Bus to Wigan?", "Ride across the gap", BIOME_TIMBER_HEARTH, 30),
    LV(lv_stones, "Stepping Stones of the Lesser Gentry", "Mind the drop", BIOME_TIMBER_HEARTH, 30),
    LV(lv_dumbwaiter, "The Dumbwaiter Wants a Word", "Only the lifts go up", BIOME_TIMBER_HEARTH, 50),
    LV(lv_zigzag, "Left a Bit, Right a Bit, Left a Bit", "There and back again", BIOME_TIMBER_HEARTH, 56),
    LV(lv_stair, "Eighty-Six Steps and No Biscuit", "Up, over and down", BIOME_TIMBER_HEARTH, 30),
    LV(lv_towers, "Twinned With Another Tower", "Over the great wall", BIOME_TIMBER_HEARTH, 34),
    LV(lv_pulley, "Pulley Inspector Found in Marmalade", "Lifts, ladders, long drop", BIOME_TIMBER_HEARTH, 37),
    LV(lv_greathearth, "Tea With the Mayor's Spare Head", "Everything you've learned", BIOME_TIMBER_HEARTH, 53),

    LV(lv_paddling, "The Municipal Paddling Incident", "The water comes and goes", BIOME_FLOODED, 55),
    LV(lv_sinking, "Mrs Nuttall's Sinking Feeling", "Don't linger on a raft", BIOME_FLOODED, 45),
    LV(lv_chasm, "A Frankly Unnecessary Chasm", "Let the water lift you", BIOME_FLOODED, 62),
    LV(lv_bridges, "Bridge Awaiting Repair Since 1974", "Beat the tide across", BIOME_FLOODED, 48),
    LV(lv_lifthall, "Ministry of Going Up and Down", "Lifts dip when it floods", BIOME_FLOODED, 50),
    LV(lv_honeycomb, "The Bees Have Since Resigned", "Climb, and don't dawdle", BIOME_FLOODED, 50),
    LV(lv_ferries, "The 4.15 to Nowhere in Particular", "Raft to raft", BIOME_FLOODED, 82),
    LV(lv_plumber, "Plumber Last Seen in 1962", "Nothing holds still", BIOME_FLOODED, 48),
    LV(lv_plug, "Do Not Pull the Archbishop's Plug", "Pull it anyway", BIOME_FLOODED, 55),
    LV(lv_reservoir, "The Reservoir of Considerable Regret", "Everything, and wet", BIOME_FLOODED, 65),

    LV(lv_gallery, "Prickly Exhibition (Do Not Lick)", "Watch your feet", BIOME_FROST_LANTERN, 30),
    LV(lv_rails, "Rail Replacement Bus Not Provided", "No solid ground", BIOME_AMETHYST_GROTTO, 30),
    LV(lv_brokenladders, "Ladders Condemned by the Vicar", "Climb, leap, climb", BIOME_AMETHYST_GROTTO, 30),
    LV(lv_spire, "The Bishop's Crumbly Bit", "It all gives way", BIOME_AMETHYST_GROTTO, 27),
    LV(lv_works, "Sausage Rethinking Plant No. 2", "Nothing stands still", BIOME_CLOCKWORK_CELLAR, 30),
    LV(lv_heart, "Local Man Marries Cuckoo Clock", "Keep time or fall", BIOME_CLOCKWORK_CELLAR, 32),
    LV(lv_gauntlet, "Attack of the Livid Teasmades", "No room to hide", BIOME_CLOCKWORK_CELLAR, 54),
    LV(lv_abyss, "The Last Abyss Before Swindon", "The last of the deep", BIOME_CLOCKWORK_CELLAR, 61),
};
