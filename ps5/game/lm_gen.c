/*
** Lantern Miner: seeded cavern generator. A line-by-line port of generator.ts: same FNV-1a string hash, same
** Mulberry32 stream, same order of every RNG call, so a seed builds the same cavern as the web version.
*/
#include "lm.h"

#include <stdio.h>
#include <stdlib.h>
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

/* Fisher-Yates over an array of `sz`-byte elements, in place. */
static void
rng_shuffle(struct rng *r, void *base, int n, size_t sz)
{
    unsigned char *a = base;
    unsigned char tmp[64];
    for (int i = n - 1; i > 0; i--) {
        int j = (int)lm_floor(rng_next(r) * (double)(i + 1));
        memcpy(tmp, a + (size_t)i * sz, sz);
        memcpy(a + (size_t)i * sz, a + (size_t)j * sz, sz);
        memcpy(a + (size_t)j * sz, tmp, sz);
    }
}

/* ------------------------------------------------------------- reachability */

struct cell { int c, r; };

struct plat { int row, start, end, type; };

static bool
walkable_support(int t)
{
    return t == T_SOLID || t == T_ONE_WAY || t == T_CONVEYOR_LEFT || t == T_CONVEYOR_RIGHT || t == T_CRUMBLE;
}

static bool
standable(uint8_t tiles[GRID_ROWS][GRID_COLS], int c, int r)
{
    if (c < 1 || c >= GRID_COLS - 1 || r < 2 || r >= GRID_ROWS - 1)
        return false;
    if (tiles[r][c] != T_EMPTY)
        return false;
    if (tiles[r - 1][c] == T_SOLID || tiles[r - 1][c] == T_BRAMBLE)
        return false;
    return walkable_support(tiles[r + 1][c]);
}

/* Breadth-first search in the same visiting order as the web version; `out` receives cells in discovery order. */
static int
compute_reachable(uint8_t tiles[GRID_ROWS][GRID_COLS], int sc, int sr, struct cell *out)
{
    static bool visited[GRID_ROWS][GRID_COLS];
    int n = 0, head = 0;
    memset(visited, 0, sizeof(visited));
    if (!standable(tiles, sc, sr))
        return 0;
    out[n++] = (struct cell){sc, sr};
    visited[sr][sc] = true;
    while (head < n) {
        struct cell cur = out[head++];
        for (int dc = -4; dc <= 4; dc++) {
            for (int dr = -3; dr <= 7; dr++) {
                if (dc == 0 && dr == 0)
                    continue;
                int nc = cur.c + dc, nr = cur.r + dr;
                if (nc < 0 || nc >= GRID_COLS || nr < 0 || nr >= GRID_ROWS)
                    continue;
                if (visited[nr][nc])
                    continue;
                if (!standable(tiles, nc, nr))
                    continue;
                int adc = dc < 0 ? -dc : dc;
                if (dr < 0) {
                    int rise = -dr;
                    if (rise == 3 && adc > 3) continue;
                    if (rise == 2 && adc > 4) continue;
                    if (rise == 1 && adc > 4) continue;
                } else if (dr == 0) {
                    if (adc > 4) continue;
                } else {
                    if (adc > 4) continue;
                }
                int step = dc >= 0 ? 1 : -1;
                bool blocked = false;
                int check_row = dr < 0 ? nr : cur.r;
                for (int c = cur.c + step; c != nc + step; c += step) {
                    if (tiles[check_row][c] == T_SOLID) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked)
                    continue;
                if (dr < 0) {
                    for (int r = cur.r - 1; r >= nr; r--) {
                        if (tiles[r][cur.c] == T_SOLID) {
                            blocked = true;
                            break;
                        }
                    }
                    if (blocked)
                        continue;
                }
                visited[nr][nc] = true;
                out[n++] = (struct cell){nc, nr};
            }
        }
    }
    return n;
}

/* ------------------------------------------------------------------ enemies */

static void
add_enemy(struct lm_cavern *cv, int kind, double x, double y, double min_x, double max_x, double min_y, double max_y,
          double speed, int direction, double phase)
{
    if (cv->n_enemies >= LM_MAX_ENEMIES)
        return;
    struct lm_enemy_spawn *e = &cv->enemies[cv->n_enemies++];
    e->kind = kind;
    e->x = x;
    e->y = y;
    e->min_x = min_x;
    e->max_x = max_x;
    e->min_y = min_y;
    e->max_y = max_y;
    e->speed = speed;
    e->direction = direction;
    e->phase = phase;
}

static void
generate_enemies(struct lm_cavern *cv, int depth, struct plat *plats, int nplat, int spawn_col, int spawn_row,
                 struct cell portal, struct rng *rng)
{
    uint8_t(*tiles)[GRID_COLS] = cv->tiles;
    double speed_scale = lm_min(1.85, 1 + (depth - 1) * 0.062);
    int total_desired = (int)lm_min(12, 3 + lm_floor((depth - 1) * 0.72));

    struct plat patrol[96];
    int np = 0;
    for (int i = 0; i < nplat; i++) {
        struct plat *p = &plats[i];
        int len = p->end - p->start + 1;
        if (len < 4) continue;
        if (p->type == T_CRUMBLE) continue;
        double mid = (p->start + p->end) / 2.0;
        if (lm_hypot(mid - spawn_col, (p->row - 1) - spawn_row) < 6.0) continue;
        if (p->row == portal.r + 1 && portal.c >= p->start - 1 && portal.c <= p->end + 1) continue;
        bool bad = false;
        for (int c = p->start; c <= p->end; c++)
            if (tiles[p->row - 1][c] == T_BRAMBLE) { bad = true; break; }
        if (bad) continue;
        patrol[np++] = *p;
    }
    rng_shuffle(rng, patrol, np, sizeof(struct plat));

    int ground_target = (int)lm_min(np, lm_ceil(total_desired * 0.52));
    int spitters = 0, chargers = 0;
    for (int i = 0; i < ground_target; i++) {
        struct plat *plat = &patrol[i];
        int plat_len = plat->end - plat->start + 1;
        int kind = EN_BEETLE;

        if (depth >= 7 && spitters < (int)lm_min(2, lm_floor((depth - 5) / 3.0)) && rng_chance(rng, 0.45)) {
            kind = EN_SPITTER;
            spitters++;
        } else if (depth >= 5 && plat_len >= 5 && chargers < (int)lm_min(2, lm_floor((depth - 3) / 2.0)) && rng_chance(rng, 0.55)) {
            kind = EN_CHARGER;
            chargers++;
        } else if (depth >= 2 && i % 2 == 1 && rng_chance(rng, 0.65)) {
            kind = EN_BOUNCER;
        }

        double min_x = plat->start * TILE_SIZE + 6;
        double max_x = (plat->end + 1) * TILE_SIZE - 28;
        if (max_x - min_x < 36)
            continue;

        double start_x = kind == EN_SPITTER ? (min_x + max_x) * 0.5 : min_x + rng_next(rng) * (max_x - min_x);
        double y = (plat->row - 1) * TILE_SIZE + 6;
        double base_speed = kind == EN_CHARGER ? 66 : kind == EN_BEETLE ? 70 : kind == EN_BOUNCER ? 50 : 0;
        int dir = rng_chance(rng, 0.5) ? 1 : -1;
        double phase = rng_next(rng) * LM_PI * 2;
        add_enemy(cv, kind, start_x, y, min_x, max_x, y - 50, y, base_speed * speed_scale, dir, phase);
    }

    if (depth >= 10) {
        int stalkers = depth >= 18 ? 2 : 1;
        for (int s = 0; s < stalkers; s++) {
            double sx = spawn_col < GRID_COLS / 2 ? (GRID_COLS - 5 - s * 4) * TILE_SIZE : (4 + s * 4) * TILE_SIZE;
            double sy = (3 + s * 3) * TILE_SIZE;
            double phase = rng_next(rng) * LM_PI * 2;
            add_enemy(cv, EN_STALKER, sx, sy, TILE_SIZE, (GRID_COLS - 2) * TILE_SIZE, TILE_SIZE, (GRID_ROWS - 2) * TILE_SIZE,
                      lm_min(82, 46 + (depth - 10) * 3.5), 1, phase);
        }
    }

    int remaining = total_desired - cv->n_enemies;
    if (remaining < 0)
        remaining = 0;
    for (int i = 0; i < remaining; i++) {
        int kind = EN_GLOW_BAT;
        double roll = rng_next(rng);
        if (depth >= 4 && roll < 0.36)
            kind = EN_WISP;
        else if (depth >= 3 && roll < 0.7)
            kind = EN_PISTON;

        if (kind == EN_GLOW_BAT) {
            static int const rows[4] = {3, 6, 9, 12};
            int row = rows[rng_pick(rng, 4)];
            int min_c = rng_int(rng, 3, 9);
            int max_c = rng_int(rng, (int)lm_min(28, min_c + 9), 29);
            double start_x = ((min_c + max_c) / 2.0) * TILE_SIZE;
            double y = row * TILE_SIZE + 4;
            if (lm_hypot(start_x / TILE_SIZE - spawn_col, row - spawn_row) < 6)
                continue;
            int dir = rng_chance(rng, 0.5) ? 1 : -1;
            double phase = rng_next(rng) * LM_PI * 2;
            add_enemy(cv, EN_GLOW_BAT, start_x, y, min_c * TILE_SIZE, max_c * TILE_SIZE, y - 16, y + 16, 82 * speed_scale, dir, phase);
        } else if (kind == EN_PISTON) {
            int col = rng_int(rng, 5, GRID_COLS - 6);
            if (abs(col - spawn_col) < 5 || abs(col - portal.c) < 3)
                continue;
            int top_row = rng_int(rng, 3, 5);
            int bot_row = rng_int(rng, 12, 15);
            if (tiles[top_row][col] == T_SOLID)
                continue;
            double x = col * TILE_SIZE + 3;
            int dir = rng_chance(rng, 0.5) ? 1 : -1;
            double phase = rng_next(rng) * LM_PI * 2;
            add_enemy(cv, EN_PISTON, x, ((top_row + bot_row) / 2.0) * TILE_SIZE, x, x, top_row * TILE_SIZE, bot_row * TILE_SIZE,
                      78 * speed_scale, dir, phase);
        } else {
            double cx = rng_int(rng, 7, GRID_COLS - 8) * TILE_SIZE;
            double cy = rng_int(rng, 4, 12) * TILE_SIZE;
            if (lm_hypot(cx / TILE_SIZE - spawn_col, cy / TILE_SIZE - spawn_row) < 6.5)
                continue;
            double phase = rng_next(rng) * LM_PI * 2;
            add_enemy(cv, EN_WISP, cx, cy, cx - 58, cx + 58, cy - 38, cy + 38, 1.65 * speed_scale, 1, phase);
        }
    }
}

/* ---------------------------------------------------------------- candidate */

static bool
pick_spread(struct rng *rng, struct cell const *pool, int npool, double min_dist, struct cell const *existing, int nexisting,
            bool occupied[GRID_ROWS][GRID_COLS], int spawn_col, int spawn_row, struct cell *out)
{
    struct cell shuffled[GRID_ROWS * GRID_COLS];
    memcpy(shuffled, pool, (size_t)npool * sizeof(struct cell));
    rng_shuffle(rng, shuffled, npool, sizeof(struct cell));
    for (int i = 0; i < npool; i++) {
        struct cell cand = shuffled[i];
        if (occupied[cand.r][cand.c])
            continue;
        if (lm_hypot(cand.c - spawn_col, cand.r - spawn_row) < 3.5)
            continue;
        bool too_close = false;
        for (int k = 0; k < nexisting; k++)
            if (lm_hypot(cand.c - existing[k].c, cand.r - existing[k].r) < min_dist) { too_close = true; break; }
        if (!too_close) {
            *out = cand;
            return true;
        }
    }
    return false;
}

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

static bool
build_candidate(struct lm_cavern *cv, char const *seed, int depth, int biome, char const *name, struct rng *rng, bool force_bridges)
{
    (void)seed;
    memset(cv, 0, sizeof(*cv));
    cv->depth = depth;
    cv->biome = biome;
    snprintf(cv->name, sizeof(cv->name), "%s", name);
    uint8_t(*tiles)[GRID_COLS] = cv->tiles;

    for (int c = 0; c < GRID_COLS; c++) {
        tiles[0][c] = T_SOLID;
        tiles[GRID_ROWS - 1][c] = T_SOLID;
    }
    for (int r = 0; r < GRID_ROWS; r++) {
        tiles[r][0] = T_SOLID;
        tiles[r][GRID_COLS - 1] = T_SOLID;
    }
    for (int c = 1; c < GRID_COLS - 1; c++) {
        if (c <= 2 || c >= GRID_COLS - 3) {
            if (rng_chance(rng, 0.55))
                tiles[1][c] = T_SOLID;
        }
    }

    bool spawn_left = depth == 1 ? true : rng_chance(rng, 0.5);
    int spawn_col = spawn_left ? 3 : GRID_COLS - 4;
    int spawn_row = GRID_ROWS - 2;

    struct plat plats[96];
    int nplat = 0;
    static int const tier_rows[4] = {14, 11, 8, 5};
    double conveyor_prob = lm_min(0.46, 0.14 + (depth - 1) * 0.032);
    double crumble_prob = lm_min(0.35, 0.1 + (depth - 1) * 0.028);
    int max_w = depth >= 8 ? 7 : 8;
    int min_w = depth >= 6 ? 3 : 4;

    for (int t = 0; t < 4; t++) {
        int base_row = tier_rows[t];
        int cursor = rng_int(rng, 1, 3);
        int segs = 0;
        while (cursor < GRID_COLS - 5) {
            int width = rng_int(rng, min_w, max_w);
            int end_col = cursor + width - 1 < GRID_COLS - 2 ? cursor + width - 1 : GRID_COLS - 2;
            if (end_col - cursor + 1 >= 3) {
                int row_offset = 0;
                if (!force_bridges && t > 0 && t < 3 && rng_chance(rng, 0.3))
                    row_offset = rng_chance(rng, 0.5) ? 1 : -1;
                int row = base_row + row_offset;
                if (row > 15) row = 15;
                if (row < 4) row = 4;

                int type = T_ONE_WAY;
                double roll = rng_next(rng);
                if (roll < conveyor_prob)
                    type = rng_chance(rng, 0.5) ? T_CONVEYOR_LEFT : T_CONVEYOR_RIGHT;
                else if (roll < conveyor_prob + crumble_prob && end_col - cursor + 1 <= 6)
                    type = T_CRUMBLE;

                bool valid = true;
                for (int c = cursor; c <= end_col; c++) {
                    if (tiles[row][c] != T_EMPTY || tiles[row - 1][c] != T_EMPTY) { valid = false; break; }
                }
                if (valid) {
                    for (int c = cursor; c <= end_col; c++)
                        tiles[row][c] = (uint8_t)type;
                    if (nplat < 96)
                        plats[nplat++] = (struct plat){row, cursor, end_col, type};
                    segs++;
                }
            }
            cursor = end_col + rng_int(rng, 2, depth >= 7 ? 4 : 3);
        }
        if (t == 0 && segs == 0) {
            for (int c = 4; c <= 10; c++)
                tiles[14][c] = T_ONE_WAY;
            plats[nplat++] = (struct plat){14, 4, 10, T_ONE_WAY};
        }
    }

    /* reliable stepping platform near spawn */
    int ns_start = spawn_left ? 4 : GRID_COLS - 10;
    int ns_end = spawn_left ? 9 : GRID_COLS - 5;
    bool has_near = false;
    for (int c = ns_start; c <= ns_end; c++) {
        if (tiles[14][c] != T_EMPTY || tiles[15][c] != T_EMPTY) { has_near = true; break; }
    }
    if (!has_near || force_bridges) {
        for (int c = ns_start; c <= ns_end; c++)
            tiles[14][c] = T_ONE_WAY;
        plats[nplat++] = (struct plat){14, ns_start, ns_end, T_ONE_WAY};
    }

    if (force_bridges) {
        static int const bridges[6][3] = {{14, 12, 19}, {11, 7, 14}, {11, 18, 25}, {8, 11, 20}, {5, 5, 12}, {5, 19, 26}};
        for (int i = 0; i < 6; i++) {
            for (int c = bridges[i][1]; c <= bridges[i][2]; c++)
                if (tiles[bridges[i][0]][c] == T_EMPTY)
                    tiles[bridges[i][0]][c] = T_ONE_WAY;
            plats[nplat++] = (struct plat){bridges[i][0], bridges[i][1], bridges[i][2], T_ONE_WAY};
        }
    }

    /* crystal bramble hazards */
    int ground_target = (int)lm_min(6, 1 + lm_floor(depth * 0.6));
    int placed = 0;
    for (int tries = 0; tries < 32 && placed < ground_target; tries++) {
        int bc = rng_int(rng, 4, GRID_COLS - 5);
        if (abs(bc - spawn_col) < 5)
            continue;
        if (tiles[16][bc] != T_EMPTY || tiles[16][bc - 1] == T_BRAMBLE || tiles[16][bc + 1] == T_BRAMBLE)
            continue;
        if (tiles[15][bc] == T_EMPTY && tiles[14][bc] == T_EMPTY) {
            tiles[16][bc] = T_BRAMBLE;
            placed++;
        }
    }

    if (depth >= 4) {
        int ledge_target = (int)lm_min(3, lm_floor((depth - 2) / 3.0));
        int placed_ledge = 0;
        struct plat wide[96];
        int nw = 0;
        for (int i = 0; i < nplat; i++)
            if (plats[i].end - plats[i].start + 1 >= 6 && plats[i].type == T_ONE_WAY && plats[i].row >= 8)
                wide[nw++] = plats[i];
        rng_shuffle(rng, wide, nw, sizeof(struct plat));
        for (int i = 0; i < nw; i++) {
            if (placed_ledge >= ledge_target)
                break;
            int mid = (int)lm_floor((wide[i].start + wide[i].end) / 2.0);
            if (tiles[wide[i].row - 1][mid] == T_EMPTY && tiles[wide[i].row - 2][mid] == T_EMPTY) {
                tiles[wide[i].row - 1][mid] = T_BRAMBLE;
                placed_ledge++;
            }
        }
    }

    static struct cell reach[GRID_ROWS * GRID_COLS];
    int nreach = compute_reachable(tiles, spawn_col, spawn_row, reach);

    int upper = 0, mid_cells = 0;
    for (int i = 0; i < nreach; i++) {
        if (reach[i].r <= 8) upper++;
        if (reach[i].r >= 9 && reach[i].r <= 13) mid_cells++;
    }
    int keys_needed = (int)lm_min(8, 4 + lm_floor(depth / 3.0));
    if (!force_bridges && (upper < 6 || mid_cells < 8 || nreach < 28))
        return false;

    /* exit portal: far from the spawn, on a sturdy ledge */
    struct cell cand[GRID_ROWS * GRID_COLS];
    double score[GRID_ROWS * GRID_COLS];
    int ncand = 0;
    for (int i = 0; i < nreach; i++) {
        struct cell cell = reach[i];
        if (cell.c < 2 || cell.c > GRID_COLS - 4) continue;
        if (cell.r > 13) continue;
        if (lm_hypot(cell.c - spawn_col, cell.r - spawn_row) < 10) continue;
        int s1 = tiles[cell.r + 1][cell.c], s2 = tiles[cell.r + 1][cell.c + 1];
        if ((s1 != T_ONE_WAY && s1 != T_SOLID) || (s2 != T_ONE_WAY && s2 != T_SOLID)) continue;
        if (!(tiles[cell.r][cell.c] == T_EMPTY && tiles[cell.r][cell.c + 1] == T_EMPTY &&
              tiles[cell.r - 1][cell.c] == T_EMPTY && tiles[cell.r - 1][cell.c + 1] == T_EMPTY))
            continue;
        cand[ncand] = cell;
        score[ncand] = (16 - cell.r) * 1.8 + lm_fabs(cell.c - spawn_col);
        ncand++;
    }

    struct cell portal_cell;
    if (ncand > 0) {
        /* stable sort, highest score first (same result as Array.prototype.sort with (a,b)=>scoreB-scoreA) */
        for (int i = 1; i < ncand; i++) {
            struct cell kc = cand[i];
            double ks = score[i];
            int j = i - 1;
            while (j >= 0 && ks - score[j] > 0) {
                cand[j + 1] = cand[j];
                score[j + 1] = score[j];
                j--;
            }
            cand[j + 1] = kc;
            score[j + 1] = ks;
        }
        int top = ncand < 5 ? ncand : 5;
        portal_cell = cand[rng_pick(rng, top)];
    } else {
        int fb = spawn_left ? GRID_COLS - 5 : 3;
        tiles[16][fb] = T_EMPTY;
        tiles[16][fb + 1] = T_EMPTY;
        portal_cell = (struct cell){fb, 16};
    }

    static bool occupied[GRID_ROWS][GRID_COLS];
    memset(occupied, 0, sizeof(occupied));
    occupied[spawn_row][spawn_col] = true;
    occupied[portal_cell.r][portal_cell.c] = true;
    occupied[portal_cell.r][portal_cell.c + 1] = true;

    /* cavern keys */
    static struct cell key_pool[GRID_ROWS * GRID_COLS];
    int npool = 0;
    {
        struct cell tmp[GRID_ROWS * GRID_COLS];
        int n;
        n = 0;
        for (int i = 0; i < nreach; i++) if (reach[i].r <= 8) tmp[n++] = reach[i];
        rng_shuffle(rng, tmp, n, sizeof(struct cell));
        memcpy(key_pool + npool, tmp, (size_t)n * sizeof(struct cell)); npool += n;
        n = 0;
        for (int i = 0; i < nreach; i++) if (reach[i].r > 8 && reach[i].r <= 13) tmp[n++] = reach[i];
        rng_shuffle(rng, tmp, n, sizeof(struct cell));
        memcpy(key_pool + npool, tmp, (size_t)n * sizeof(struct cell)); npool += n;
        n = 0;
        for (int i = 0; i < nreach; i++) if (reach[i].r > 13) tmp[n++] = reach[i];
        rng_shuffle(rng, tmp, n, sizeof(struct cell));
        memcpy(key_pool + npool, tmp, (size_t)n * sizeof(struct cell)); npool += n;
    }

    struct cell placed_cells[32];
    int nplaced = 0;
    for (int i = 0; i < keys_needed; i++) {
        struct cell cell;
        bool ok = pick_spread(rng, key_pool, npool, 4.6, placed_cells, nplaced, occupied, spawn_col, spawn_row, &cell) ||
                  pick_spread(rng, reach, nreach, 2.2, placed_cells, nplaced, occupied, spawn_col, spawn_row, &cell);
        if (!ok) {
            if (!force_bridges)
                return false;
            break;
        }
        occupied[cell.r][cell.c] = true;
        placed_cells[nplaced++] = cell;
        double wx = cell.c * TILE_SIZE + TILE_SIZE * 0.5;
        double wy = cell.r * TILE_SIZE + TILE_SIZE * 0.42;
        add_collect(cv, COL_KEY, 0, 0, wx, wy, rng_next(rng) * LM_PI * 2);
    }
    if (cv->n_collect < keys_needed && !force_bridges)
        return false;

    /* Power-ups are no longer lying around the caverns (they are bought in the shop). The placement still runs so
       every random draw and reserved cell stays the same and the rest of the cavern is unchanged. */
    int pu_types[PU_COUNT] = {PU_SPRING_BOOTS, PU_LANTERN_SHIELD, PU_TEA_THERMOS, PU_STARLIGHT_MAGNET, PU_CLOCKWORK_SLOW};
    int pu_count = depth == 1 ? 2 : rng_chance(rng, 0.55) ? 2 : 1;
    rng_shuffle(rng, pu_types, PU_COUNT, sizeof(int));
    for (int i = 0; i < pu_count; i++) {
        struct cell cell;
        if (!pick_spread(rng, reach, nreach, 3.8, placed_cells, nplaced, occupied, spawn_col, spawn_row, &cell))
            break;
        occupied[cell.r][cell.c] = true;
        placed_cells[nplaced++] = cell;
        double wx = cell.c * TILE_SIZE + TILE_SIZE * 0.5;
        double wy = cell.r * TILE_SIZE + TILE_SIZE * 0.42;
        (void)wx; (void)wy;
        (void)rng_next(rng);
    }

    /* bonus relics */
    int relic_count = rng_int(rng, 3 + (int)lm_min(2, lm_floor(depth / 5.0)), 5 + (int)lm_min(2, lm_floor(depth / 6.0)));
    for (int i = 0; i < relic_count; i++) {
        struct cell cell;
        if (!pick_spread(rng, reach, nreach, 2.8, placed_cells, nplaced, occupied, spawn_col, spawn_row, &cell))
            break;
        occupied[cell.r][cell.c] = true;
        if (nplaced < 32)
            placed_cells[nplaced++] = cell;
        double wx = cell.c * TILE_SIZE + TILE_SIZE * 0.5;
        double wy = cell.r * TILE_SIZE + TILE_SIZE * 0.48;
        add_collect(cv, COL_RELIC, 0, i % 3, wx, wy, rng_next(rng) * LM_PI * 2);
    }

    generate_enemies(cv, depth, plats, nplat, spawn_col, spawn_row, portal_cell, rng);

    /* ambient hanging brass lanterns */
    static int const lantern_cols[5] = {5, 11, 16, 21, 26};
    for (int i = 0; i < 5; i++) {
        if (rng_chance(rng, 0.8)) {
            int chain = rng_int(rng, 16, 44);
            struct lm_lantern *l = &cv->lanterns[cv->n_lanterns++];
            l->x = lantern_cols[i] * TILE_SIZE + rng_int(rng, -8, 8);
            l->y = TILE_SIZE + chain;
            l->sway_phase = rng_next(rng) * LM_PI * 2;
            l->chain_length = chain;
        }
    }

    cv->spawn.x = spawn_col * TILE_SIZE + TILE_SIZE * 0.5;
    cv->spawn.y = spawn_row * TILE_SIZE + TILE_SIZE - 26;
    cv->portal.x = portal_cell.c * TILE_SIZE;
    cv->portal.y = (portal_cell.r - 1) * TILE_SIZE;
    cv->portal.col = portal_cell.c;
    cv->portal.row = portal_cell.r;
    cv->keys_total = 0;
    for (int i = 0; i < cv->n_collect; i++)
        if (cv->collect[i].kind == COL_KEY)
            cv->keys_total++;
    return true;
}

static int const k_biome_order[5] = {BIOME_TIMBER_HEARTH, BIOME_GLOW_MOSS, BIOME_AMETHYST_GROTTO, BIOME_CLOCKWORK_CELLAR, BIOME_FROST_LANTERN};

void
lm_generate_cavern(char const *seed, int depth, struct lm_cavern *out)
{
    struct rng base;
    char key[160];
    char name[96];
    int biome;

    snprintf(key, sizeof(key), "%s::meta::%d", seed, depth);
    rng_seed(&base, key);
    if (depth <= 5) {
        biome = k_biome_order[depth - 1];
    } else {
        int idx = (depth - 1 + rng_int(&base, 0, 1)) % 5;
        biome = k_biome_order[idx];
    }
    if (depth == 1) {
        snprintf(name, sizeof(name), "Central Hearth Cavern");
    } else if (depth % SHOP_INTERVAL == 0) {
        snprintf(name, sizeof(name), "Old Barnaby\xe2\x80\x99s Threshold");
    } else {
        int p = rng_pick(&base, 5);
        int n = rng_pick(&base, 5);
        snprintf(name, sizeof(name), "%s %s", lm_biomes[biome].prefixes[p], lm_biomes[biome].nouns[n]);
    }

    for (int attempt = 0; attempt < 18; attempt++) {
        struct rng rng;
        snprintf(key, sizeof(key), "%s::d%d::a%d", seed, depth, attempt);
        rng_seed(&rng, key);
        if (build_candidate(out, seed, depth, biome, name, &rng, attempt >= 13))
            return;
    }
    {
        struct rng rng;
        snprintf(key, sizeof(key), "%s::fallback::%d", seed, depth);
        rng_seed(&rng, key);
        build_candidate(out, seed, depth, biome, name, &rng, true);
    }
}
