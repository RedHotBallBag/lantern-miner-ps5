/*
** Lantern Miner: simulation. A port of engine.ts (physics, guardians, power-ups, perks, shop rules, run flow) with the
** same constants and the same order of operations. The app reads the state directly and consumes `events`.
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
    g->in_left = g->in_right = g->in_jump = false;
    g->prev_jump = false;
}

static void
emit_game_over(struct lm_game *g, bool air);
static void lose_life(struct lm_game *g, bool air);

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
    for (int i = 0; i < 4; i++) {
        p->scarf[i].x = px + 10;
        p->scarf[i].y = py + 11;
    }
}

static void
load_cavern(struct lm_game *g, int depth)
{
    lm_generate_cavern(g->seed, depth, &g->cavern);
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
    }
    g->n_proj = 0;
    memset(g->crumble, 0, sizeof(g->crumble));
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
    g->phoenix = false;
    g->respawn_timer = 0;
    g->run_start_high = g->high_score; /* records as they stood when this run began */
    g->run_start_depth = g->best_depth;
    g->total_relics_run = 0;
    g->total_keys_run = 0;
    memset(g->perks, 0, sizeof(g->perks));
    memset(g->powers, 0, sizeof(g->powers));
    memset(g->power_time, 0, sizeof(g->power_time));
    g->next_shield = false;
    g->paused = false;
    g->pending_advance = 0;
    g->banner.active = false;
    g->events = 0;
    clear_inputs(g);
    load_cavern(g, 1);
    g->running = true;
    lm_audio_set_music_mode(MUSIC_CAVERN, 1.0);
}

void
lm_set_paused(struct lm_game *g, bool paused)
{
    g->paused = paused;
    if (paused)
        clear_inputs(g);
}

void
lm_set_input(struct lm_game *g, bool left, bool right, bool jump)
{
    if (g->paused || !g->running) {
        clear_inputs(g);
        return;
    }
    g->in_left = left;
    g->in_right = right;
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
power_is_timed(int item)
{
    return item == SHOP_SPRING_BOOTS || item == SHOP_STARLIGHT_MAGNET || item == SHOP_CLOCKWORK_SLOW;
}

static double
power_base_time(int item)
{
    return item == SHOP_SPRING_BOOTS ? 25.0 : 20.0;
}

/* Timed powers cost more for each extra x you stack (x2 = +75%, x3 = +150% of the base price). */
int
lm_shop_cost(struct lm_game const *g, int item)
{
    int cost = lm_shop[item].cost;
    if (power_is_timed(item))
        cost += cost * 3 * g->powers[item - SHOP_SPRING_BOOTS] / 4;
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
    return item == SHOP_DELVERS_CACHE ? 3 : item == SHOP_PHOENIX_FEATHER ? 5 : 1;
}

void
lm_shop_item_status(struct lm_game const *g, int item, struct lm_shop_status *out)
{
    struct lm_shop_def const *it = &lm_shop[item];
    char a[24], b[24];
    int cost = lm_shop_cost(g, item);
    memset(out, 0, sizeof(*out));
    if (item == SHOP_KETTLE_BREW) {
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
    } else if (item == SHOP_DELVERS_CACHE || item == SHOP_PHOENIX_FEATHER) {
        int need = lm_shop_unlock_at(item);
        if (g->depth / SHOP_INTERVAL < need) {
            snprintf(out->owned, sizeof(out->owned), "Locked");
            snprintf(out->status, sizeof(out->status), "Stall %d unlocks", need);
            return;
        }
        if (item == SHOP_PHOENIX_FEATHER && g->phoenix) {
            snprintf(out->owned, sizeof(out->owned), "Packed");
            snprintf(out->status, sizeof(out->status), "Ready");
            out->is_owned = true;
            return;
        }
        snprintf(out->owned, sizeof(out->owned), item == SHOP_PHOENIX_FEATHER ? "Once" : "Bundle");
    } else {
        int owned = it->perk >= 0 ? g->perks[it->perk] : item >= SHOP_SPRING_BOOTS && item <= SHOP_CLOCKWORK_SLOW ? g->powers[item - SHOP_SPRING_BOOTS] : 0;
        if (it->max_stacks > 0 && owned >= it->max_stacks) {
            snprintf(out->status, sizeof(out->status), power_is_timed(item) ? "Max x3" : "Active");
            if (power_is_timed(item))
                snprintf(out->owned, sizeof(out->owned), "x%d %ds", owned, (int)lm_ceil(g->power_time[item - SHOP_SPRING_BOOTS]));
            else if (item == SHOP_LANTERN_SHIELD)
                snprintf(out->owned, sizeof(out->owned), "%d left", owned);
            else if (it->max_stacks > 1)
                snprintf(out->owned, sizeof(out->owned), "%d/%d", owned, it->max_stacks);
            else
                snprintf(out->owned, sizeof(out->owned), "Max");
            out->is_owned = true;
            return;
        }
        if (power_is_timed(item)) {
            if (owned > 0)
                snprintf(out->owned, sizeof(out->owned), "x%d %ds", owned, (int)lm_ceil(g->power_time[item - SHOP_SPRING_BOOTS]));
            else
                snprintf(out->owned, sizeof(out->owned), "%ds", (int)power_base_time(item));
        } else if (item == SHOP_LANTERN_SHIELD && owned > 0)
            snprintf(out->owned, sizeof(out->owned), "%d left", owned);
        else if (it->max_stacks > 1)
            snprintf(out->owned, sizeof(out->owned), "%d/%d", owned, it->max_stacks);
        else
            snprintf(out->owned, sizeof(out->owned), "Unique");
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
    if (g->relic_points < cost)
        return false;
    if (item == SHOP_KETTLE_BREW) {
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
    } else if (item == SHOP_DELVERS_CACHE) {
        if (g->depth / SHOP_INTERVAL < lm_shop_unlock_at(item))
            return false;
        g->relic_points -= cost;
        g->lives = g->lives + 2 < MAX_LIVES ? g->lives + 2 : MAX_LIVES;
        g->hearts = g->max_hearts;
        if (g->powers[PU_TEA_THERMOS] < 3)
            g->powers[PU_TEA_THERMOS] += 1;
    } else if (item == SHOP_PHOENIX_FEATHER) {
        if (g->phoenix || g->depth / SHOP_INTERVAL < lm_shop_unlock_at(item))
            return false;
        g->relic_points -= cost;
        g->phoenix = true;
    } else if (item >= SHOP_SPRING_BOOTS && item <= SHOP_CLOCKWORK_SLOW) {
        int pu = item - SHOP_SPRING_BOOTS;
        if (g->powers[pu] >= it->max_stacks)
            return false;
        g->relic_points -= cost;
        if (power_is_timed(item)) {
            /* each purchase adds another full timer; the xN is how many timers are stacked (up to 3) */
            g->power_time[pu] = lm_min(g->power_time[pu] + power_base_time(item), power_base_time(item) * 3);
            g->powers[pu] = (int)lm_ceil(g->power_time[pu] / power_base_time(item));
        } else if (pu == PU_LANTERN_SHIELD) {
            g->powers[pu] = SHIELD_CAVERNS; /* shields the next few caverns */
        } else {
            g->powers[pu] += 1;
        }
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

void
lm_leave_shop(struct lm_game *g)
{
    char t[128], s[200];
    g->depth += 1;
    g->paused = false;
    clear_inputs(g);
    load_cavern(g, g->depth);
    snprintf(t, sizeof(t), "Cavern %02d \xc2\xb7 %s", g->depth, g->cavern.name);
    snprintf(s, sizeof(s), "%s \xc2\xb7 Gather %d Keys", lm_biomes[g->cavern.biome].label, g->cavern.keys_total);
    set_banner(g, t, s, 1.4);
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
            if (tile == T_EMPTY || tile == T_BRAMBLE)
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

static void
check_hazards(struct lm_game *g, struct lm_player *p)
{
    if (p->invuln > 0)
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

/* Clockwork Slow is now a bought, permanent power: guardians and conveyors run at 75% (the old timed pickup used
   `timed`, which was too strong to keep on all the time). Returns 1 when not owned. */
static double
power_slow(struct lm_game const *g, double timed)
{
    (void)timed;
    return g->powers[PU_CLOCKWORK_SLOW] > 0 ? 0.75 : 1.0;
}

static void
update_player(struct lm_game *g, double dt)
{
    struct lm_player *p = &g->player;
    int cleats = g->perks[PERK_CONVEYOR_CLEATS];
    int feather = g->perks[PERK_FEATHERSTEP];

    p->invuln = lm_max(0, p->invuln - dt);
    p->spring = lm_max(0, p->spring - dt);
    p->magnet = lm_max(0, p->magnet - dt);
    p->slow = 0; /* applied through power_slow() */
    /* bought powers run on a timer that only counts down while you are playing; when it ends the stacks reset */
    static int const timed[3] = {PU_SPRING_BOOTS, PU_STARLIGHT_MAGNET, PU_CLOCKWORK_SLOW};
    for (int k = 0; k < 3; k++) {
        int pu = timed[k];
        if (g->powers[pu] > 0) {
            if (!g->banner.active) /* the timer waits while the cavern banner is up */
                g->power_time[pu] -= dt;
            if (g->power_time[pu] <= 0) {
                g->power_time[pu] = 0;
                g->powers[pu] = 0;
            } else {
                g->powers[pu] = (int)lm_ceil(g->power_time[pu] / power_base_time(SHOP_SPRING_BOOTS + pu));
            }
        }
    }
    p->spring = g->powers[PU_SPRING_BOOTS] > 0 ? 1 : 0;
    p->magnet = g->powers[PU_STARLIGHT_MAGNET] > 0 ? 1 : 0;

    p->squash_x += (1 - p->squash_x) * lm_min(1, dt * 14);
    p->squash_y += (1 - p->squash_y) * lm_min(1, dt * 14);

    int move_dir = 0;
    if (g->in_left) move_dir -= 1;
    if (g->in_right) move_dir += 1;
    if (move_dir != 0)
        p->facing = move_dir > 0 ? 1 : -1;

    double max_speed = MOVE_SPEED * (1 + cleats * 0.12);
    double target_vx = move_dir * max_speed;
    double accel = p->grounded ? MOVE_ACCEL_GROUND : MOVE_ACCEL_AIR;
    if (p->vx < target_vx)
        p->vx = lm_min(target_vx, p->vx + accel * dt);
    else if (p->vx > target_vx)
        p->vx = lm_max(target_vx, p->vx - accel * dt);

    double conveyor_vx = 0;
    if (p->grounded && p->on_conveyor != 0) {
        double slow_factor = power_slow(g, 0.5);
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
            p->jump_buffer = 0;
            p->squash_x = 0.76;
            p->squash_y = 1.26;
            lm_audio_jump(true);
            spawn_burst(g, p->x + p->width / 2, p->y + p->height, 8, 0x2ce87d, 95);
        }
    }

    if (g->prev_jump && !g->in_jump && p->vy < -150)
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
    resolve_vertical(g, p, prev_bottom);

    if (!was_grounded && p->grounded && impact_vy > 130) {
        double intensity = lm_min(1, impact_vy / MAX_FALL_SPEED);
        p->squash_x = 1 + intensity * 0.24;
        p->squash_y = 1 - intensity * 0.2;
        lm_audio_land(intensity);
        spawn_burst(g, p->x + p->width / 2, p->y + p->height, 5, 0xd6c3a5, 55);
    }

    if (p->grounded && lm_fabs(p->vx) > 15)
        p->walk_phase += dt * (lm_fabs(p->vx) / MOVE_SPEED);

    double neck_x = p->x + p->width * 0.5 - p->facing * 4;
    double neck_y = p->y + 11;
    p->scarf[0].x = neck_x;
    p->scarf[0].y = neck_y;
    for (int i = 1; i < 4; i++) {
        double tx = p->scarf[i - 1].x - p->facing * 4.2;
        double ty = p->scarf[i - 1].y + 0.8 + lm_sin(g->elapsed * 9 + i) * 1.1;
        p->scarf[i].x += (tx - p->scarf[i].x) * lm_min(1, dt * 18);
        p->scarf[i].y += (ty - p->scarf[i].y) * lm_min(1, dt * 18);
    }

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
    double slow_mult = power_slow(g, 0.48) * chrono;
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
    double step_dt = dt * (power_slow(g, 0.48) * chrono);
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

static void
activate_powerup(struct lm_game *g, int type, double x, double y)
{
    struct lm_powerup_def const *def = &lm_powerups[type];
    struct lm_player *p = &g->player;
    double dur_mult = g->perks[PERK_CHRONOMETER] > 0 ? 1.5 : 1;
    char buf[48];
    lm_audio_powerup();
    g->hitstop = 0.035;
    spawn_burst(g, x, y, 16, def->color, 145);
    rumble(g, 0.35, 0.1);
    switch (type) {
    case PU_SPRING_BOOTS:
        p->spring = def->duration * dur_mult;
        spawn_float(g, x, y - 10, "SPRING BOOTS!", def->color);
        break;
    case PU_LANTERN_SHIELD:
        p->has_shield = true;
        spawn_float(g, x, y - 10, "LANTERN SHIELD!", def->color);
        break;
    case PU_TEA_THERMOS:
        g->air = lm_min(MAX_AIR, g->air + 38);
        g->score += 150;
        spawn_float(g, x, y - 10, "+AIR & TEA!", def->color);
        break;
    case PU_STARLIGHT_MAGNET:
        p->magnet = def->duration * dur_mult;
        spawn_float(g, x, y - 10, "STARLIGHT MAGNET!", def->color);
        break;
    case PU_CLOCKWORK_SLOW:
    default:
        p->slow = def->duration * dur_mult;
        spawn_float(g, x, y - 10, "CLOCKWORK SLOW!", def->color);
        break;
    }
    (void)buf;
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

        if (magnet_radius > 0 && item->kind != COL_POWERUP && dist < magnet_radius && dist > 1) {
            double pull = p->magnet > 0 ? 275 : 135;
            item->x += (dx / dist) * pull * dt;
            item->y += (dy / dist) * pull * dt;
        }

        if (dist < 22) {
            item->collected = true;
            if (item->kind == COL_KEY) {
                g->keys_collected += 1;
                g->total_keys_run += 1;
                int pts = (int)lm_round(200 * score_mult);
                g->score += pts;
                g->air = lm_min(MAX_AIR, g->air + 9);
                lm_audio_key(g->keys_collected - 1);
                spawn_burst(g, item->x, item->y, 14, 0xf43fa5, 135);
                snprintf(buf, sizeof(buf), "+%d", pts);
                spawn_float(g, item->x, item->y - 10, buf, 0xf43fa5);
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
                g->air = lm_min(MAX_AIR, g->air + 6);
                lm_audio_relic();
                spawn_burst(g, item->x, item->y, 10, 0xffe347, 105);
                snprintf(buf, sizeof(buf), "+%d PTS", earned);
                spawn_float(g, item->x, item->y - 10, buf, 0xffe347);
                rumble(g, 0.12, 0.04);
            } else if (item->kind == COL_POWERUP) {
                activate_powerup(g, item->powerup, item->x, item->y);
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
        if (g->score > g->high_score)
            g->high_score = g->score;

        lm_audio_portal_enter();
        spawn_burst(g, pcx, pcy, 26, 0x2ce87d, 175);
        rumble(g, 0.5, 0.2);

        g->adv.depth_cleared = g->depth;
        g->adv.relics_in_cavern = g->relics_this_cavern;
        g->adv.total_score = g->score;
        g->adv.air_bonus = total;
        g->events |= EV_CAVERN_ADVANCED;

        if (g->depth % SHOP_INTERVAL == 0) {
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
            snprintf(s, sizeof(s), "Air Bonus +%s \xc2\xb7 Entering Cavern %02d \xc2\xb7 Shop in %d", n, g->depth + 1, left);
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
    double depth_drain = 1 + lm_min(0.65, (g->depth - 1) * 0.045);
    g->air = lm_max(0, g->air - BASE_AIR_DRAIN_PER_SEC * drain_mult * depth_drain * dt);
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
        return; /* already ended this step (the original keeps simulating to the end of the step) */
    g->paused = true;
    clear_inputs(g);
    lm_audio_set_music_mode(MUSIC_NONE, 1);
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
    g->events |= EV_GAME_OVER;
}

/* Out of hearts or air: costs a life and restarts this cavern; with no lives left the run ends. */
static void
lose_life(struct lm_game *g, bool air)
{
    struct lm_player const *p = &g->player;
    if ((g->events & EV_GAME_OVER) || g->respawn_timer > 0)
        return;
    if (g->lives <= 1 && g->phoenix) {
        g->phoenix = false;
        g->lives = 3; /* the feather's flame; the life being lost now comes off this */
        spawn_float(g, p->x + p->width * 0.5, p->y - 24, "PHOENIX RISES!", 0xff6b3d);
    }
    if (g->lives <= 1) {
        g->lives = 0;
        emit_game_over(g, air);
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
            g->depth += 1;
            load_cavern(g, g->depth);
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
#define STEP_ENDED() ((g->events & EV_GAME_OVER) || g->respawn_timer > 0)
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
