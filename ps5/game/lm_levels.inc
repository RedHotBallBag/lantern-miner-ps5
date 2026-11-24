/* The hand-built caverns. Included by lm_gen.c; the table at the bottom is the order they are played in.
**
** The grid is 32 x 18 tiles; the outer walls, ceiling and floor are added for you. Columns run 1..30, rows 1..16.
** Things the miner stands on are given by the row they occupy; items, foes, start and exit by the row they stand IN
** (one above the ledge). Reach: a jump climbs 3 rows (never 4, so 4 rows apart means "ladder or lift only") and
** clears a gap of about 5 tiles (so 8 or more means "moving platform only").
** Foes are listed most basic first: shallow caverns only use the first few.
** After editing a map, run tools/check-levels.sh: it plays every map and mirror to prove it can still be finished. */

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
    GEM(23, 12), GEM(12, 4), GEM(16, 16), CHEST(29, 4),
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
    FOE(EN_BEETLE, 20, 16), FOE(EN_BEETLE, 24, 7), FOE(EN_GLOW_BAT, 15, 11), FOE(EN_BEETLE, 9, 7), FOE(EN_WISP, 13, 11),
    FOE(EN_PISTON, 25, 12), FOE(EN_GLOW_BAT, 8, 3),
};

/* ------------------------------------------------------------- tier 2: bigger ideas */

static struct op const lv_chasm[] = {
    BLOCK(1, 12, 5, 16), BLOCK(14, 12, 17, 16), BLOCK(26, 12, 30, 16), PIT(6, 13), PIT(18, 25),
    SLIDER(12, 6, 11, 3), SLIDER(12, 18, 23, 3), SLIDER(7, 7, 21, 3),
    LEDGE(9, 15, 16), LEDGE(9, 2, 4), LEDGE(4, 25, 29),
    START(2, 11), EXIT(28, 11),
    KEY(3, 8), KEY(16, 8), KEY(10, 10), KEY(22, 10), KEY(26, 3),
    GEM(14, 11), GEM(12, 5), GEM(19, 5), CHEST(29, 3),
    FOE(EN_GLOW_BAT, 10, 4), FOE(EN_BEETLE, 16, 11), FOE(EN_GLOW_BAT, 21, 14), FOE(EN_BEETLE, 27, 11),
    FOE(EN_WISP, 21, 4), FOE(EN_SPITTER, 28, 3), FOE(EN_GLOW_BAT, 10, 14),
};

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

static struct op const lv_bridges[] = {
    BLOCK(1, 13, 4, 16), BLOCK(27, 13, 30, 16), PIT(5, 26),
    CRUMBLE(13, 6, 12), LEDGE(13, 14, 17), CRUMBLE(13, 19, 25),
    LEDGE(10, 9, 11), LEDGE(10, 20, 22), CRUMBLE(8, 13, 18), LEDGE(6, 6, 9), LEDGE(6, 22, 25),
    START(2, 12), EXIT(28, 12),
    KEY(15, 12), KEY(16, 7), KEY(7, 5), KEY(24, 5), KEY(10, 9),
    GEM(8, 12), GEM(23, 12), GEM(14, 5), CHEST(21, 9),
    FOE(EN_GLOW_BAT, 15, 10), FOE(EN_GLOW_BAT, 10, 3), FOE(EN_BEETLE, 16, 12), FOE(EN_WISP, 22, 14),
    FOE(EN_GLOW_BAT, 20, 3), FOE(EN_WISP, 9, 15), FOE(EN_SPITTER, 23, 5),
};

static struct op const lv_ferries[] = {
    BLOCK(1, 15, 4, 16), PIT(5, 30),
    SLIDER(14, 5, 24, 4), LEDGE(12, 28, 30), SLIDER(10, 6, 22, 4), LEDGE(8, 1, 4), SLIDER(6, 6, 23, 3),
    LEDGE(4, 27, 30),
    START(2, 14), EXIT(28, 3),
    KEY(16, 13), KEY(28, 11), KEY(15, 9), KEY(2, 7), KEY(15, 5),
    GEM(9, 12), GEM(22, 8), GEM(10, 4), CHEST(4, 7),
    FOE(EN_GLOW_BAT, 15, 12), FOE(EN_GLOW_BAT, 18, 8), FOE(EN_WISP, 9, 7), FOE(EN_GLOW_BAT, 18, 3),
    FOE(EN_SPITTER, 30, 11), FOE(EN_WISP, 22, 12), FOE(EN_GLOW_BAT, 9, 15),
};

/* ------------------------------------------------------------- tier 3: hard */

static struct op const lv_towers[] = {
    BLOCK(15, 7, 16, 16), LEDGE(14, 2, 6), LEDGE(11, 8, 13), LEDGE(7, 12, 14), LEDGE(7, 17, 19),
    LEDGE(11, 18, 23), LEDGE(14, 25, 29), LEDGE(4, 3, 9), LEDGE(4, 22, 28),
    LADDER(12, 7, 10), LADDER(19, 7, 10),
    START(2, 16), EXIT(25, 3),
    KEY(4, 3), KEY(15, 6), KEY(9, 10), KEY(27, 13), KEY(28, 16), KEY(11, 16),
    GEM(2, 13), GEM(20, 16), GEM(16, 3), CHEST(8, 3),
    FOE(EN_BEETLE, 22, 16), FOE(EN_BEETLE, 11, 10), FOE(EN_CHARGER, 22, 10), FOE(EN_GLOW_BAT, 14, 3),
    FOE(EN_SPITTER, 29, 13), FOE(EN_BOUNCER, 5, 3), FOE(EN_PISTON, 24, 9), FOE(EN_WISP, 7, 8), FOE(EN_STALKER, 27, 8),
};

static struct op const lv_lifthall[] = {
    BLOCK(1, 15, 4, 16), PIT(5, 30),
    BLOCK(9, 12, 10, 16), BLOCK(15, 8, 16, 16), BLOCK(21, 5, 22, 16), LEDGE(13, 28, 30),
    LIFT(6, 12, 15, 2), LIFT(12, 8, 12, 2), LIFT(18, 5, 8, 2), LIFT(25, 5, 13, 2),
    START(2, 14), EXIT(29, 12),
    KEY(9, 11), KEY(15, 7), KEY(21, 4), KEY(13, 5), KEY(28, 8),
    GEM(7, 9), GEM(19, 3), GEM(26, 3), CHEST(22, 4),
    FOE(EN_GLOW_BAT, 12, 4), FOE(EN_GLOW_BAT, 24, 14), FOE(EN_WISP, 19, 12), FOE(EN_PISTON, 24, 8),
    FOE(EN_SPITTER, 16, 7), FOE(EN_WISP, 7, 5), FOE(EN_PISTON, 14, 6), FOE(EN_GLOW_BAT, 17, 2), FOE(EN_STALKER, 3, 3),
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
    FOE(EN_SPITTER, 29, 7), FOE(EN_CHARGER, 11, 4), FOE(EN_PISTON, 24, 10), FOE(EN_WISP, 17, 13), FOE(EN_STALKER, 27, 3),
};

static struct op const lv_rails[] = {
    BLOCK(1, 12, 3, 16), PIT(4, 30),
    SLIDER(12, 4, 12, 3), LIFT(16, 6, 12, 2), SLIDER(6, 19, 25, 3), LIFT(28, 6, 15, 2), SLIDER(15, 18, 24, 3),
    LEDGE(4, 22, 23), LEDGE(9, 1, 2),
    START(2, 11), EXIT(22, 3),
    KEY(9, 10), KEY(16, 4), KEY(23, 13), KEY(29, 4), KEY(19, 13),
    GEM(6, 10), GEM(28, 11), GEM(20, 4), CHEST(1, 8),
    FOE(EN_GLOW_BAT, 10, 7), FOE(EN_GLOW_BAT, 22, 10), FOE(EN_WISP, 11, 15), FOE(EN_GLOW_BAT, 12, 3),
    FOE(EN_WISP, 22, 9), FOE(EN_PISTON, 26, 10), FOE(EN_GLOW_BAT, 8, 5), FOE(EN_STALKER, 28, 2),
};

static struct op const lv_honeycomb[] = {
    PIT(7, 30),
    LEDGE(14, 4, 6), LEDGE(14, 10, 12), LEDGE(14, 16, 18), LEDGE(14, 22, 24), LEDGE(14, 27, 29),
    LEDGE(11, 7, 9), LEDGE(11, 13, 15), LEDGE(11, 19, 21), LEDGE(11, 25, 27),
    LEDGE(8, 4, 6), LEDGE(8, 10, 12), LEDGE(8, 16, 18), LEDGE(8, 22, 24), LEDGE(8, 28, 30),
    LEDGE(5, 7, 9), LEDGE(5, 13, 15), LEDGE(5, 19, 21), LEDGE(5, 25, 27),
    START(2, 16), EXIT(25, 4),
    KEY(11, 13), KEY(28, 13), KEY(4, 7), KEY(20, 10), KEY(14, 4), KEY(29, 7),
    GEM(17, 13), GEM(10, 7), GEM(19, 4), CHEST(7, 4),
    FOE(EN_GLOW_BAT, 13, 13), FOE(EN_GLOW_BAT, 19, 7), FOE(EN_WISP, 9, 5), FOE(EN_PISTON, 24, 10),
    FOE(EN_PISTON, 15, 12), FOE(EN_WISP, 22, 11), FOE(EN_GLOW_BAT, 11, 2), FOE(EN_STALKER, 28, 3),
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
    FOE(EN_SPITTER, 29, 9), FOE(EN_GLOW_BAT, 9, 3), FOE(EN_WISP, 23, 8), FOE(EN_PISTON, 15, 9), FOE(EN_STALKER, 27, 3),
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
    FOE(EN_STALKER, 27, 2),
};

static struct op const lv_heart[] = {
    BLOCK(1, 15, 3, 16), BLOCK(28, 15, 30, 16), PIT(4, 27), BLOCK(14, 8, 17, 11),
    BELT_R(14, 6, 11), BELT_L(14, 20, 25), LEDGE(11, 2, 6), LEDGE(11, 25, 29),
    BELT_L(8, 5, 9), BELT_R(8, 22, 26), SLIDER(5, 6, 23, 3), SLIDER(15, 12, 17, 3),
    START(2, 14), EXIT(15, 7),
    KEY(2, 10), KEY(29, 10), KEY(7, 7), KEY(24, 7), KEY(15, 3),
    GEM(15, 14), GEM(8, 13), GEM(23, 13), CHEST(30, 14),
    FOE(EN_BEETLE, 22, 13), FOE(EN_GLOW_BAT, 10, 3), FOE(EN_BEETLE, 27, 10), FOE(EN_PISTON, 12, 10),
    FOE(EN_PISTON, 19, 10), FOE(EN_SPITTER, 29, 14), FOE(EN_WISP, 16, 13), FOE(EN_CHARGER, 24, 7), FOE(EN_STALKER, 26, 3),
};

static struct op const lv_spire[] = {
    BLOCK(1, 15, 4, 16), PIT(5, 30),
    CRUMBLE(14, 7, 10), CRUMBLE(14, 13, 16), LEDGE(12, 19, 23), CRUMBLE(10, 26, 29), LEDGE(8, 19, 22),
    CRUMBLE(8, 12, 15), LEDGE(6, 6, 9), CRUMBLE(4, 12, 18), LEDGE(4, 23, 27),
    START(2, 14), EXIT(25, 3),
    KEY(14, 13), KEY(28, 9), KEY(21, 7), KEY(7, 5), KEY(15, 3),
    GEM(9, 13), GEM(13, 7), GEM(22, 11), CHEST(20, 11),
    FOE(EN_GLOW_BAT, 11, 11), FOE(EN_WISP, 25, 6), FOE(EN_GLOW_BAT, 17, 10), FOE(EN_BEETLE, 8, 5),
    FOE(EN_SPITTER, 23, 11), FOE(EN_WISP, 10, 10), FOE(EN_PISTON, 24, 9), FOE(EN_GLOW_BAT, 20, 2), FOE(EN_STALKER, 3, 3),
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
    FOE(EN_STALKER, 28, 8),
};

/* ------------------------------------------------------------- World 1 additions */

static struct op const lv_stair[] = {
    BLOCK(8, 14, 11, 16), BLOCK(12, 11, 15, 16), BLOCK(16, 8, 19, 16), BLOCK(20, 11, 23, 16), BLOCK(24, 14, 26, 16),
    LEDGE(5, 2, 12), LEDGE(5, 22, 28), LADDER(12, 5, 10), LADDER(22, 5, 10), SPIKES(16, 28, 28),
    START(2, 16), EXIT(3, 4),
    KEY(5, 16), KEY(14, 10), KEY(18, 7), KEY(27, 4), KEY(30, 16), KEY(9, 4),
    GEM(10, 13), GEM(21, 10), GEM(17, 3), CHEST(25, 4),
    FOE(EN_BEETLE, 25, 13), FOE(EN_GLOW_BAT, 17, 2), FOE(EN_BEETLE, 26, 4), FOE(EN_GLOW_BAT, 6, 9),
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
    FOE(EN_BEETLE, 11, 4), FOE(EN_GLOW_BAT, 22, 2), FOE(EN_WISP, 21, 11), FOE(EN_CHARGER, 28, 4),
};

#define LV(map, title, hint, biome) {title, hint, biome, map, (int)(sizeof(map) / sizeof(map[0]))}

/* The campaign, in order: ten caverns to a world. World 1 is finished; the caverns after it are the earlier maps
   waiting to be reworked into their own worlds. */
static struct level const k_levels[] = {
    LV(lv_hearth, "Central Hearth", "Keys open the portal", BIOME_TIMBER_HEARTH),
    LV(lv_loft, "The Ladder Loft", "Climb with Up and Down", BIOME_TIMBER_HEARTH),
    LV(lv_crossing, "The First Crossing", "Ride across the gap", BIOME_TIMBER_HEARTH),
    LV(lv_stones, "Stepping Stones", "Mind the drop", BIOME_TIMBER_HEARTH),
    LV(lv_dumbwaiter, "The Dumbwaiter", "Only the lifts go up", BIOME_TIMBER_HEARTH),
    LV(lv_zigzag, "Zigzag Galleries", "There and back again", BIOME_TIMBER_HEARTH),
    LV(lv_stair, "The Miners' Stair", "Up, over and down", BIOME_TIMBER_HEARTH),
    LV(lv_towers, "The Twin Towers", "Over the great wall", BIOME_TIMBER_HEARTH),
    LV(lv_pulley, "The Pulley House", "Lifts, ladders, long drop", BIOME_TIMBER_HEARTH),
    LV(lv_greathearth, "The Great Hearth", "Everything you've learned", BIOME_TIMBER_HEARTH),

    LV(lv_bridges, "Rickety Bridges", "Don't stop moving", BIOME_GLOW_MOSS),
    LV(lv_ferries, "The Ferry Lines", "Hop from ferry to ferry", BIOME_GLOW_MOSS),
    LV(lv_chasm, "The Great Chasm", "Wait for your ride", BIOME_GLOW_MOSS),
    LV(lv_lifthall, "The Lift Hall", "Pillar to pillar", BIOME_GLOW_MOSS),
    LV(lv_honeycomb, "The Honeycomb", "Small steps, long fall", BIOME_GLOW_MOSS),
    LV(lv_gallery, "The Bramble Gallery", "Watch your feet", BIOME_FROST_LANTERN),
    LV(lv_rails, "The Brass Rails", "No solid ground", BIOME_AMETHYST_GROTTO),
    LV(lv_brokenladders, "The Broken Ladders", "Climb, leap, climb", BIOME_AMETHYST_GROTTO),
    LV(lv_spire, "The Crumbling Spire", "It all gives way", BIOME_AMETHYST_GROTTO),
    LV(lv_works, "The Conveyor Works", "Nothing stands still", BIOME_CLOCKWORK_CELLAR),
    LV(lv_heart, "The Clockwork Heart", "Keep time or fall", BIOME_CLOCKWORK_CELLAR),
    LV(lv_gauntlet, "The Gauntlet", "No room to hide", BIOME_CLOCKWORK_CELLAR),
    LV(lv_abyss, "The Grand Abyss", "The last of the deep", BIOME_CLOCKWORK_CELLAR),
};
