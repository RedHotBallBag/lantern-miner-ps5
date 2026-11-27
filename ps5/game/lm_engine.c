/*
 * Lantern Miner: physics, guardians, power-ups, the shop and the run itself.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ helpers */

double
lm_rand(struct lm_game *g)
{
    uint32_t x = g->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng_state = x ? x : 0x9e3779b9u;
    return (double)(g->rng_state >> 8) / 16777216.0;
}

void
lm_fmt_int(char *buf, size_t n, long v)
{
    char tmp[32];
    int len = 0, o = 0;
    bool neg = v < 0;
    unsigned long u = neg ? (unsigned long)(-v) : (unsigned long)v;
    do {
        tmp[len++] = (char)('0' + u % 10);
        u /= 10;
    } while (u && len < 30);
    if (neg && o < (int)n - 1) buf[o++] = '-';
    for (int i = len - 1; i >= 0 && o < (int)n - 1; i--) {
        buf[o++] = tmp[i];
        if (i > 0 && i % 3 == 0 && o < (int)n - 1)
            buf[o++] = ',';
    }
    buf[o] = 0;
}

static inline uint32_t
rgb_hex(uint32_t hex)
{
    return hex;
}

static void
rumble(struct lm_game *g, double strength, double secs)
{
    secs = lm_max(secs, 0.1); /* anything shorter barely spins the motors up */
    if (strength > g->rumble_strength)
        g->rumble_strength = strength;
    if (secs > g->rumble_time)
        g->rumble_time = secs;
}

static enum lm_tile
get_tile(struct lm_game const *g, int col, int row)
{
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS)
        return T_SOLID;
    return (enum lm_tile)g->cavern.tiles[row][col];
}

static void
spawn_burst(struct lm_game *g, double x, double y, int count, uint32_t color, double speed)
{
    for (int i = 0; i < count; i++) {
        if (g->n_particles > 140) {
            memmove(&g->particles[0], &g->particles[1], (size_t)(g->n_particles - 1) * sizeof(struct lm_particle));
            g->n_particles--;
        }
        double angle = (LM_TAU * i) / count + (lm_rand(g) - 0.5) * 0.5;
        double spd = speed * (0.45 + lm_rand(g) * 0.7);
        double life = 0.32 + lm_rand(g) * 0.3;
        struct lm_particle *p = &g->particles[g->n_particles++];
        p->x = x;
        p->y = y;
        p->vx = lm_cos(angle) * spd;
        p->vy = lm_sin(angle) * spd - 35;
        p->size = lm_rand(g) > 0.5 ? 4 : 3;
        p->color = rgb_hex(color);
        p->alpha = 1;
        p->life = life;
        p->max_life = life;
        p->gravity = 260;
    }
}

static void
spawn_float(struct lm_game *g, double x, double y, char const *text, uint32_t color)
{
    if (g->n_floats >= LM_MAX_FLOATS) {
        memmove(&g->floats[0], &g->floats[1], (size_t)(g->n_floats - 1) * sizeof(struct lm_float_text));
        g->n_floats--;
    }
    struct lm_float_text *f = &g->floats[g->n_floats++];
    f->x = lm_max(58, lm_min(CANVAS_W - 58, x));
    f->y = lm_max(36, y);
    f->vy = -34;
    snprintf(f->text, sizeof(f->text), "%s", text);
    f->color = color;
    f->life = 0.95;
    f->max_life = 0.95;
}

static void
clear_inputs(struct lm_game *g)
{
    g->in_left = g->in_right = g->in_up = g->in_down = g->in_jump = false;
    g->prev_jump = false;
}

static void
emit_game_over(struct lm_game *g, bool air);
static void set_banner(struct lm_game *g, char const *title, char const *sub, double secs);
static void lose_life(struct lm_game *g, bool air);

/* --------------------------------------------------------------------- water */

#define NO_WATER (CANVAS_H + 100.0)

double
lm_tide_at(struct lm_tide const *t, double clock, int *phase)
{
    int at = TIDE_LOW;
    double y = t->on ? t->low_y : NO_WATER;
    double swing = t->low_y - t->high_y;
    if (t->on && swing > 0) {
        double move = swing / TIDE_SPEED;
        double k = lm_fmod(clock, t->low_secs + TIDE_WARNING + move * 2 + t->high_secs);
        if (k < t->low_secs) {
            at = TIDE_LOW;
        } else if ((k -= t->low_secs) < TIDE_WARNING) {
            at = TIDE_WARN;
        } else if ((k -= TIDE_WARNING) < move) {
            at = TIDE_RISING;
            y = t->low_y - k * TIDE_SPEED;
        } else if ((k -= move) < t->high_secs) {
            at = TIDE_HIGH;
            y = t->high_y;
        } else {
            at = TIDE_FALLING;
            y = t->high_y + (k - t->high_secs) * TIDE_SPEED;
        }
    }
    if (phase)
        *phase = at;
    return y;
}

void
lm_reset_water(struct lm_game *g)
{
    struct lm_tide const *t = &g->cavern.tide;
    g->tide_clock = t->on ? lm_max(0, t->low_secs + TIDE_WARNING - TIDE_FIRST) : 0;
    g->drain_y = g->drain_to = 0;
    g->water_y = lm_tide_at(t, g->tide_clock, &g->tide_phase);
    for (int i = 0; i < g->n_movers; i++)
        if (g->movers[i].raft) /* afloat from the start */
            g->movers[i].y = lm_min(g->movers[i].home_y, g->water_y - RAFT_FREEBOARD);
}

/* ---------------------------------------------------------------------- load */

static void
create_player(struct lm_game *g)
{
    struct lm_cavern const *cv = &g->cavern;
    struct lm_player *p = &g->player;
    double px = cv->spawn.x - 10, py = cv->spawn.y;
    memset(p, 0, sizeof(*p));
    p->x = px;
    p->y = py;
    p->width = 20;
    p->height = 26;
    p->facing = cv->spawn.x < CANVAS_W * 0.5 ? 1 : -1;
    p->can_air_jump = g->perks[PERK_FEATHERSTEP] > 0;
    p->invuln = 0.9;
    p->squash_x = 1;
    p->squash_y = 1;
    p->rider = -1;
    p->fall_top = py;
    for (int i = 0; i < 4; i++) {
        p->scarf[i].x = px + 10;
        p->scarf[i].y = py + 11;
    }
}

static void
load_cavern(struct lm_game *g, int depth)
{
    /* the chest is rolled afresh every time, so it is never the same old one */
    char luck[64];
    snprintf(luck, sizeof(luck), "%s::%d", g->seed, (int)(lm_rand(g) * 1000000));
    lm_generate_cavern(luck, depth, &g->cavern);
    memset(g->power_time, 0, sizeof(g->power_time));
    if (g->chest_taken == depth)
        for (int i = 0; i < g->cavern.n_collect; i++)
            if (g->cavern.collect[i].kind == COL_CHEST)
                g->cavern.collect[i].collected = true;
    if (g->paid_depth != depth) {
        g->paid_depth = depth;
        g->paid = 0;
    }
    /* gems already banked stay gone; keys come back, as the door needs them */
    for (int i = 0; i < g->cavern.n_collect; i++)
        if (g->cavern.collect[i].kind == COL_RELIC && (g->paid >> i & 1))
            g->cavern.collect[i].collected = true;
    create_player(g);
    if (g->powers[PU_LANTERN_SHIELD] > 0) {
        g->player.has_shield = true; /* the Lantern Shield renews for a few caverns, then runs out */
        g->powers[PU_LANTERN_SHIELD] -= 1;
        /* a Ward Charm stays packed for later rather than being wasted on top of it */
    } else if (g->next_shield) {
        g->player.has_shield = true;
        g->next_shield = false;
    }
    g->n_enemies = g->cavern.n_enemies;
    for (int i = 0; i < g->n_enemies; i++) {
        struct lm_enemy *e = &g->enemies[i];
        struct lm_enemy_spawn const *sp = &g->cavern.enemies[i];
        memset(e, 0, sizeof(*e));
        e->s = *sp;
        e->vx = (sp->kind == EN_BEETLE || sp->kind == EN_GLOW_BAT || sp->kind == EN_CHARGER) ? sp->speed * sp->direction : 0;
        e->vy = sp->kind == EN_PISTON ? sp->speed * sp->direction : 0;
        e->width = (sp->kind == EN_BEETLE || sp->kind == EN_CHARGER) ? 24 : 22;
        e->height = (sp->kind == EN_BEETLE || sp->kind == EN_CHARGER) ? 20 : 22;
        e->timer = sp->kind == EN_SPITTER ? 1.2 + lm_fmod(sp->phase, 1.4) : 0;
        e->anchor_x = sp->x;
        e->anchor_y = sp->y;
        if (sp->kind == EN_FISH) {
            e->s.y = NO_WATER; /* out of sight until it leaps */
            e->timer = 1.4 + lm_fmod(sp->phase, 2.0);
        }
    }
    g->n_proj = 0;
    memset(g->crumble, 0, sizeof(g->crumble));
    g->n_movers = g->cavern.n_movers;
    memcpy(g->movers, g->cavern.movers, sizeof(g->movers));
    lm_reset_water(g);
    g->n_particles = 0;
    g->n_floats = 0;
    g->keys_collected = 0;
    g->relics_this_cavern = 0;
    g->portal_unlocked = false;
    g->transition_locked = false;
    g->pending_advance = 0;
    g->air = MAX_AIR;
    g->hitstop = 0;
    g->shake_intensity = 0;
    g->hit_flash = 0;
}

static void
init_motes(struct lm_game *g)
{
    for (int i = 0; i < LM_MOTES; i++) {
        struct lm_mote *m = &g->motes[i];
        m->x = lm_rand(g) * CANVAS_W;
        m->y = lm_rand(g) * CANVAS_H;
        m->vx = (lm_rand(g) - 0.5) * 10;
        m->vy = -4 - lm_rand(g) * 7;
        m->radius = lm_rand(g) > 0.65 ? 2.5 : 1.5;
        m->alpha = 0.25 + lm_rand(g) * 0.45;
        m->phase = lm_rand(g) * LM_TAU;
    }
}

void
lm_init(struct lm_game *g, uint32_t rng_seed)
{
    memset(g, 0, sizeof(*g));
    g->rng_state = rng_seed ? rng_seed : 0x1234567u;
    snprintf(g->seed, sizeof(g->seed), "HEARTH-101");
    g->depth = 1;
    g->hearts = g->max_hearts = BASE_HEARTS;
    g->lives = START_LIVES;
    g->air = MAX_AIR;
    g->shake_enabled = true;
    lm_generate_cavern(g->seed, 1, &g->cavern);
    create_player(g);
    init_motes(g);
}

void
lm_set_records(struct lm_game *g, int high_score, int best_depth)
{
    g->high_score = high_score;
    g->best_depth = best_depth;
}

void
lm_start_run(struct lm_game *g, char const *seed)
{
    snprintf(g->seed, sizeof(g->seed), "%s", seed && *seed ? seed : "HEARTH-101");
    g->depth = 1;
    g->score = 0;
    g->relic_points = 0;
    g->max_hearts = BASE_HEARTS;
    g->hearts = BASE_HEARTS;
    g->lives = START_LIVES;
    g->continues = g->continues_bought = 0;
    g->hurt_depth = 0;
    g->lives_bought = 0;
    g->world_score = 0;
    g->flawless = true;
    g->flawless_award = false;
    g->down = false;
    g->respawn_timer = 0;
    g->run_start_high = g->high_score; /* records as they stood when this run began */
    g->run_start_depth = g->best_depth;
    g->total_relics_run = 0;
    g->total_keys_run = 0;
    memset(g->perks, 0, sizeof(g->perks));
    memset(g->powers, 0, sizeof(g->powers));
    memset(g->power_time, 0, sizeof(g->power_time));
    g->next_shield = false;
    g->chest_taken = 0;
    g->paid_depth = 0;
    g->paid = 0;
    g->paused = false;
    g->pending_advance = 0;
    g->banner.active = false;
    g->events = 0;
    clear_inputs(g);
    g->practice = false;
    load_cavern(g, 1);
    set_banner(g, g->cavern.name, g->cavern.hint, 1.8);
    g->running = true;
    lm_audio_set_music_mode(MUSIC_CAVERN, 1.0);
}

void
lm_start_practice(struct lm_game *g, int depth, bool stocked)
{
    lm_start_run(g, "TESTING");
    depth = depth < 1 ? 1 : depth > lm_level_count() ? lm_level_count() : depth;
    g->practice = true;
    g->lives = MAX_LIVES;
    g->flawless = false;
    if (stocked) {
        g->relic_points = 20000;
        g->continues = MAX_CONTINUES;
        g->powers[PU_SPRING_BOOTS] = g->powers[PU_STARLIGHT_MAGNET] = g->powers[PU_CLOCKWORK_SLOW] = 1;
        g->powers[PU_TEA_THERMOS] = 3;
    }
    g->depth = depth;
    load_cavern(g, depth);
    set_banner(g, g->cavern.name, g->cavern.hint, 1.8);
}

void
lm_set_paused(struct lm_game *g, bool paused)
{
    g->paused = paused;
    if (paused)
        clear_inputs(g);
}

void
lm_set_input(struct lm_game *g, bool left, bool right, bool up, bool down, bool jump)
{
    if (g->paused || !g->running) {
        clear_inputs(g);
        return;
    }
    g->in_left = left;
    g->in_right = right;
    g->in_up = up;
    g->in_down = down;
    if (jump && !g->in_jump)
        g->player.jump_buffer = JUMP_BUFFER_TIME;
    g->in_jump = jump;
}

int
lm_cavern_until_shop(struct lm_game const *g)
{
    int rem = g->depth % SHOP_INTERVAL;
    return rem == 0 ? 0 : SHOP_INTERVAL - rem;
}

/* --------------------------------------------------------------------- shop */

static bool
power_is_timed(int pu)
{
    return pu == PU_SPRING_BOOTS || pu == PU_STARLIGHT_MAGNET || pu == PU_CLOCKWORK_SLOW;
}

static bool
item_is_timed(int item)
{
    return item >= SHOP_SPRING_BOOTS && item <= SHOP_CLOCKWORK_SLOW && power_is_timed(item - SHOP_SPRING_BOOTS);
}

bool
lm_use_power(struct lm_game *g, int pu)
{
    struct lm_player *p = &g->player;
    if (!g->running || g->paused || g->respawn_timer > 0 || g->pending_advance > 0 || g->transition_locked)
        return false;
    if (!power_is_timed(pu) || g->powers[pu] <= 0 || g->power_time[pu] > 0)
        return false;
    g->powers[pu] = 0;
    g->power_time[pu] = lm_powerups[pu].duration;
    lm_audio_powerup();
    spawn_burst(g, p->x + p->width * 0.5, p->y + p->height * 0.5, 16, lm_powerups[pu].color, 145);
    spawn_float(g, p->x + p->width * 0.5, p->y - 52, lm_powerups[pu].name, lm_powerups[pu].color); /* clear of the time bars */
    rumble(g, 0.3, 0.1);
    return true;
}

int
lm_shop_cost(struct lm_game const *g, int item)
{
    int cost = lm_shop[item].cost;
    if (item == SHOP_CONTINUE)
        cost += cost / 2 * g->continues_bought;
    if (item == SHOP_EXTRA_LIFE)
        cost += LIFE_PRICE_STEP * g->lives_bought;
    if (item != SHOP_EXTRA_LIFE && item != SHOP_CONTINUE)
        cost = cost * (100 - lm_shop_discount(g)) / 100;
    return (cost + 5) / 10 * 10; /* tidy to the nearest 10 */
}

/* Old Barnaby warms to a regular: 5% off for every stall visited after the first, up to 30%. */
int
lm_shop_discount(struct lm_game const *g)
{
    int stall = g->depth / SHOP_INTERVAL; /* 1 at the first stall (depth 5) */
    int d = (stall - 1) * 5;
    return d < 0 ? 0 : d > 30 ? 30 : d;
}

/* The stall number at which an item starts being sold. */
int
lm_shop_unlock_at(int item)
{
    /* the first stall sells the basics, and each one after brings something new */
    switch (item) {
    case SHOP_SPRING_BOOTS: case SHOP_LANTERN_SHIELD: case SHOP_CONVEYOR_CLEATS: case SHOP_GEM_RESONANCE: case SHOP_CONTINUE:
        return 2;
    case SHOP_STARLIGHT_MAGNET: case SHOP_BRASS_LANTERN: case SHOP_FEATHERSTEP:
        return 3;
    case SHOP_CLOCKWORK_SLOW: case SHOP_EXTRA_HEARTH: case SHOP_CHRONOMETER:
        return 4;
    default:
        return 1;
    }
}

void
lm_shop_item_status(struct lm_game const *g, int item, struct lm_shop_status *out)
{
    struct lm_shop_def const *it = &lm_shop[item];
    char a[24], b[24];
    int cost = lm_shop_cost(g, item);
    memset(out, 0, sizeof(*out));
    if (g->depth / SHOP_INTERVAL < lm_shop_unlock_at(item)) {
        snprintf(out->owned, sizeof(out->owned), "Locked");
        snprintf(out->status, sizeof(out->status), "Stall %d unlocks", lm_shop_unlock_at(item));
        return;
    }
    if (item == SHOP_CONTINUE) {
        snprintf(out->owned, sizeof(out->owned), "%d held", g->continues);
        if (g->continues >= MAX_CONTINUES) {
            snprintf(out->status, sizeof(out->status), "Max Held");
            out->is_owned = true;
            return;
        }
    } else if (item == SHOP_KETTLE_BREW) {
        if (g->hearts >= g->max_hearts) {
            snprintf(out->status, sizeof(out->status), "Full Hearts");
            snprintf(out->owned, sizeof(out->owned), "%d/%d", g->hearts, g->max_hearts);
            return;
        }
        snprintf(out->owned, sizeof(out->owned), "%d/%d Hearts", g->hearts, g->max_hearts);
    } else if (item == SHOP_LANTERN_WARD) {
        if (g->next_shield) {
            snprintf(out->status, sizeof(out->status), "Packed");
            snprintf(out->owned, sizeof(out->owned), "Ready");
            out->is_owned = true;
            return;
        }
        snprintf(out->owned, sizeof(out->owned), "1 Cavern");
    } else if (item == SHOP_EXTRA_LIFE) {
        snprintf(out->owned, sizeof(out->owned), "%d lives", g->lives);
        if (g->lives >= MAX_LIVES) {
            snprintf(out->status, sizeof(out->status), "Max Lives");
            out->is_owned = true;
            return;
        }
    } else if (item_is_timed(item)) {
        if (g->powers[item - SHOP_SPRING_BOOTS] > 0) {
            snprintf(out->owned, sizeof(out->owned), "Carried");
            snprintf(out->status, sizeof(out->status), "Ready to use");
            out->is_owned = true;
            return;
        }
        snprintf(out->owned, sizeof(out->owned), "%ds", (int)lm_powerups[item - SHOP_SPRING_BOOTS].duration);
    } else {
        bool power = item >= SHOP_SPRING_BOOTS && item <= SHOP_CLOCKWORK_SLOW;
        int owned = it->perk >= 0 ? g->perks[it->perk] : power ? g->powers[item - SHOP_SPRING_BOOTS] : 0;
        if (it->max_stacks > 0 && owned >= it->max_stacks) {
            snprintf(out->status, sizeof(out->status), it->perk >= 0 ? "Owned" : "Active");
            if (item == SHOP_LANTERN_SHIELD)
                snprintf(out->owned, sizeof(out->owned), "%d left", owned);
            else if (it->max_stacks > 1)
                snprintf(out->owned, sizeof(out->owned), "%d/%d", owned, it->max_stacks);
            else
                snprintf(out->owned, sizeof(out->owned), "Yours");
            out->is_owned = true;
            return;
        }
        if (it->max_stacks > 1)
            snprintf(out->owned, sizeof(out->owned), "%d/%d", owned, it->max_stacks);
        else if (it->perk >= 0)
            snprintf(out->owned, sizeof(out->owned), "Whole run");
        else
            snprintf(out->owned, sizeof(out->owned), "3 Caverns");
    }
    if (g->relic_points >= cost) {
        lm_fmt_int(a, sizeof(a), cost);
        snprintf(out->status, sizeof(out->status), "Buy \xc2\xb7 %s pts", a);
        out->can_buy = true;
    } else {
        lm_fmt_int(b, sizeof(b), cost - g->relic_points);
        snprintf(out->status, sizeof(out->status), "Need %s more", b);
    }
}

bool
lm_shop_purchase(struct lm_game *g, int item)
{
    struct lm_shop_def const *it = &lm_shop[item];
    int cost = lm_shop_cost(g, item);
    if (g->relic_points < cost || g->depth / SHOP_INTERVAL < lm_shop_unlock_at(item))
        return false;
    if (item == SHOP_CONTINUE) {
        if (g->continues >= MAX_CONTINUES)
            return false;
        g->relic_points -= cost;
        g->continues += 1;
        g->continues_bought += 1;
    } else if (item == SHOP_KETTLE_BREW) {
        if (g->hearts >= g->max_hearts)
            return false;
        g->relic_points -= cost;
        g->hearts = g->hearts + 1 < g->max_hearts ? g->hearts + 1 : g->max_hearts;
    } else if (item == SHOP_LANTERN_WARD) {
        if (g->next_shield)
            return false;
        g->relic_points -= cost;
        g->next_shield = true;
    } else if (item == SHOP_EXTRA_LIFE) {
        if (g->lives >= MAX_LIVES)
            return false;
        g->relic_points -= cost;
        g->lives += 1;
        g->lives_bought += 1;
    } else if (item >= SHOP_SPRING_BOOTS && item <= SHOP_CLOCKWORK_SLOW) {
        int pu = item - SHOP_SPRING_BOOTS;
        if (g->powers[pu] >= it->max_stacks)
            return false;
        g->relic_points -= cost;
        if (pu == PU_LANTERN_SHIELD)
            g->powers[pu] = SHIELD_CAVERNS; /* shields the next few caverns */
        else
            g->powers[pu] += 1;
    } else if (it->perk >= 0) {
        int owned = g->perks[it->perk];
        if (it->max_stacks > 0 && owned >= it->max_stacks)
            return false;
        g->relic_points -= cost;
        g->perks[it->perk] = owned + 1;
        if (it->perk == PERK_EXTRA_HEARTH) {
            g->max_hearts = g->max_hearts + 1 < 5 ? g->max_hearts + 1 : 5;
            g->hearts = g->max_hearts;
        }
    }
    lm_audio_shop_buy();
    return true;
}

static void
set_banner(struct lm_game *g, char const *title, char const *sub, double secs)
{
    snprintf(g->banner.title, sizeof(g->banner.title), "%s", title);
    snprintf(g->banner.subtitle, sizeof(g->banner.subtitle), "%s", sub);
    g->banner.remaining = g->banner.duration = secs;
    g->banner.active = true;
}

int
lm_world_of(int depth)
{
    return (depth - 1) / WORLD_SIZE + 1;
}

/* Loads cavern `depth` with its name across the screen, noting the start of a new world. */
static void
enter_cavern(struct lm_game *g, int depth)
{
    char t[128], s[200];
    if ((depth - 1) % WORLD_SIZE == 0 && depth != g->depth) {
        g->world_score = g->score;
        g->flawless = true;
    }
    g->depth = depth;
    load_cavern(g, depth);
    snprintf(t, sizeof(t), "Cavern %02d \xc2\xb7 %s", depth, g->cavern.name);
    if ((depth - 1) % WORLD_SIZE == 0)
        snprintf(s, sizeof(s), "World %d \xc2\xb7 %s", lm_world_of(depth), g->cavern.hint);
    else
        snprintf(s, sizeof(s), "%s \xc2\xb7 Gather %d Keys", g->cavern.hint, g->cavern.keys_total);
    set_banner(g, t, s, 1.5);
}

void
lm_leave_shop(struct lm_game *g)
{
    g->paused = false;
    g->flawless_award = false;
    clear_inputs(g);
    enter_cavern(g, g->depth + 1);
    lm_audio_set_music_mode(MUSIC_CAVERN, 1 + lm_min(0.24, (g->depth - 1) * 0.02));
}

/* ------------------------------------------------------------------ physics */

static void
resolve_horizontal(struct lm_game *g, struct lm_player *p, double total_vx)
{
    int top_row = (int)lm_floor((p->y + 2) / TILE_SIZE);
    int bot_row = (int)lm_floor((p->y + p->height - 2) / TILE_SIZE);
    if (total_vx > 0) {
        int right_col = (int)lm_floor((p->x + p->width) / TILE_SIZE);
        for (int r = top_row; r <= bot_row; r++) {
            if (get_tile(g, right_col, r) == T_SOLID) {
                p->x = right_col * TILE_SIZE - p->width - 0.01;
                p->vx = 0;
                break;
            }
        }
    } else if (total_vx < 0) {
        int left_col = (int)lm_floor(p->x / TILE_SIZE);
        for (int r = top_row; r <= bot_row; r++) {
            if (get_tile(g, left_col, r) == T_SOLID) {
                p->x = (left_col + 1) * TILE_SIZE + 0.01;
                p->vx = 0;
                break;
            }
        }
    }
}

static void
trigger_crumble(struct lm_game *g, int col, int row)
{
    struct lm_crumble *c = &g->crumble[row][col];
    if (c->state == CR_SHAKING || c->state == CR_BROKEN)
        return;
    double depth_tighten = lm_max(0.62, 1 - (g->depth - 1) * 0.025);
    double duration = CRUMBLE_SHAKE_TIME * depth_tighten * lm_pow(1.75, g->perks[PERK_CRUMBLE_LIGHTFOOT]);
    c->state = CR_SHAKING;
    c->timer = duration;
    c->max_shake = duration;
}

static void
resolve_vertical(struct lm_game *g, struct lm_player *p, double prev_bottom)
{
    int left_col = (int)lm_floor((p->x + 2) / TILE_SIZE);
    int right_col = (int)lm_floor((p->x + p->width - 2) / TILE_SIZE);

    if (p->vy < 0) {
        int top_row = (int)lm_floor(p->y / TILE_SIZE);
        for (int c = left_col; c <= right_col; c++) {
            if (get_tile(g, c, top_row) == T_SOLID) {
                p->y = (top_row + 1) * TILE_SIZE + 0.01;
                p->vy = 0;
                break;
            }
        }
    } else {
        int foot_row = (int)lm_floor((p->y + p->height) / TILE_SIZE);
        for (int c = left_col; c <= right_col; c++) {
            enum lm_tile tile = get_tile(g, c, foot_row);
            if (tile == T_EMPTY || tile == T_BRAMBLE || tile == T_PIT)
                continue;
            if (tile == T_CRUMBLE && c >= 0 && c < GRID_COLS && foot_row >= 0 && foot_row < GRID_ROWS &&
                g->crumble[foot_row][c].state == CR_BROKEN)
                continue;
            double tile_top = foot_row * TILE_SIZE;
            if (tile == T_SOLID || prev_bottom <= tile_top + 7) {
                p->y = tile_top - p->height;
                p->vy = 0;
                p->grounded = true;
                if (tile == T_CONVEYOR_LEFT) p->on_conveyor = -1;
                else if (tile == T_CONVEYOR_RIGHT) p->on_conveyor = 1;
                else if (tile == T_CRUMBLE) trigger_crumble(g, c, foot_row);
                /* standing across several crumble blocks sets them all off */
                for (int c2 = c + 1; c2 <= right_col; c2++)
                    if (get_tile(g, c2, foot_row) == T_CRUMBLE)
                        trigger_crumble(g, c2, foot_row);
                break;
            }
        }
    }
}

static bool
damage_player(struct lm_game *g, double source_x, double source_y)
{
    struct lm_player *p = &g->player;
    double px = p->x + p->width / 2, py = p->y + p->height / 2;
    int knock = px < source_x ? -1 : 1;
    (void)source_y;

    p->climbing = false; /* a hit knocks you off a ladder */
    g->hurt_depth = g->depth;

    if (p->has_shield) {
        p->has_shield = false;
        p->invuln = 1.25;
        p->vx = knock * 170;
        p->vy = -240;
        g->hitstop = 0.05;
        g->shake_intensity = 6;
        lm_audio_hurt(true);
        spawn_burst(g, px, py, 16, 0xf5a623, 165);
        spawn_float(g, px, p->y - 8, "WARD SAVED!", 0xf5a623);
        rumble(g, 0.45, 0.12);
        return true;
    }

    g->hearts = g->hearts - 1 > 0 ? g->hearts - 1 : 0;
    p->invuln = INVULN_DURATION;
    p->vx = knock * 190;
    p->vy = -290;
    p->squash_x = 1.25;
    p->squash_y = 0.78;
    g->hitstop = 0.055;
    g->shake_intensity = 8.5;
    g->hit_flash = 1;
    lm_audio_hurt(false);
    spawn_burst(g, px, py, 14, 0xf43fa5, 145);
    spawn_float(g, px, p->y - 8, "OUCH!", 0xf43fa5);
    rumble(g, 0.9, 0.22);
    if (g->hearts <= 0)
        lose_life(g, false);
    return false;
}

/* Water costs a heart (or the ward) and throws the miner back up to land somewhere dry, or not. A raft keeps the
   feet dry until it has gone right under. */
static bool
check_pit(struct lm_game *g, struct lm_player *p)
{
    double feet = p->y + p->height, cx = p->x + p->width * 0.5;
    double depth = 0;
    if (p->rider >= 0 && g->movers[p->rider].raft) {
        if (g->movers[p->rider].sink < 1)
            return false;
    } else if (feet - 4 > g->water_y) {
        depth = feet - g->water_y;
    } else if (get_tile(g, (int)lm_floor(cx / TILE_SIZE), (int)lm_floor((feet - 4) / TILE_SIZE)) != T_PIT) {
        return false;
    }
    if (p->vy < 0)
        return true; /* already on the way out */
    double vx = p->vx;
    spawn_burst(g, cx, p->y + p->height, 18, lm_biomes[g->cavern.biome].accent_spec, 170);
    lm_audio_splash();
    if (p->bouncing || p->invuln <= 0)
        damage_player(g, cx, p->y + p->height);
    if (g->respawn_timer > 0 || g->down || (g->events & EV_GAME_OVER))
        return true;
    p->vx = vx; /* no sideways shove: the way out is the player's to steer */
    /* from deep down the throw is harder, so it always clears the surface */
    p->vy = -lm_min(760, lm_max(-PIT_BOUNCE_VELOCITY, lm_sqrt(2 * GRAVITY * (depth + 120))));
    p->bouncing = p->thrown = true;
    p->grounded = false;
    p->climbing = false;
    p->rider = -1;
    p->coyote = 0;
    p->has_used_double_jump = false;
    p->fall_top = p->y;
    p->invuln = lm_max(p->invuln, INVULN_DURATION);
    return true;
}

static void
check_hazards(struct lm_game *g, struct lm_player *p)
{
    if (check_pit(g, p) || p->invuln > 0)
        return;
    int left_col = (int)lm_floor((p->x + 4) / TILE_SIZE);
    int right_col = (int)lm_floor((p->x + p->width - 4) / TILE_SIZE);
    int foot_row = (int)lm_floor((p->y + p->height - 4) / TILE_SIZE);
    for (int c = left_col; c <= right_col; c++) {
        if (get_tile(g, c, foot_row) == T_BRAMBLE) {
            damage_player(g, c * TILE_SIZE + TILE_SIZE * 0.5, foot_row * TILE_SIZE + TILE_SIZE);
            return;
        }
    }
}

/* Clockwork Slow: guardians and conveyors run at three quarters speed. Lifts and platforms are left alone. */
static double
power_slow(struct lm_game const *g)
{
    return g->power_time[PU_CLOCKWORK_SLOW] > 0 ? 0.75 : 1.0;
}

static void
update_scarf(struct lm_game const *g, struct lm_player *p, double dt)
{
    p->scarf[0].x = p->x + p->width * 0.5 - p->facing * 4;
    p->scarf[0].y = p->y + 11;
    for (int i = 1; i < 4; i++) {
        double tx = p->scarf[i - 1].x - p->facing * 4.2;
        double ty = p->scarf[i - 1].y + 0.8 + lm_sin(g->elapsed * 9 + i) * 1.1;
        p->scarf[i].x += (tx - p->scarf[i].x) * lm_min(1, dt * 18);
        p->scarf[i].y += (ty - p->scarf[i].y) * lm_min(1, dt * 18);
    }
}

/* ------------------------------------------------- ladders and platforms */

static bool
ladder_at(struct lm_game const *g, int col, double y)
{
    int row = (int)lm_floor(y / TILE_SIZE);
    return col >= 0 && col < GRID_COLS && row >= 0 && row < GRID_ROWS && g->cavern.ladders[row][col];
}

static void
update_movers(struct lm_game *g, double dt)
{
    for (int i = 0; i < g->n_movers; i++) {
        struct lm_mover *m = &g->movers[i];
        if (m->raft) {
            double was_x = m->x, was_y = m->y;
            if (g->player.rider == i) {
                m->linger += dt;
                if (m->linger > RAFT_GRACE)
                    m->sink = lm_min(1, m->sink + dt / RAFT_SINK_TIME);
            } else {
                m->linger = 0;
                m->sink = lm_max(0, m->sink - dt * 1.6);
            }
            if (m->speed > 0) {
                m->x += m->direction * m->speed * dt;
                if (m->x <= m->min_x) {
                    m->x = m->min_x;
                    m->direction = 1;
                } else if (m->x >= m->max_x) {
                    m->x = m->max_x;
                    m->direction = -1;
                }
            }
            m->y = lm_min(m->home_y, g->water_y - RAFT_FREEBOARD) + m->sink * RAFT_DEPTH;
            m->dx = m->x - was_x;
            m->dy = m->y - was_y;
            continue;
        }
        double *pos = m->vertical ? &m->y : &m->x;
        double lo = m->vertical ? m->min_y : m->min_x, hi = m->vertical ? m->max_y : m->max_x;
        double old = *pos;
        *pos += m->direction * m->speed * dt;
        if (*pos <= lo) {
            *pos = lo;
            m->direction = 1;
        } else if (*pos >= hi) {
            *pos = hi;
            m->direction = -1;
        }
        m->dx = m->vertical ? 0 : *pos - old;
        m->dy = m->vertical ? *pos - old : 0;
    }
}

/* Moving platforms are one-way: they only catch feet that were above them. */
static void
land_on_movers(struct lm_game *g, struct lm_player *p, double prev_bottom)
{
    if (p->vy < 0)
        return;
    for (int i = 0; i < g->n_movers; i++) {
        struct lm_mover const *m = &g->movers[i];
        if (p->x + p->width - 2 <= m->x || p->x + 2 >= m->x + m->width)
            continue;
        if (prev_bottom <= m->y - m->dy + 7 && p->y + p->height >= m->y) {
            p->y = m->y - p->height;
            p->vy = 0;
            p->grounded = true;
            p->on_conveyor = 0;
            p->rider = i;
            return;
        }
    }
}

/* Grabs, climbs and leaves ladders. Returns true while the ladder is in charge of the miner this step. */
static bool
update_climb(struct lm_game *g, struct lm_player *p, double dt)
{
    double pcx = p->x + p->width * 0.5;
    int col = (int)lm_floor(pcx / TILE_SIZE);
    double centre = col * TILE_SIZE + TILE_SIZE * 0.5;
    double foot = p->y + p->height;
    int dir = (g->in_down ? 1 : 0) - (g->in_up ? 1 : 0);

    if (!p->climbing) {
        if (dir == 0 || p->jump_buffer > 0 || p->vy < -150 || lm_fabs(pcx - centre) > 12)
            return false;
        /* up: a ladder beside the body. down: also a ladder under the ledge being stood on */
        bool beside = ladder_at(g, col, foot - 2);
        bool under = dir > 0 && p->grounded && ladder_at(g, col, foot + 2);
        if (!under && !(beside && (dir < 0 || !p->grounded)))
            return false;
        p->climbing = true;
    }

    if (p->jump_buffer > 0) { /* hop off */
        p->climbing = false;
        p->vy = JUMP_VELOCITY * 0.85;
        p->jump_buffer = 0;
        lm_audio_jump(false);
        return false;
    }
    if ((dir == 0 && (g->in_left || g->in_right)) || (!ladder_at(g, col, foot - 2) && !ladder_at(g, col, foot + 2))) {
        p->climbing = false;
        return false;
    }

    p->x += (centre - p->width * 0.5 - p->x) * lm_min(1, dt * 18);
    p->vx = 0;
    p->vy = dir * CLIMB_SPEED;
    p->grounded = false;
    p->rider = -1;
    p->on_conveyor = 0;
    p->coyote = 0;
    p->has_used_double_jump = false;

    double next = foot + p->vy * dt;
    bool footing = true;
    if (dir < 0 && !ladder_at(g, col, next - 0.01)) {
        /* over the top: step onto the ledge the ladder hangs from */
        next = lm_floor((next - 0.01) / TILE_SIZE + 1) * TILE_SIZE;
        p->climbing = false;
    } else if (dir > 0 && !ladder_at(g, col, next)) {
        int row = (int)lm_floor(next / TILE_SIZE);
        footing = false;
        for (int c = (int)lm_floor((p->x + 2) / TILE_SIZE); c <= (int)lm_floor((p->x + p->width - 2) / TILE_SIZE); c++) {
            enum lm_tile t = get_tile(g, c, row);
            if (t != T_EMPTY && t != T_BRAMBLE && t != T_PIT && !(t == T_CRUMBLE && g->crumble[row][c].state == CR_BROKEN))
                footing = true;
        }
        if (footing)
            next = row * TILE_SIZE; /* reached the floor below */
        p->climbing = false; /* otherwise the ladder stops in mid-air and the miner drops off it */
    }
    if (!p->climbing) {
        p->vy = 0;
        p->grounded = footing;
    }
    p->y = next - p->height;
    if (dir != 0)
        p->walk_phase += dt * 0.7;
    return true;
}

static void
update_player(struct lm_game *g, double dt)
{
    struct lm_player *p = &g->player;
    int cleats = g->perks[PERK_CONVEYOR_CLEATS];
    int feather = g->perks[PERK_FEATHERSTEP];

    p->invuln = lm_max(0, p->invuln - dt);
    if (p->invuln <= 0)
        p->bouncing = false;
    p->lead = lm_max(0, p->lead - dt);
    p->muddle = lm_max(0, p->muddle - dt);
    /* a running power only counts down while you are playing, and waits for the cavern banner */
    for (int pu = 0; pu < PU_COUNT; pu++) {
        double left = g->power_time[pu];
        if (left <= 0 || g->banner.active)
            continue;
        g->power_time[pu] = lm_max(0, left - dt);
        if (left <= POWER_WARNING + 1 && (int)lm_ceil(g->power_time[pu]) != (int)lm_ceil(left))
            lm_audio_ui_move(); /* tick, tick, gone */
    }
    p->spring = g->power_time[PU_SPRING_BOOTS];
    p->magnet = g->power_time[PU_STARLIGHT_MAGNET];
    p->slow = g->power_time[PU_CLOCKWORK_SLOW];

    p->squash_x += (1 - p->squash_x) * lm_min(1, dt * 14);
    p->squash_y += (1 - p->squash_y) * lm_min(1, dt * 14);

    /* ride the platform underfoot */
    if (p->rider >= 0 && p->rider < g->n_movers) {
        struct lm_mover const *m = &g->movers[p->rider];
        p->x += m->dx;
        p->y = m->y - p->height;
        resolve_horizontal(g, p, m->dx);
    }

    int move_dir = 0;
    if (g->in_left) move_dir -= 1;
    if (g->in_right) move_dir += 1;
    if (p->muddle > 0)
        move_dir = -move_dir;
    if (move_dir != 0)
        p->facing = move_dir > 0 ? 1 : -1;

    if (update_climb(g, p, dt)) {
        p->fall_top = p->y;
        g->prev_jump = g->in_jump;
        update_scarf(g, p, dt);
        check_hazards(g, p);
        return;
    }

    double max_speed = MOVE_SPEED * (1 + cleats * 0.12) * (p->lead > 0 ? LEAD_SPEED : 1);
    double target_vx = move_dir * max_speed;
    double accel = p->grounded ? MOVE_ACCEL_GROUND : MOVE_ACCEL_AIR;
    if (p->vx < target_vx)
        p->vx = lm_min(target_vx, p->vx + accel * dt);
    else if (p->vx > target_vx)
        p->vx = lm_max(target_vx, p->vx - accel * dt);

    double conveyor_vx = 0;
    if (p->grounded && p->on_conveyor != 0) {
        double slow_factor = power_slow(g);
        double cleat_factor = lm_max(0.2, 1 - cleats * 0.55);
        double depth_boost = 1 + lm_min(0.6, (g->depth - 1) * 0.04);
        conveyor_vx = p->on_conveyor * CONVEYOR_PUSH_SPEED * depth_boost * slow_factor * cleat_factor;
    }

    double coyote_cap = COYOTE_TIME + feather * 0.05;
    if (p->grounded) {
        p->coyote = coyote_cap;
        p->has_used_double_jump = false;
    } else {
        p->coyote = lm_max(0, p->coyote - dt);
    }
    p->jump_buffer = lm_max(0, p->jump_buffer - dt);

    bool can_air_hop = p->spring > 0 || feather > 0;
    if (p->jump_buffer > 0) {
        if (p->coyote > 0) {
            p->vy = p->spring > 0 ? SPRING_JUMP_VELOCITY : JUMP_VELOCITY;
            p->grounded = false;
            p->coyote = 0;
            p->jump_buffer = 0;
            p->squash_x = 0.78;
            p->squash_y = 1.24;
            lm_audio_jump(false);
            spawn_burst(g, p->x + p->width / 2, p->y + p->height, 6, 0xe8d5b7, 75);
        } else if (can_air_hop && !p->has_used_double_jump) {
            p->vy = p->spring > 0 ? DOUBLE_JUMP_VELOCITY * 1.08 : DOUBLE_JUMP_VELOCITY * 0.9;
            p->has_used_double_jump = true;
            p->thrown = false;
            p->fall_top = p->y; /* a flutter in mid-air breaks the fall */
            p->jump_buffer = 0;
            p->squash_x = 0.76;
            p->squash_y = 1.26;
            lm_audio_jump(true);
            spawn_burst(g, p->x + p->width / 2, p->y + p->height, 8, 0x2ce87d, 95);
        }
    }

    if (p->vy >= 0)
        p->thrown = false;
    if (g->prev_jump && !g->in_jump && p->vy < -150 && !p->thrown)
        p->vy *= JUMP_CUT_MULTIPLIER;
    g->prev_jump = g->in_jump;

    p->vy = lm_min(MAX_FALL_SPEED, p->vy + GRAVITY * dt);

    double total_vx = p->vx + conveyor_vx;
    p->x += total_vx * dt;
    resolve_horizontal(g, p, total_vx);

    double prev_bottom = p->y + p->height;
    bool was_grounded = p->grounded;
    double impact_vy = p->vy;
    p->y += p->vy * dt;
    p->grounded = false;
    p->on_conveyor = 0;
    p->rider = -1;
    resolve_vertical(g, p, prev_bottom);
    land_on_movers(g, p, prev_bottom);

    /* too far to fall: hearts and wards do not help */
    if (p->grounded && p->y - p->fall_top > MAX_SAFE_FALL) {
        g->hearts = 0;
        g->shake_intensity = 10;
        g->hit_flash = 1;
        spawn_float(g, p->x + p->width * 0.5, p->y - 8, "SPLAT!", 0xf43fa5);
        lose_life(g, false);
        if (g->respawn_timer > 0)
            snprintf(g->banner.title, sizeof(g->banner.title), "Too Far to Fall!");
        return;
    }
    p->fall_top = p->grounded ? p->y : lm_min(p->fall_top, p->y);

    if (!was_grounded && p->grounded && impact_vy > 130) {
        double intensity = lm_min(1, impact_vy / MAX_FALL_SPEED);
        p->squash_x = 1 + intensity * 0.24;
        p->squash_y = 1 - intensity * 0.2;
        lm_audio_land(intensity);
        spawn_burst(g, p->x + p->width / 2, p->y + p->height, 5, 0xd6c3a5, 55);
    }

    if (p->grounded && lm_fabs(p->vx) > 15)
        p->walk_phase += dt * (lm_fabs(p->vx) / MOVE_SPEED);

    update_scarf(g, p, dt);
    check_hazards(g, p);
}

static void
update_crumbles(struct lm_game *g, double dt)
{
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            struct lm_crumble *cs = &g->crumble[r][c];
            if (cs->state == CR_SHAKING) {
                cs->timer -= dt;
                if (cs->timer <= 0) {
                    cs->state = CR_BROKEN;
                    cs->timer = CRUMBLE_RESPAWN_TIME;
                    lm_audio_crumble();
                    spawn_burst(g, c * TILE_SIZE + TILE_SIZE * 0.5, r * TILE_SIZE + 6, 8, 0xbfa07e, 85);
                }
            } else if (cs->state == CR_BROKEN) {
                cs->timer -= dt;
                if (cs->timer <= 0) {
                    cs->state = CR_INTACT;
                    spawn_burst(g, c * TILE_SIZE + TILE_SIZE * 0.5, r * TILE_SIZE + 6, 4, 0xf5d076, 35);
                }
            }
        }
    }
}

static void
update_enemies(struct lm_game *g, double dt)
{
    double chrono = g->perks[PERK_CHRONOMETER] > 0 ? 0.8 : 1;
    double slow_mult = power_slow(g) * chrono;
    double step_dt = dt * slow_mult;
    double pcx = g->player.x + g->player.width * 0.5;
    double pcy = g->player.y + g->player.height * 0.5;

    for (int i = 0; i < g->n_enemies; i++) {
        struct lm_enemy *e = &g->enemies[i];
        struct lm_enemy_spawn *s = &e->s;
        if (e->stun > 0) {
            e->stun -= dt;
            continue;
        }

        if (s->kind == EN_STALKER && e->cooldown > 0) { /* still taking shape: harmless */
            e->cooldown -= dt;
            continue;
        }

        switch (s->kind) {
        case EN_BEETLE:
            if (e->telegraph > 0) {
                e->telegraph -= step_dt;
                continue;
            }
            s->x += s->direction * s->speed * step_dt;
            if (s->x <= s->min_x) {
                s->x = s->min_x;
                s->direction = 1;
                e->telegraph = 0.2;
            } else if (s->x >= s->max_x) {
                s->x = s->max_x;
                s->direction = -1;
                e->telegraph = 0.2;
            }
            break;
        case EN_CHARGER:
            if (e->cooldown > 0)
                e->cooldown -= step_dt;
            if (e->telegraph > 0) {
                e->telegraph -= step_dt;
                if (e->telegraph <= 0) {
                    e->is_charging = true;
                    lm_audio_spark();
                }
            } else if (e->is_charging) {
                s->x += s->direction * s->speed * 3.1 * step_dt;
                if (s->x <= s->min_x) {
                    s->x = s->min_x;
                    s->direction = 1;
                    e->is_charging = false;
                    e->cooldown = 1.1;
                } else if (s->x >= s->max_x) {
                    s->x = s->max_x;
                    s->direction = -1;
                    e->is_charging = false;
                    e->cooldown = 1.1;
                }
            } else {
                bool same_band = lm_fabs(pcy - (s->y + e->height * 0.5)) < 26;
                double dist_x = pcx - (s->x + e->width * 0.5);
                if (same_band && lm_fabs(dist_x) < 260 && e->cooldown <= 0) {
                    s->direction = dist_x >= 0 ? 1 : -1;
                    e->telegraph = 0.42;
                } else {
                    s->x += s->direction * s->speed * step_dt;
                    if (s->x <= s->min_x) {
                        s->x = s->min_x;
                        s->direction = 1;
                    } else if (s->x >= s->max_x) {
                        s->x = s->max_x;
                        s->direction = -1;
                    }
                }
            }
            break;
        case EN_SPITTER: {
            double fire_interval = lm_max(1.8, 3.1 - (g->depth - 7) * 0.08);
            e->timer -= step_dt;
            if (e->timer <= 0.48 && e->timer > 0)
                e->telegraph = e->timer;
            if (e->timer <= 0) {
                e->timer = fire_interval;
                e->telegraph = 0;
                int dir = pcx >= s->x + e->width * 0.5 ? 1 : -1;
                double proj_speed = lm_min(240, 145 + (g->depth - 7) * 9);
                if (g->n_proj < LM_MAX_PROJ) {
                    struct lm_projectile *pr = &g->proj[g->n_proj++];
                    pr->x = s->x + e->width * 0.5 + dir * 10;
                    pr->y = s->y + e->height * 0.45;
                    pr->vx = dir * proj_speed;
                    pr->vy = 0;
                    pr->radius = 5;
                    pr->color = 0xf43fa5;
                    pr->life = 4.5;
                }
                lm_audio_spark();
            }
            break;
        }
        case EN_STALKER: {
            double ex = s->x + e->width * 0.5, ey = s->y + e->height * 0.5;
            double dx = pcx - ex, dy = pcy - ey;
            double dist = lm_hypot(dx, dy);
            if (dist > 4) {
                s->x += (dx / dist) * s->speed * step_dt;
                s->y += (dy / dist) * s->speed * step_dt;
            }
            break;
        }
        case EN_FISH: /* ripples, leaps in an arc to the height the map set, and drops back */
            if (e->is_charging) {
                e->vy += FISH_GRAVITY * step_dt;
                s->y += e->vy * step_dt;
                if (s->y >= e->vx && e->vy > 0) { /* vx holds the surface it left from */
                    e->is_charging = false;
                    spawn_burst(g, s->x + e->width * 0.5, e->vx, 8, lm_biomes[g->cavern.biome].accent_spec, 90);
                    s->y = NO_WATER;
                    e->timer = 2.1;
                }
            } else {
                double base = g->water_y < CANVAS_H - 40 ? g->water_y : NO_WATER;
                int col = (int)lm_floor((s->x + e->width * 0.5) / TILE_SIZE);
                if (base == NO_WATER && get_tile(g, col, GRID_ROWS - 2) == T_PIT)
                    base = (GRID_ROWS - 2) * TILE_SIZE + WATER_LIP;
                if (base == NO_WATER) { /* high and dry: it waits for the water to come back */
                    e->telegraph = 0;
                    e->timer = lm_max(e->timer, FISH_WARNING);
                    continue;
                }
                e->vx = base;
                e->timer -= step_dt;
                e->telegraph = e->timer <= FISH_WARNING ? lm_max(0.01, e->timer) : 0;
                if (e->timer <= 0) {
                    double rise = lm_max(70, base - e->anchor_y);
                    e->is_charging = true;
                    e->telegraph = 0;
                    e->vy = -lm_sqrt(2 * FISH_GRAVITY * rise);
                    s->y = base;
                    spawn_burst(g, s->x + e->width * 0.5, base, 10, lm_biomes[g->cavern.biome].accent_spec, 110);
                }
            }
            break;
        case EN_SPIDER: /* waits on its thread, twitches, drops, sits, then climbs home */
            if (e->cooldown > 0)
                e->cooldown -= step_dt;
            if (e->is_charging) {
                s->y += s->speed * step_dt;
                if (s->y >= s->max_y) {
                    s->y = s->max_y;
                    e->is_charging = false;
                    e->timer = 0.55;
                }
            } else if (e->timer > 0) {
                e->timer -= step_dt;
            } else if (s->y > e->anchor_y) {
                s->y = lm_max(e->anchor_y, s->y - 75 * step_dt);
                if (s->y <= e->anchor_y)
                    e->cooldown = 1.2;
            } else if (e->telegraph > 0) {
                e->telegraph -= step_dt;
                if (e->telegraph <= 0) {
                    e->is_charging = true;
                    lm_audio_spark();
                }
            } else if (e->cooldown <= 0 && lm_fabs(pcx - (s->x + e->width * 0.5)) < 48 && pcy > s->y + 10 &&
                       pcy < s->max_y + 60) {
                e->telegraph = 0.45;
            }
            break;
        case EN_BOUNCER:
            e->timer += step_dt;
            if (!e->is_jumping) {
                if (e->timer > 0.68)
                    e->telegraph = 0.25;
                if (e->timer >= 0.96) {
                    e->is_jumping = true;
                    e->telegraph = 0;
                    e->timer = 0;
                    e->vy = -295;
                    e->vx = s->direction * s->speed;
                }
            } else {
                e->vy += 880 * step_dt;
                s->x += e->vx * step_dt;
                s->y += e->vy * step_dt;
                if (s->x <= s->min_x) {
                    s->x = s->min_x;
                    s->direction = 1;
                    e->vx = lm_fabs(e->vx);
                } else if (s->x >= s->max_x) {
                    s->x = s->max_x;
                    s->direction = -1;
                    e->vx = -lm_fabs(e->vx);
                }
                if (s->y >= s->max_y) {
                    s->y = s->max_y;
                    e->vy = 0;
                    e->is_jumping = false;
                }
            }
            break;
        case EN_GLOW_BAT:
            e->timer += step_dt;
            s->x += s->direction * s->speed * step_dt;
            if (s->x <= s->min_x) {
                s->x = s->min_x;
                s->direction = 1;
            } else if (s->x >= s->max_x) {
                s->x = s->max_x;
                s->direction = -1;
            }
            s->y = e->anchor_y + lm_sin(e->timer * 3.6 + s->phase) * 15;
            break;
        case EN_PISTON:
            if (e->telegraph > 0) {
                e->telegraph -= step_dt;
                continue;
            }
            s->y += s->direction * s->speed * step_dt;
            if (s->y <= s->min_y) {
                s->y = s->min_y;
                s->direction = 1;
                e->telegraph = 0.26;
            } else if (s->y >= s->max_y) {
                s->y = s->max_y;
                s->direction = -1;
                e->telegraph = 0.26;
            }
            break;
        case EN_WISP:
        default:
            e->timer += step_dt * s->speed;
            s->x = e->anchor_x + lm_cos(e->timer + s->phase) * 52;
            s->y = e->anchor_y + lm_sin(e->timer * 1.5 + s->phase) * 30;
            break;
        }

        if (g->player.invuln <= 0) {
            double pad = 4;
            struct lm_player *p = &g->player;
            bool overlap = p->x + pad < s->x + e->width - pad && p->x + p->width - pad > s->x + pad &&
                           p->y + pad < s->y + e->height - pad && p->y + p->height - pad > s->y + pad;
            if (overlap) {
                bool handled = damage_player(g, s->x + e->width / 2, s->y + e->height / 2);
                if (handled) {
                    e->stun = 2.0;
                    e->is_charging = false;
                }
            }
        }
    }
}

static void
update_projectiles(struct lm_game *g, double dt)
{
    double chrono = g->perks[PERK_CHRONOMETER] > 0 ? 0.8 : 1;
    double step_dt = dt * (power_slow(g) * chrono);
    double pcx = g->player.x + g->player.width * 0.5;
    double pcy = g->player.y + g->player.height * 0.5;

    for (int i = g->n_proj - 1; i >= 0; i--) {
        struct lm_projectile *pr = &g->proj[i];
        pr->life -= step_dt;
        pr->x += pr->vx * step_dt;
        pr->y += pr->vy * step_dt;
        int col = (int)lm_floor(pr->x / TILE_SIZE), row = (int)lm_floor(pr->y / TILE_SIZE);
        if (pr->life <= 0 || get_tile(g, col, row) == T_SOLID) {
            spawn_burst(g, pr->x, pr->y, 4, pr->color, 55);
            g->proj[i] = g->proj[--g->n_proj];
            continue;
        }
        if (g->player.invuln <= 0) {
            double dist = lm_hypot(pr->x - pcx, pr->y - pcy);
            if (dist < pr->radius + 9) {
                damage_player(g, pr->x, pr->y);
                g->proj[i] = g->proj[--g->n_proj];
            }
        }
    }
}

/* A cursed chest's guardian: it rises in the far top corner and, after a moment's warning, follows the miner
   through walls for the rest of the cavern. */
static void
release_ghoul(struct lm_game *g)
{
    if (g->n_enemies >= LM_MAX_ENEMIES)
        return;
    struct lm_enemy *e = &g->enemies[g->n_enemies++];
    memset(e, 0, sizeof(*e));
    e->s.kind = EN_STALKER;
    e->s.x = g->player.x < CANVAS_W * 0.5 ? CANVAS_W - 2.0 * TILE_SIZE : TILE_SIZE;
    e->s.y = TILE_SIZE * 1.5;
    e->s.min_x = e->s.min_y = TILE_SIZE;
    e->s.max_x = CANVAS_W - 2.0 * TILE_SIZE;
    e->s.max_y = CANVAS_H - 2.0 * TILE_SIZE;
    e->s.speed = 36 + lm_min(26, g->depth * 1.2); /* about a stalker's pace, far slower than the miner */
    e->s.direction = 1;
    e->width = e->height = 22;
    e->anchor_x = e->s.x;
    e->anchor_y = e->s.y;
    e->cooldown = 2.0;
}

static void
open_chest(struct lm_game *g, struct lm_collectable const *item)
{
    struct lm_player *p = &g->player;
    int prize = item->powerup;
    char buf[48];
    uint32_t color = 0xffe347;
    g->chest_taken = g->depth;
    if (prize == CHEST_LIFE && g->lives >= MAX_LIVES)
        prize = CHEST_JACKPOT;
    if (prize == CHEST_CONTINUE && g->continues >= MAX_CONTINUES)
        prize = CHEST_JACKPOT;
    if (prize == PU_TEA_THERMOS && g->powers[PU_TEA_THERMOS] >= 3)
        prize = CHEST_JACKPOT;
    if (prize < PU_COUNT && power_is_timed(prize) && g->powers[prize] > 0)
        prize = CHEST_JACKPOT; /* already carrying one */
    if (prize == PU_LANTERN_SHIELD && p->has_shield)
        prize = CHEST_JACKPOT;
    if (prize >= CHEST_LEAD) {
        color = 0xb45cff;
        if (prize == CHEST_LEAD) {
            p->lead = LEAD_TIME;
            snprintf(buf, sizeof(buf), "CURSED! LEAD BOOTS");
        } else if (prize == CHEST_MUDDLE) {
            p->muddle = MUDDLE_TIME;
            snprintf(buf, sizeof(buf), "CURSED! LEFT IS RIGHT");
        } else if (prize == CHEST_LEAK) {
            g->air = lm_max(5, g->air - LEAK_AIR);
            snprintf(buf, sizeof(buf), "CURSED! AIR LEAK");
        } else {
            release_ghoul(g);
            snprintf(buf, sizeof(buf), "CURSED! SOMETHING IS COMING");
        }
        lm_audio_hurt(false);
        g->hit_flash = 0.6;
        g->shake_intensity = 6;
    } else if (prize == CHEST_LIFE) {
        g->lives += 1;
        color = 0xf43fa5;
        snprintf(buf, sizeof(buf), "SPARE LANTERN! +1 LIFE");
    } else if (prize == CHEST_CONTINUE) {
        g->continues += 1;
        color = 0x2ce87d;
        snprintf(buf, sizeof(buf), "RARE FIND! +1 CONTINUE");
    } else if (prize == CHEST_JACKPOT) {
        int pts = 600 + g->depth * 40;
        g->relic_points += pts;
        g->score += pts;
        snprintf(buf, sizeof(buf), "TREASURE! +%d PTS", pts);
    } else {
        color = lm_powerups[prize].color;
        if (prize == PU_LANTERN_SHIELD) {
            p->has_shield = true;
        } else {
            g->powers[prize] += 1; /* flasks pile up; the rest wait for their button */
        }
        snprintf(buf, sizeof(buf), "%s!", lm_powerups[prize].name);
    }
    if (prize < CHEST_LEAD)
        lm_audio_powerup();
    g->hitstop = 0.035;
    spawn_burst(g, item->x, item->y, 22, color, 160);
    spawn_float(g, item->x, item->y - 18, buf, color);
    rumble(g, 0.5, 0.2);
}

static void
check_collectables(struct lm_game *g, double dt)
{
    struct lm_player *p = &g->player;
    double pcx = p->x + p->width * 0.5, pcy = p->y + p->height * 0.5;
    int reson = g->perks[PERK_GEM_RESONANCE];
    double score_mult = 1 + reson * 0.35;
    double magnet_radius = p->magnet > 0 ? 155 : reson > 0 ? 60 + reson * 24 : 0;
    char buf[48];

    for (int i = 0; i < g->cavern.n_collect; i++) {
        struct lm_collectable *item = &g->cavern.collect[i];
        if (item->collected)
            continue;
        double dx = pcx - item->x, dy = pcy - item->y;
        double dist = lm_hypot(dx, dy);

        if (magnet_radius > 0 && (item->kind == COL_KEY || item->kind == COL_RELIC) && dist < magnet_radius && dist > 1) {
            double pull = p->magnet > 0 ? 275 : 135;
            item->x += (dx / dist) * pull * dt;
            item->y += (dy / dist) * pull * dt;
        }

        if (dist < 22) {
            bool paid = g->paid >> i & 1; /* a key picked up again after a retry */
            g->paid |= 1u << i;
            item->collected = true;
            if (item->kind == COL_KEY) {
                g->keys_collected += 1;
                lm_audio_key(g->keys_collected - 1);
                spawn_burst(g, item->x, item->y, 14, 0xf43fa5, 135);
                if (!paid) {
                    g->total_keys_run += 1;
                    int pts = (int)lm_round(200 * score_mult);
                    g->score += pts;
                    snprintf(buf, sizeof(buf), "+%d", pts);
                    spawn_float(g, item->x, item->y - 10, buf, 0xf43fa5);
                }
                rumble(g, 0.18, 0.05);

                if (g->keys_collected >= g->cavern.keys_total && !g->portal_unlocked) {
                    g->portal_unlocked = true;
                    g->shake_intensity = 5;
                    lm_audio_portal_open();
                    spawn_burst(g, g->cavern.portal.x + TILE_SIZE, g->cavern.portal.y + TILE_SIZE, 22, 0x2ce87d, 160);
                    spawn_float(g, g->cavern.portal.x + TILE_SIZE, g->cavern.portal.y - 6, "PORTAL OPEN!", 0x2ce87d);
                    rumble(g, 0.55, 0.25);
                }
            } else if (item->kind == COL_RELIC) {
                g->total_relics_run += 1;
                g->relics_this_cavern += 1;
                int base = 100 + g->depth * 10;
                int earned = (int)lm_round(base * score_mult);
                g->relic_points += earned;
                g->score += earned;
                lm_audio_relic();
                spawn_burst(g, item->x, item->y, 10, 0xffe347, 105);
                snprintf(buf, sizeof(buf), "+%d PTS", earned);
                spawn_float(g, item->x, item->y - 10, buf, 0xffe347);
                rumble(g, 0.12, 0.04);
            } else if (item->kind == COL_AIR) {
                g->air = lm_min(MAX_AIR, g->air + AIR_BOTTLE);
                lm_audio_powerup();
                spawn_burst(g, item->x, item->y, 14, 0x22e4f5, 120);
                spawn_float(g, item->x, item->y - 10, "+AIR", 0x22e4f5);
                rumble(g, 0.2, 0.08);
            } else if (item->kind == COL_PLUG) {
                if (g->drain_y <= 0)
                    g->drain_y = g->water_y;
                g->drain_to = lm_max(g->drain_to, item->powerup * TILE_SIZE + WATER_LIP);
                g->shake_intensity = 6;
                lm_audio_tide();
                spawn_burst(g, item->x, item->y, 18, 0x22e4f5, 150);
                spawn_float(g, item->x, item->y - 14, "GLUG GLUG GLUG", 0x22e4f5);
                rumble(g, 0.6, 0.35);
            } else if (item->kind == COL_CHEST) {
                open_chest(g, item);
            }
        }
    }
}

static void
check_portal(struct lm_game *g)
{
    if (!g->portal_unlocked || g->transition_locked)
        return;
    struct lm_player *p = &g->player;
    double px = p->x + p->width * 0.5, py = p->y + p->height * 0.5;
    double pcx = g->cavern.portal.x + TILE_SIZE, pcy = g->cavern.portal.y + TILE_SIZE;

    if (lm_fabs(px - pcx) < 24 && lm_fabs(py - pcy) < 28) {
        g->transition_locked = true;
        int air_bonus = (int)lm_round(g->air * 6);
        int depth_bonus = g->depth * 100;
        int total = air_bonus + depth_bonus;
        g->score += total;
        if (g->score > g->high_score && !g->practice)
            g->high_score = g->score;

        lm_audio_portal_enter();
        spawn_burst(g, pcx, pcy, 26, 0x2ce87d, 175);
        rumble(g, 0.5, 0.2);

        g->adv.depth_cleared = g->depth;
        g->adv.relics_in_cavern = g->relics_this_cavern;
        g->adv.total_score = g->score;
        g->adv.air_bonus = total;
        g->adv.clean = g->hurt_depth != g->depth;
        g->adv.relic_bonus = 100 + g->depth * 10 + (g->adv.clean ? 100 : 0);
        g->relic_points += g->adv.relic_bonus;
        g->events |= EV_CAVERN_ADVANCED;
        if (g->depth % WORLD_SIZE == 0 && g->flawless && g->continues < MAX_CONTINUES) {
            g->continues += 1; /* a whole world without losing a life */
            g->flawless_award = true;
        }

        if (g->depth >= lm_level_count()) {
            emit_game_over(g, false);
            g->summary.won = true;
        } else if (g->depth % SHOP_INTERVAL == 0) {
            g->paused = true;
            clear_inputs(g);
            lm_audio_set_music_mode(MUSIC_CAMPFIRE, 1);
            g->events |= EV_OPEN_SHOP;
        } else {
            char t[96], s[200], n[32];
            int left = SHOP_INTERVAL - (g->depth % SHOP_INTERVAL);
            g->pending_advance = 1.25;
            lm_fmt_int(n, sizeof(n), total);
            snprintf(t, sizeof(t), "Cavern %02d Cleared!", g->depth);
            snprintf(s, sizeof(s), "Air Bonus +%s \xc2\xb7 Relic Points +%d%s \xc2\xb7 Shop in %d", n, g->adv.relic_bonus,
                     g->adv.clean ? " (not a scratch)" : "", left);
            set_banner(g, t, s, 1.25);
        }
    }
}

static void
update_air(struct lm_game *g, double dt)
{
    if (g->transition_locked)
        return;
    double drain_mult = lm_pow(0.78, g->perks[PERK_BRASS_LANTERN]);
    g->air = lm_max(0, g->air - MAX_AIR / g->cavern.air_secs * drain_mult * dt);
    if (g->air <= 0 && g->powers[PU_TEA_THERMOS] > 0) {
        /* a packed Tea Thermos drinks itself instead of the lamp going out */
        struct lm_player const *p = &g->player;
        g->powers[PU_TEA_THERMOS] -= 1;
        g->air = lm_min(MAX_AIR, 38.0);
        g->score += 150;
        lm_audio_powerup();
        g->hitstop = 0.035;
        spawn_burst(g, p->x + p->width * 0.5, p->y + p->height * 0.5, 16, lm_powerups[PU_TEA_THERMOS].color, 145);
        spawn_float(g, p->x + p->width * 0.5, p->y - 10, "TEA BREAK! +AIR", lm_powerups[PU_TEA_THERMOS].color);
        rumble(g, 0.35, 0.1);
    }
    if (g->air <= 0)
        lose_life(g, true);
}

static void
emit_game_over(struct lm_game *g, bool air)
{
    struct lm_run_summary *s = &g->summary;
    if (g->events & EV_GAME_OVER)
        return; /* already dealt with this step */
    g->paused = true;
    clear_inputs(g);
    lm_audio_set_music_mode(MUSIC_NONE, 1);
    if (!(g->transition_locked && g->depth >= lm_level_count()))
        lm_audio_game_over();
    rumble(g, 0.8, 0.45);

    bool new_hi = g->score > g->run_start_high;
    bool new_depth = g->depth > g->run_start_depth;
    if (new_hi) g->high_score = g->score;
    if (new_depth) g->best_depth = g->depth;

    s->depth_reached = g->depth;
    s->caverns_cleared = g->depth - 1;
    s->score = g->score;
    s->high_score = g->high_score;
    s->new_high_score = new_hi;
    s->new_best_depth = new_depth;
    s->relics = g->total_relics_run;
    s->relic_points_left = g->relic_points;
    s->keys = g->total_keys_run;
    snprintf(s->seed, sizeof(s->seed), "%s", g->seed);
    s->air = air;
    s->won = false;
    g->events |= EV_GAME_OVER;
}

/* Out of hearts or air: costs a life and restarts this cavern; with no lives left the run ends. */
static void
lose_life(struct lm_game *g, bool air)
{
    struct lm_player const *p = &g->player;
    if ((g->events & EV_GAME_OVER) || g->down || g->respawn_timer > 0)
        return;
    g->flawless = false;
    g->hurt_depth = g->depth;
    if (g->lives <= 1) {
        g->lives = 0;
        if (g->continues > 0) {
            g->down = true;
            g->paused = true;
            clear_inputs(g);
            lm_audio_set_music_mode(MUSIC_NONE, 1);
            lm_audio_game_over();
            rumble(g, 0.8, 0.45);
            g->events |= EV_CONTINUE;
        } else {
            emit_game_over(g, air);
        }
        return;
    }
    g->lives -= 1;
    g->respawn_timer = 1.3;
    g->transition_locked = true; /* stops the air drain while down */
    clear_inputs(g);
    lm_audio_hurt(false);
    spawn_burst(g, p->x + p->width * 0.5, p->y + p->height * 0.5, 22, air ? 0x22e4f5 : 0xf43fa5, 175);
    {
        char t[64], s[96];
        snprintf(t, sizeof(t), air ? "Lantern Out!" : "Knocked Down!");
        snprintf(s, sizeof(s), "%d %s left \xc2\xb7 Restarting Cavern %02d", g->lives, g->lives == 1 ? "life" : "lives", g->depth);
        set_banner(g, t, s, 1.3);
    }
    rumble(g, 0.8, 0.3);
}

/* ------------------------------------------------------------- ambient fx */

static void
update_motes(struct lm_game *g, double dt)
{
    for (int i = 0; i < LM_MOTES; i++) {
        struct lm_mote *m = &g->motes[i];
        m->x += m->vx * dt;
        m->y += m->vy * dt;
        if (m->y < 10) {
            m->y = CANVAS_H - 10;
            m->x = lm_rand(g) * CANVAS_W;
        }
        if (m->x < 10) m->x = CANVAS_W - 10;
        if (m->x > CANVAS_W - 10) m->x = 10;
    }
}

static void
update_particles(struct lm_game *g, double dt)
{
    for (int i = g->n_particles - 1; i >= 0; i--) {
        struct lm_particle *p = &g->particles[i];
        p->life -= dt;
        if (p->life <= 0) {
            memmove(&g->particles[i], &g->particles[i + 1], (size_t)(g->n_particles - i - 1) * sizeof(*p));
            g->n_particles--;
            continue;
        }
        p->vy += p->gravity * dt;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->alpha = p->life / p->max_life;
    }
}

static void
update_floats(struct lm_game *g, double dt)
{
    for (int i = g->n_floats - 1; i >= 0; i--) {
        struct lm_float_text *f = &g->floats[i];
        f->life -= dt;
        if (f->life <= 0) {
            memmove(&g->floats[i], &g->floats[i + 1], (size_t)(g->n_floats - i - 1) * sizeof(*f));
            g->n_floats--;
            continue;
        }
        f->y += f->vy * dt;
    }
}

/* --------------------------------------------------------------------- step */

/* The tide runs from the moment the cavern appears, banner and all, so every attempt starts the same way. */
static void
update_water(struct lm_game *g, double dt)
{
    int was = g->tide_phase;
    g->tide_clock += dt;
    double y = lm_tide_at(&g->cavern.tide, g->tide_clock, &g->tide_phase);
    if (g->drain_to > 0) {
        g->drain_y = lm_min(g->drain_to, g->drain_y + DRAIN_SPEED * dt);
        y = lm_max(y, g->drain_y);
    }
    g->water_y = y;
    /* no warning once the plugs are out and there is nothing left to rise */
    if (g->tide_phase == TIDE_WARN && was != TIDE_WARN && g->drain_to < g->cavern.tide.low_y) {
        lm_audio_tide();
        spawn_float(g, CANVAS_W * 0.5, lm_max(60, g->cavern.tide.high_y - 14), "THE WATER STIRS", 0x22e4f5);
        rumble(g, 0.25, 0.3);
    }
}

void
lm_step(struct lm_game *g, double dt)
{
    if (!g->running)
        return;
    g->elapsed += dt;

    if (g->rumble_time > 0) {
        g->rumble_time -= dt;
        if (g->rumble_time <= 0) {
            g->rumble_time = 0;
            g->rumble_strength = 0;
        }
    }
    if (g->paused)
        return;

    if (g->banner.active) {
        g->banner.remaining -= dt;
        if (g->banner.remaining <= 0)
            g->banner.active = false;
    }

    if (g->shake_intensity > 0.1) {
        g->shake_intensity = lm_max(0, g->shake_intensity - dt * 26);
        g->shake_x = (lm_rand(g) - 0.5) * 2 * g->shake_intensity;
        g->shake_y = (lm_rand(g) - 0.5) * 2 * g->shake_intensity;
    } else {
        g->shake_x = 0;
        g->shake_y = 0;
    }
    g->hit_flash = lm_max(0, g->hit_flash - dt * 2.8);

    if (g->respawn_timer > 0) {
        g->respawn_timer -= dt;
        update_motes(g, dt);
        update_particles(g, dt);
        update_floats(g, dt);
        if (g->respawn_timer <= 0) {
            int shield_left = g->powers[PU_LANTERN_SHIELD];
            g->respawn_timer = 0;
            load_cavern(g, g->depth); /* fresh cavern, full air */
            g->powers[PU_LANTERN_SHIELD] = shield_left; /* a retry does not eat shield renewals */
            g->hearts = g->max_hearts;
            g->player.invuln = 2.0;
            clear_inputs(g);
        }
        return;
    }

    if (g->pending_advance > 0) {
        g->pending_advance -= dt;
        update_motes(g, dt);
        update_particles(g, dt);
        update_floats(g, dt);
        if (g->pending_advance <= 0) {
            enter_cavern(g, g->depth + 1);
            lm_audio_update_tempo(1 + lm_min(0.24, (g->depth - 1) * 0.02));
        }
        return;
    }

    if (g->hitstop > 0) {
        g->hitstop -= dt;
        return;
    }

    update_motes(g, dt);
    update_particles(g, dt);
    update_floats(g, dt);
    update_crumbles(g, dt);
    update_water(g, dt);
    update_movers(g, dt);
#define STEP_ENDED() ((g->events & EV_GAME_OVER) || g->down || g->respawn_timer > 0)
    update_player(g, dt);
    if (STEP_ENDED())
        return;
    update_enemies(g, dt);
    if (STEP_ENDED())
        return;
    update_projectiles(g, dt);
    if (STEP_ENDED())
        return;
    check_collectables(g, dt);
    check_portal(g);
    update_air(g, dt);
#undef STEP_ENDED
}

/* ------------------------------------------------------------- continues */

void
lm_use_continue(struct lm_game *g)
{
    if (!g->down || g->continues <= 0)
        return;
    g->continues -= 1;
    g->down = false;
    g->paused = false;
    g->events = 0;
    g->lives = START_LIVES;
    g->hearts = g->max_hearts;
    g->score = g->world_score;
    g->chest_taken = 0;
    g->paid_depth = 0;
    g->hurt_depth = 0;
    g->respawn_timer = g->pending_advance = 0;
    clear_inputs(g);
    {
        int world_start = (lm_world_of(g->depth) - 1) * WORLD_SIZE + 1;
        g->depth = 0; /* so the new world gets announced */
        enter_cavern(g, world_start);
    }
    g->running = true;
    lm_audio_set_music_mode(MUSIC_CAVERN, 1.0);
}

void
lm_give_up(struct lm_game *g)
{
    if (!g->down)
        return;
    g->down = false;
    emit_game_over(g, false);
}

/* ---------------------------------------------------------------- saving */

void
lm_save_run(struct lm_game const *g, FILE *f, bool in_shop)
{
    /* caught between the portal and the next cavern: resume in the next one */
    int depth = g->depth + (g->pending_advance > 0 ? 1 : 0);
    fprintf(f, "version=1\nseed=%s\ndepth=%d\nin_shop=%d\ndown=%d\n", g->seed, depth, in_shop, g->down);
    fprintf(f, "score=%d\nworld_score=%d\nrelic_points=%d\nlives=%d\nhearts=%d\nmax_hearts=%d\n", g->score, g->world_score,
            g->relic_points, g->lives, g->hearts, g->max_hearts);
    fprintf(f, "continues=%d\ncontinues_bought=%d\nflawless=%d\nnext_shield=%d\nshield_on=%d\n", g->continues,
            g->continues_bought, g->flawless, g->next_shield, g->player.has_shield && g->pending_advance <= 0);
    /* format 2: powers are carried or not, and a running one is not saved */
    fprintf(f, "format=3\nhurt_depth=%d\n", g->hurt_depth);
    fprintf(f, "relics=%d\nkeys=%d\nchest_taken=%d\nlives_bought=%d\n", g->total_relics_run, g->total_keys_run, g->chest_taken,
            g->lives_bought);
    fprintf(f, "run_start_high=%d\nrun_start_depth=%d\npaid_depth=%d\npaid=%u\n", g->run_start_high, g->run_start_depth,
            g->paid_depth, g->paid);
    for (int i = 0; i < PERK_COUNT; i++)
        fprintf(f, "perk%d=%d\n", i, g->perks[i]);
    for (int i = 0; i < PU_COUNT; i++)
        fprintf(f, "power%d=%d\n", i, g->powers[i]);
}

bool
lm_load_run(struct lm_game *g, FILE *f, bool *in_shop)
{
    char line[96], seed[40] = "";
    int version = 0, depth = 0, shop = 0, down = 0, shield_on = 0;
    int perks[PERK_COUNT] = {0}, powers[PU_COUNT] = {0}, times[PU_COUNT] = {0};
    int v[16] = {0};
    int start_high = -1, start_depth = -1, paid_depth = 0; /* absent from older saves */
    uint32_t paid = 0;
    static char const *const names[] = {"score", "world_score", "relic_points", "lives", "hearts", "max_hearts", "continues",
                                        "continues_bought", "flawless", "next_shield", "relics", "keys", "chest_taken",
                                        "lives_bought", "hurt_depth", "format"};
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        int n, k;
        if (!eq)
            continue;
        *eq++ = 0;
        eq[strcspn(eq, "\r\n")] = 0;
        n = atoi(eq);
        if (!strcmp(line, "version")) version = n;
        else if (!strcmp(line, "seed")) snprintf(seed, sizeof(seed), "%.39s", eq);
        else if (!strcmp(line, "depth")) depth = n;
        else if (!strcmp(line, "in_shop")) shop = n;
        else if (!strcmp(line, "down")) down = n;
        else if (!strcmp(line, "shield_on")) shield_on = n;
        else if (!strcmp(line, "run_start_high")) start_high = n;
        else if (!strcmp(line, "run_start_depth")) start_depth = n;
        else if (!strcmp(line, "paid_depth")) paid_depth = n; /* before the "paid" test below */
        else if (!strcmp(line, "paid")) paid = (uint32_t)strtoul(eq, NULL, 10);
        else if (!strncmp(line, "perk", 4) && (k = atoi(line + 4)) >= 0 && k < PERK_COUNT) perks[k] = n;
        else if (!strncmp(line, "power", 5) && (k = atoi(line + 5)) >= 0 && k < PU_COUNT) powers[k] = n;
        else if (!strncmp(line, "time", 4) && (k = atoi(line + 4)) >= 0 && k < PU_COUNT) times[k] = n;
        else
            for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
                if (!strcmp(line, names[i]))
                    v[i] = n;
    }
    if (version != 1 || depth < 1 || depth > lm_level_count() || v[5] < 1 || (!down && v[3] < 1))
        return false;

    lm_start_run(g, seed);
    g->depth = depth;
    g->score = v[0];
    g->world_score = v[1];
    g->relic_points = v[2];
    g->lives = v[3];
    g->max_hearts = v[5] > 5 ? 5 : v[5];
    g->hearts = v[4] < 1 ? g->max_hearts : v[4] > g->max_hearts ? g->max_hearts : v[4];
    g->continues = v[6];
    g->continues_bought = v[7];
    g->flawless = v[8] != 0;
    g->total_relics_run = v[10];
    g->total_keys_run = v[11];
    g->chest_taken = v[12];
    g->lives_bought = v[13];
    g->hurt_depth = v[14];
    for (int i = 0; i < PERK_COUNT; i++)
        g->perks[i] = perks[i] > 0; /* older saves could hold two or three of a thing */
    if (start_high >= 0)
        g->run_start_high = start_high;
    if (start_depth >= 0)
        g->run_start_depth = start_depth;
    g->paid_depth = paid_depth;
    g->paid = paid;
    if (v[15] < 3 && depth > WORLD_SIZE) /* caverns past the first world have been rebuilt since */
        g->paid_depth = g->chest_taken = 0, g->paid = 0;
    load_cavern(g, depth);
    /* loading a cavern spends shields, so put them back exactly as saved */
    memcpy(g->powers, powers, sizeof(powers));
    for (int i = 0; i < PU_COUNT; i++)
        if (power_is_timed(i)) /* an older save with time left on the clock gets one to carry */
            g->powers[i] = powers[i] > 0 || (v[15] < 2 && times[i] > 0);
    g->next_shield = v[9] != 0;
    g->player.has_shield = shield_on != 0;
    g->player.can_air_jump = g->perks[PERK_FEATHERSTEP] > 0;
    *in_shop = shop != 0;
    if (*in_shop || down) {
        g->paused = true;
        g->banner.active = false;
        g->adv.depth_cleared = depth;
        g->adv.air_bonus = 0;
    } else {
        set_banner(g, g->cavern.name, g->cavern.hint, 1.8);
    }
    if (down) {
        g->down = true;
        lm_audio_set_music_mode(MUSIC_NONE, 1);
    } else if (*in_shop) {
        lm_audio_set_music_mode(MUSIC_CAMPFIRE, 1);
    }
    return true;
}
