/*
 * Checks that every cavern and its mirror can be finished, and in the air allowed, by searching
 * button presses with the real engine. Foes are left out, so it tests the layout, not the difficulty.
 * Run it with tools/check-levels.sh.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lm.h"

#define MAX_NODES 120000
#define HZ 120

struct snap { struct lm_player p; struct lm_mover m[LM_MAX_MOVERS]; double sx, sy, t; };
struct node { struct snap s; float score; };

static struct lm_game g;
static struct node *nodes;
static int *heap, n_nodes, n_heap;
static uint8_t *seen;
#define SEEN_BITS (1u << 24)

static void
push(int id)
{
    int i = n_heap++;
    while (i > 0 && nodes[heap[(i - 1) / 2]].score > nodes[id].score) {
        heap[i] = heap[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    heap[i] = id;
}

static int
pop(void)
{
    int top = heap[0], last = heap[--n_heap], i = 0;
    for (;;) {
        int k = i * 2 + 1;
        if (k >= n_heap) break;
        if (k + 1 < n_heap && nodes[heap[k + 1]].score < nodes[heap[k]].score) k++;
        if (nodes[heap[k]].score >= nodes[last].score) break;
        heap[i] = heap[k];
        i = k;
    }
    heap[i] = last;
    return top;
}

static void
grab(struct snap *s)
{
    s->p = g.player;
    memcpy(s->m, g.movers, sizeof(s->m));
    s->sx = g.safe_x;
    s->sy = g.safe_y;
    s->t = g.elapsed;
}

static void
restore(struct snap const *s)
{
    g.player = s->p;
    memcpy(g.movers, s->m, sizeof(s->m));
    g.safe_x = s->sx;
    g.safe_y = s->sy;
    g.elapsed = s->t;
    memset(g.crumble, 0, sizeof(g.crumble));
    g.prev_jump = g.in_jump = false;
    /* nothing the search does may finish the cavern or end the run */
    g.keys_collected = 0;
    g.portal_unlocked = g.transition_locked = g.paused = false;
    g.pending_advance = g.respawn_timer = 0;
    g.events = 0;
    g.lives = 5;
    g.down = false;
    g.n_particles = g.n_floats = 0;
}

/* dir -1/0/1, vert -1 up / 1 down; the jump is held from the start, the direction begins after `delay` seconds */
struct act { int dir, vert; bool jump; double secs, delay; };
static struct act const acts[] = {
    {-1, 0, 0, 0.08, 0}, {1, 0, 0, 0.08, 0}, {-1, 0, 0, 0.25, 0}, {1, 0, 0, 0.25, 0}, {0, 0, 0, 0.35, 0},
    {0, -1, 0, 0.2, 0}, {0, 1, 0, 0.2, 0},
    {-1, 0, 1, 0.9, 0}, {1, 0, 1, 0.9, 0}, {0, 0, 1, 0.7, 0}, {-1, 0, 1, 0.9, 0.22}, {1, 0, 1, 0.9, 0.22},
    {-1, 0, 1, 0.45, 0}, {1, 0, 1, 0.45, 0},
};

/* Plays one action. Returns false if it hurt (spikes or a pit); *hit is set when the target was touched. */
static bool
play(struct act const *a, int target, bool *hit)
{
    int steps = (int)(a->secs * HZ), delay = (int)(a->delay * HZ);
    g.player.invuln = 0;
    g.hearts = g.max_hearts = 5;
    for (int i = 0; i < steps + HZ; i++) {
        bool over = i >= steps;
        if (over && (g.player.grounded || g.player.climbing))
            break;
        int dir = i >= delay ? a->dir : 0;
        lm_set_input(&g, dir < 0, dir > 0, !over && a->vert < 0, !over && a->vert > 0, a->jump && !over);
        g.air = 100;
        g.hitstop = 0;
        g.banner.active = false;
        lm_step(&g, 1.0 / HZ);
        if (g.player.invuln > 0 || g.respawn_timer > 0)
            return false;
        if (target >= 0) {
            if (g.cavern.collect[target].collected)
                *hit = true; /* only counts if the miner then lands safely */
        } else {
            double px = g.player.x + g.player.width * 0.5, py = g.player.y + g.player.height * 0.5;
            if (lm_fabs(px - (g.cavern.portal.x + TILE_SIZE)) < 24 && lm_fabs(py - (g.cavern.portal.y + TILE_SIZE)) < 28) {
                *hit = true; /* walking into the portal ends the cavern there and then */
                return true;
            }
        }
    }
    if (*hit && !(g.player.grounded || g.player.climbing))
        return false;
    return true;
}

/* Searches from `from` to the target collectable (or the portal when target < 0); on success `from` moves there. */
static bool
search(struct snap *from, int target)
{
    double tx = target >= 0 ? g.cavern.collect[target].x : g.cavern.portal.x + TILE_SIZE;
    double ty = target >= 0 ? g.cavern.collect[target].y : g.cavern.portal.y + TILE_SIZE;
    memset(seen, 0, SEEN_BITS / 8);
    n_nodes = n_heap = 0;
    nodes[0].s = *from;
    nodes[0].score = 0;
    n_nodes = 1;
    push(0);
    while (n_heap) {
        int id = pop();
        for (unsigned k = 0; k < sizeof(acts) / sizeof(acts[0]); k++) {
            bool hit = false;
            restore(&nodes[id].s);
            for (int i = 0; i < g.cavern.n_collect; i++) g.cavern.collect[i].collected = false;
            if (!play(&acts[k], target, &hit))
                continue;
            if (hit) {
                grab(from);
                return true;
            }
            struct lm_player const *p = &g.player;
            uint64_t key = (uint64_t)(p->x / 8) | (uint64_t)(p->y / 8) << 8 | (uint64_t)(p->rider + 1) << 16 |
                           (uint64_t)p->climbing << 20 | (uint64_t)p->grounded << 21 |
                           (uint64_t)(g.n_movers ? ((long)(g.elapsed / 0.5)) % 64 : 0) << 22 |
                           (uint64_t)(p->vx > 60 ? 1 : p->vx < -60 ? 2 : 0) << 28; /* a run-up changes what a jump reaches */
            uint32_t h = (uint32_t)((key * 0x9e3779b97f4a7c15ull) >> 40) & (SEEN_BITS - 1);
            if (seen[h / 8] & (1 << (h % 8)))
                continue;
            seen[h / 8] |= (uint8_t)(1 << (h % 8));
            if (n_nodes >= MAX_NODES)
                return false;
            grab(&nodes[n_nodes].s);
            nodes[n_nodes].score = (float)(lm_hypot(p->x + 10 - tx, p->y + 13 - ty) + n_nodes * 0.02);
            push(n_nodes++);
        }
    }
    return false;
}

static bool
solve(int level, bool mirror, int depth)
{
    lm_init(&g, 1);
    lm_start_run(&g, "CHECK-100");
    g.depth = depth;
    lm_build_level(level, mirror, "CHECK-100", depth, &g.cavern);
    g.cavern.n_enemies = g.n_enemies = 0;
    g.n_movers = g.cavern.n_movers;
    memcpy(g.movers, g.cavern.movers, sizeof(g.movers));
    g.player.x = g.cavern.spawn.x - 10;
    g.player.y = g.cavern.spawn.y;
    g.safe_x = g.player.x;
    g.safe_y = g.player.y;

    struct snap at;
    grab(&at);
    bool done[LM_MAX_COLLECT] = {0}, ok = true;
    /* chests too: any chest that can appear must be reachable. Nearest first. */
    for (;;) {
        int best = -1;
        double bd = 1e18;
        for (int i = 0; i < g.cavern.n_collect; i++) {
            double d = lm_hypot(g.cavern.collect[i].x - at.p.x, g.cavern.collect[i].y - at.p.y);
            if (!done[i] && d < bd) { bd = d; best = i; }
        }
        if (best < 0)
            break;
        done[best] = true;
        if (!search(&at, best)) {
            printf("  %s%s depth %d: cannot reach %s at column %d row %d\n", g.cavern.name, mirror ? " (mirrored)" : "", depth,
                   g.cavern.collect[best].kind == COL_KEY ? "key" : g.cavern.collect[best].kind == COL_CHEST ? "chest" : g.cavern.collect[best].kind == COL_AIR ? "air bottle" : "gem",
                   (int)(g.cavern.collect[best].x / TILE_SIZE), (int)(g.cavern.collect[best].y / TILE_SIZE));
            ok = false;
        }
    }
    if (!search(&at, -1)) {
        printf("  %s%s depth %d: cannot reach the portal\n", g.cavern.name, mirror ? " (mirrored)" : "", depth);
        ok = false;
    }
    if (depth != 50) {
        /* again from the start, taking only the keys and the air bottle */
        lm_init(&g, 1);
        lm_start_run(&g, "CHECK-100");
        g.depth = depth;
        lm_build_level(level, mirror, "CHECK-100", depth, &g.cavern);
        g.cavern.n_enemies = g.n_enemies = 0;
        g.n_movers = g.cavern.n_movers;
        memcpy(g.movers, g.cavern.movers, sizeof(g.movers));
        g.player.x = g.cavern.spawn.x - 10;
        g.player.y = g.cavern.spawn.y;
        g.safe_x = g.player.x;
        g.safe_y = g.player.y;
        grab(&at);
        memset(done, 0, sizeof(done));
        for (;;) {
            int best = -1;
            double bd = 1e18;
            for (int i = 0; i < g.cavern.n_collect; i++) {
                int kind = g.cavern.collect[i].kind;
                double d = lm_hypot(g.cavern.collect[i].x - at.p.x, g.cavern.collect[i].y - at.p.y);
                if (!done[i] && (kind == COL_KEY || kind == COL_AIR) && d < bd) { bd = d; best = i; }
            }
            if (best < 0)
                break;
            done[best] = true;
            if (!search(&at, best))
                ok = false;
        }
        if (!search(&at, -1))
            ok = false;
        double most = g.cavern.air_secs * (1 + AIR_BOTTLE / MAX_AIR);
        printf("  %s%s: keys and door %.0f s, air %.0f s (%.0f s with the bottle)\n", g.cavern.name,
               mirror ? " (mirrored)" : "", at.t, g.cavern.air_secs, most);
        if (at.t > most * 0.95) {
            printf("  not enough air, needs a tank of %.0f\n", at.t / 0.95 / (1 + AIR_BOTTLE / MAX_AIR) + 1);
            ok = false;
        }
    }
    /* foes back in: standing at the start for a few seconds must be safe, even with every foe present */
    lm_init(&g, 1);
    lm_start_run(&g, "CHECK-100");
    g.depth = depth;
    lm_build_level(level, mirror, "CHECK-100", depth, &g.cavern);
    g.n_enemies = g.cavern.n_enemies;
    for (int i = 0; i < g.n_enemies; i++) {
        memset(&g.enemies[i], 0, sizeof(g.enemies[i]));
        g.enemies[i].s = g.cavern.enemies[i];
        g.enemies[i].width = g.enemies[i].height = 22;
        g.enemies[i].anchor_x = g.enemies[i].s.x;
        g.enemies[i].anchor_y = g.enemies[i].s.y;
        g.enemies[i].timer = 1.5;
    }
    g.n_movers = g.cavern.n_movers;
    memcpy(g.movers, g.cavern.movers, sizeof(g.movers));
    g.player.x = g.cavern.spawn.x - 10;
    g.player.y = g.cavern.spawn.y;
    g.player.invuln = 0.9;
    for (int i = 0; i < 4 * HZ; i++) {
        g.banner.active = false;
        lm_step(&g, 1.0 / HZ);
        if (g.hearts < g.max_hearts) {
            printf("  %s%s depth %d: hurt after %.1f s just standing at the start\n", g.cavern.name, mirror ? " (mirrored)" : "",
                   depth, i / (double)HZ);
            ok = false;
            break;
        }
    }
    return ok;
}

int
main(int argc, char **argv)
{
    int first = argc > 1 ? atoi(argv[1]) : 0, last = argc > 2 ? atoi(argv[2]) : lm_level_count() - 1;
    int bad = 0, total = 0;
    nodes = malloc(MAX_NODES * sizeof(*nodes));
    heap = malloc(MAX_NODES * sizeof(*heap));
    seen = malloc(SEEN_BITS / 8);
    setvbuf(stdout, NULL, _IOLBF, 0);
    for (int i = first; i <= last; i++) {
        /* the depth it first turns up at, and a deep lap (faster platforms, quicker crumbling) */
        int depths[2] = {i + 1, 50}; /* its place in the campaign, and a late-game speed */
        for (int d = 0; d < 2; d++)
            for (int m = 0; m < 2; m++, total++)
                if (!solve(i, m, depths[d]))
                    bad++;
        printf("map %2d checked\n", i);
    }
    printf("%d of %d cavern variants can be finished\n", total - bad, total);
    return bad != 0;
}
