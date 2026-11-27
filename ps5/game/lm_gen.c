/*
 * Lantern Miner: builds a cavern from its hand-drawn map. Only the chest is down to luck.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lm.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------- rng */

struct rng { uint32_t state; };

static uint32_t
hash_string(char const *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 16777619u;
    }
    return h;
}

static void
rng_seed(struct rng *r, char const *s)
{
    r->state = hash_string(s);
    if (r->state == 0)
        r->state = 1337;
}

static double
rng_next(struct rng *r)
{
    uint32_t t = (r->state += 0x6d2b79f5u);
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}

static int
rng_int(struct rng *r, int min, int max)
{
    return min + (int)lm_floor(rng_next(r) * (double)(max - min + 1));
}

static int
rng_pick(struct rng *r, int n)
{
    return (int)lm_floor(rng_next(r) * (double)n);
}

static bool
rng_chance(struct rng *r, double p)
{
    return rng_next(r) < p;
}

/* ------------------------------------------------------------------- maps */

enum { OP_FILL, OP_LADDER, OP_MOVER, OP_START, OP_EXIT, OP_KEY, OP_GEM, OP_CHEST, OP_FOE, OP_RAFT, OP_TIDE, OP_PLUG };
struct op { uint8_t kind, arg, c0, r0, c1, r1; };
struct level { char const *name, *hint; int biome, air; struct op const *ops; int n; };

#include "lm_levels.inc"

#define N_LEVELS ((int)(sizeof(k_levels) / sizeof(k_levels[0])))

int lm_level_count(void) { return N_LEVELS; }

/* ------------------------------------------------------------------- foes */

static bool
open_for_foe(struct lm_cavern const *cv, int c, int r)
{
    return c > 0 && c < GRID_COLS - 1 && r > 0 && r < GRID_ROWS - 1 && cv->tiles[r][c] == T_EMPTY;
}

static bool
footing(struct lm_cavern const *cv, int c, int r)
{
    int t = cv->tiles[r + 1][c];
    return open_for_foe(cv, c, r) &&
           (t == T_SOLID || t == T_ONE_WAY || t == T_CONVEYOR_LEFT || t == T_CONVEYOR_RIGHT || t == T_CRUMBLE);
}

static void
add_foe(struct lm_cavern *cv, int kind, int c, int r, struct rng *rng)
{
    if (cv->n_enemies >= LM_MAX_ENEMIES)
        return;
    int depth = cv->depth;
    double scale = lm_min(1.75, 1 + (depth - 1) * 0.04);
    int dir = rng_chance(rng, 0.5) ? 1 : -1;
    double phase = rng_next(rng) * LM_PI * 2;

    /* nastier kinds wait their turn: an easier foe takes the post until then */
    if (kind == EN_SPITTER && depth < 7) kind = EN_BEETLE;
    if (kind == EN_CHARGER && depth < 5) kind = EN_BEETLE;
    if (kind == EN_BOUNCER && depth < 3) kind = EN_BEETLE;
    if (kind == EN_WISP && depth < 4) kind = EN_GLOW_BAT;
    if (kind == EN_PISTON && depth < 3) kind = EN_GLOW_BAT;
    if (kind == EN_SPIDER && depth < 5) kind = EN_GLOW_BAT;

    struct lm_enemy_spawn e = {0};
    e.kind = kind;
    e.direction = dir;
    e.phase = phase;
    e.x = c * TILE_SIZE + 4;
    e.y = r * TILE_SIZE + 4;
    e.min_x = e.max_x = e.x;
    e.min_y = e.max_y = e.y;

    if (kind == EN_BEETLE || kind == EN_CHARGER || kind == EN_BOUNCER || kind == EN_SPITTER) {
        int lo = c, hi = c;
        while (lo > c - 6 && footing(cv, lo - 1, r)) lo--;
        while (hi < c + 6 && footing(cv, hi + 1, r)) hi++;
        e.min_x = lo * TILE_SIZE + (hi > lo ? 6 : 3);
        e.max_x = (hi + 1) * TILE_SIZE - (hi > lo ? 28 : 27);
        e.x = lm_clamp(c * TILE_SIZE + 4, e.min_x, e.max_x);
        e.y = r * TILE_SIZE + (kind == EN_BEETLE || kind == EN_CHARGER ? 10 : 8);
        e.min_y = e.y - 50;
        e.max_y = e.y;
        e.speed = (kind == EN_CHARGER ? 62 : kind == EN_BEETLE ? 58 : kind == EN_BOUNCER ? 46 : 0) * scale;
    } else if (kind == EN_GLOW_BAT) {
        int lo = c, hi = c;
        while (lo > c - 5 && open_for_foe(cv, lo - 1, r)) lo--;
        while (hi < c + 5 && open_for_foe(cv, hi + 1, r)) hi++;
        e.min_x = lo * TILE_SIZE;
        e.max_x = hi * TILE_SIZE + 6;
        e.min_y = e.y - 16;
        e.max_y = e.y + 16;
        e.speed = 66 * scale;
    } else if (kind == EN_PISTON) {
        int lo = r, hi = r;
        while (lo > r - 4 && lo > 2 && open_for_foe(cv, c, lo - 1)) lo--;
        while (hi < r + 4 && open_for_foe(cv, c, hi + 1)) hi++;
        e.x = e.min_x = e.max_x = c * TILE_SIZE + 3;
        e.min_y = lo * TILE_SIZE;
        e.max_y = hi * TILE_SIZE + 6;
        e.speed = 62 * scale;
    } else if (kind == EN_WISP) {
        e.x = c * TILE_SIZE;
        e.y = r * TILE_SIZE;
        e.min_x = e.x - 58;
        e.max_x = e.x + 58;
        e.min_y = e.y - 38;
        e.max_y = e.y + 38;
        e.speed = 1.35 * scale;
    } else if (kind == EN_FISH) { /* lurks under the water and leaps up to the row it was given */
        e.speed = 1;
    } else { /* spider: its thread reaches up to whatever is overhead, and down as far as is clear */
        int top = r, low = r;
        while (open_for_foe(cv, c, top - 1)) top--;
        while (low < r + 10 && open_for_foe(cv, c, low + 1)) low++;
        e.min_y = top * TILE_SIZE;
        e.max_y = low * TILE_SIZE + 8;
        e.speed = 230 * lm_min(1.3, scale);
    }
    cv->enemies[cv->n_enemies++] = e;
}

/* ------------------------------------------------------------------ build */

static void
add_collect(struct lm_cavern *cv, int kind, int powerup, int variant, double x, double y, double phase)
{
    if (cv->n_collect >= LM_MAX_COLLECT)
        return;
    struct lm_collectable *c = &cv->collect[cv->n_collect++];
    c->kind = kind;
    c->powerup = powerup;
    c->relic_variant = variant;
    c->x = x;
    c->y = y;
    c->base_y = y;
    c->collected = false;
    c->phase = phase;
}

static int
roll_chest(struct rng *rng)
{
    /* weights out of 100. 59 are good: shield 12, tea 12, spring 8, magnet 7, slow 7, treasure 11, a life 1, a
       continue 1. 41 are nasty: lead boots 11, muddle 10, air leak 11, something that follows you 9. */
    static int const weight[CHEST_KINDS] = {
        [PU_SPRING_BOOTS] = 8, [PU_LANTERN_SHIELD] = 12, [PU_TEA_THERMOS] = 12, [PU_STARLIGHT_MAGNET] = 7,
        [PU_CLOCKWORK_SLOW] = 7, [CHEST_JACKPOT] = 11, [CHEST_LIFE] = 1, [CHEST_CONTINUE] = 1,
        [CHEST_LEAD] = 11, [CHEST_MUDDLE] = 10, [CHEST_LEAK] = 11, [CHEST_GHOUL] = 9};
    int n = rng_pick(rng, 100);
    for (int i = 0; i < CHEST_KINDS; i++) {
        n -= weight[i];
        if (n < 0)
            return i;
    }
    return PU_TEA_THERMOS;
}

void
lm_build_level(int index, bool mirror, char const *seed, int depth, struct lm_cavern *cv)
{
    struct level const *lv = &k_levels[index];
    struct rng rng, luck;
    char key[160];
    int foes = 0, gems = 0;
    /* the foe budget is the difficulty curve: gentle for two caverns, four from the third, then a steady climb */
    static int const early[10] = {1, 2, 4, 4, 5, 5, 6, 7, 7, 8};
    int budget = depth <= 10 ? early[depth < 1 ? 0 : depth - 1] : (int)lm_min(12, 8 + lm_floor((depth - 10) * 0.4));

    snprintf(key, sizeof(key), "cavern::%d::%d", index, depth); /* the same for every run */
    rng_seed(&rng, key);
    snprintf(key, sizeof(key), "%s::chest::%d", seed, depth);
    rng_seed(&luck, key);
    memset(cv, 0, sizeof(*cv));
    cv->depth = depth;
    cv->biome = lv->biome;
    cv->air_secs = lv->air;
    snprintf(cv->name, sizeof(cv->name), "%s", lv->name);
    snprintf(cv->hint, sizeof(cv->hint), "%s", lv->hint);

    for (int c = 0; c < GRID_COLS; c++)
        cv->tiles[0][c] = cv->tiles[GRID_ROWS - 1][c] = T_SOLID;
    for (int r = 0; r < GRID_ROWS; r++)
        cv->tiles[r][0] = cv->tiles[r][GRID_COLS - 1] = T_SOLID;

#define COL(c) (mirror ? GRID_COLS - 1 - (c) : (c))
    /* the terrain first: foes read it to work out their patrols */
    for (int i = 0; i < lv->n; i++) {
        struct op const *o = &lv->ops[i];
        if (o->kind == OP_FILL) {
            int t = o->arg;
            if (mirror && t == T_CONVEYOR_LEFT) t = T_CONVEYOR_RIGHT;
            else if (mirror && t == T_CONVEYOR_RIGHT) t = T_CONVEYOR_LEFT;
            for (int r = o->r0; r <= o->r1; r++)
                for (int c = o->c0; c <= o->c1; c++)
                    cv->tiles[r][COL(c)] = (uint8_t)t;
        } else if (o->kind == OP_LADDER) {
            for (int r = o->r0; r <= o->r1; r++)
                cv->ladders[r][COL(o->c0)] = 1;
        }
    }

    for (int i = 0; i < lv->n; i++) {
        struct op const *o = &lv->ops[i];
        int c = COL(o->c0), r = o->r0;
        double x = c * TILE_SIZE + TILE_SIZE * 0.5, y = r * TILE_SIZE;
        switch (o->kind) {
        case OP_MOVER: {
            if (cv->n_movers >= LM_MAX_MOVERS)
                break;
            struct lm_mover *m = &cv->movers[cv->n_movers++];
            int w = o->arg;
            int a = mirror ? GRID_COLS - o->c0 - w : o->c0, b = mirror ? GRID_COLS - o->c1 - w : o->c1;
            m->width = w * TILE_SIZE;
            m->vertical = o->r0 != o->r1;
            m->min_x = (a < b ? a : b) * TILE_SIZE;
            m->max_x = (a < b ? b : a) * TILE_SIZE;
            m->min_y = o->r0 * TILE_SIZE;
            m->max_y = o->r1 * TILE_SIZE;
            m->speed = lm_min(96, 54 + (depth - 1) * 2.2);
            m->direction = cv->n_movers % 2 ? 1 : -1;
            if (mirror && !m->vertical)
                m->direction = -m->direction;
            m->x = m->direction > 0 ? m->min_x : m->max_x;
            m->y = m->vertical ? (m->direction > 0 ? m->min_y : m->max_y) : m->min_y;
            break;
        }
        case OP_RAFT: {
            if (cv->n_movers >= LM_MAX_MOVERS)
                break;
            struct lm_mover *m = &cv->movers[cv->n_movers++];
            int w = o->arg;
            int a = mirror ? GRID_COLS - o->c0 - w : o->c0, b = mirror ? GRID_COLS - o->c1 - w : o->c1;
            m->raft = true;
            m->width = w * TILE_SIZE;
            m->min_x = (a < b ? a : b) * TILE_SIZE;
            m->max_x = (a < b ? b : a) * TILE_SIZE;
            m->home_y = o->r0 * TILE_SIZE - 5;
            m->min_y = m->max_y = m->home_y;
            m->speed = a == b ? 0 : 46;
            m->direction = mirror ? -1 : 1;
            m->x = a * TILE_SIZE;
            m->y = m->home_y;
            break;
        }
        case OP_TIDE:
            cv->tide.on = true;
            cv->tide.low_y = o->c0 * TILE_SIZE + WATER_LIP;
            cv->tide.high_y = o->r0 * TILE_SIZE + WATER_LIP;
            cv->tide.low_secs = o->arg;
            cv->tide.high_secs = o->c1;
            break;
        case OP_PLUG:
            add_collect(cv, COL_PLUG, o->arg, 0, x, y + TILE_SIZE * 0.55, 0);
            break;
        case OP_START:
            cv->spawn.x = x;
            cv->spawn.y = y + TILE_SIZE - 26;
            break;
        case OP_EXIT: {
            int left = mirror ? GRID_COLS - 2 - o->c0 : o->c0;
            cv->portal.x = left * TILE_SIZE;
            cv->portal.y = (r - 1) * TILE_SIZE;
            cv->portal.col = left;
            cv->portal.row = r;
            break;
        }
        case OP_KEY:
            add_collect(cv, COL_KEY, 0, 0, x, y + TILE_SIZE * 0.42, rng_next(&rng) * LM_PI * 2);
            cv->keys_total++;
            break;
        case OP_GEM:
            add_collect(cv, COL_RELIC, 0, gems++ % 3, x, y + TILE_SIZE * 0.48, rng_next(&rng) * LM_PI * 2);
            break;
        case OP_CHEST:
            /* luck: a little over half the caverns have their chest */
            #ifdef LM_ALL_CHESTS
            if (true) {
#else
            if (depth >= 2 && rng_chance(&luck, 0.55)) {
#endif
                int prize = roll_chest(&luck);
#ifdef LM_ALL_CHESTS
                prize = CHEST_JACKPOT; /* the level checker tests routes, so no curses */
#endif
                add_collect(cv, COL_CHEST, prize, 0, x, y + TILE_SIZE * 0.6, rng_next(&luck) * LM_PI * 2);
            }
            break;
        case OP_FOE:
            if (foes < budget) {
                add_foe(cv, o->arg, c, r, &rng);
                foes++;
            }
            break;
        default:
            break;
        }
    }
#undef COL

    /* the gem furthest from the start is an air bottle instead, so topping up always means a detour */
    {
        struct lm_collectable *far = NULL;
        double best = -1;
        for (int i = 0; i < cv->n_collect; i++) {
            struct lm_collectable *c = &cv->collect[i];
            double dx = c->x - cv->spawn.x, dy = c->y - cv->spawn.y, d = dx * dx + dy * dy;
            if (c->kind == COL_RELIC && d > best) {
                best = d;
                far = c;
            }
        }
        if (far)
            far->kind = COL_AIR;
    }

    /* ambient hanging brass lanterns */
    static int const lantern_cols[5] = {5, 11, 16, 21, 26};
    for (int i = 0; i < 5; i++) {
        if (rng_chance(&rng, 0.8)) {
            int chain = rng_int(&rng, 16, 44);
            struct lm_lantern *l = &cv->lanterns[cv->n_lanterns++];
            l->x = lantern_cols[i] * TILE_SIZE + rng_int(&rng, -8, 8);
            l->y = TILE_SIZE + chain;
            l->sway_phase = rng_next(&rng) * LM_PI * 2;
            l->chain_length = chain;
        }
    }
}

void
lm_generate_cavern(char const *seed, int depth, struct lm_cavern *out)
{
    int i = depth < 1 ? 0 : depth > N_LEVELS ? N_LEVELS - 1 : depth - 1;
    lm_build_level(i, false, seed, depth, out);
}
