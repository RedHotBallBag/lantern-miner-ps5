/*
 * Desktop test run: drives the whole game with a scripted pad and saves screenshots.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../game/app.h"
#include "../game/lm.h"
#include "vk_host.h"

struct ctx {
    struct app *app;
    struct app_input in;
    double now;
};

static struct ctx g;
static struct vk_host *host;
static char outdir[400];
static volatile int g_audio_run;

static void
draw(struct vkr *r, void *user)
{
    struct ctx *c = user;
    app_frame(c->app, &c->in, c->now, r);
}

static void
hlog(char const *fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    fprintf(stderr, "[app] ");
    vfprintf(stderr, fmt, a);
    fputc('\n', stderr);
    va_end(a);
}

static void *
audio_sim(void *arg)
{
    int16_t buf[256 * 2];
    struct timespec next;
    (void)arg;
    clock_gettime(CLOCK_MONOTONIC, &next);
    while (g_audio_run) {
        app_audio_pull(g.app, buf, 256);
        next.tv_nsec += 5333333;
        while (next.tv_nsec >= 1000000000) { next.tv_nsec -= 1000000000; next.tv_sec++; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    return NULL;
}

/* run n frames of 1/60 s; `pressed` on the first, `held` throughout; writes png after the last when given */
static void
frames(int n, uint32_t pressed, uint32_t held, char const *png)
{
    for (int i = 0; i < n; i++) {
        g.in.pressed = i == 0 ? pressed : 0;
        g.in.held = held;
        g.in.connected = true;
        g.now += 1.0 / 60.0;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", outdir, png ? png : "x");
        if (!vk_host_render(host, draw, &g, i == n - 1 && png ? path : NULL)) { fprintf(stderr, "render failed\n"); exit(3); }
    }
}

static struct lm_game *
game(void)
{
    return app_debug_game(g.app);
}

/* run the game for `secs` holding the given buttons, with periodic jumps */
static void
play(double secs, uint32_t dir, bool hop)
{
    int n = (int)(secs * 60);
    for (int i = 0; i < n; i++) {
        uint32_t held = dir | (hop && (i % 50) < 14 ? PAD_CROSS : 0);
        frames(1, 0, held, NULL);
    }
}

int
main(int argc, char **argv)
{
    char const *data = argc > 1 ? argv[1] : "data";
    char const *fonts = argc > 2 ? argv[2] : "assets/fonts";
    char const *assets = argc > 3 ? argv[3] : "assets";
    char const *work = argc > 4 ? argv[4] : ".";
    unsigned h = argc > 5 ? (unsigned)atoi(argv[5]) : 1080;
    struct app_env env;
    pthread_t audio;

    snprintf(outdir, sizeof(outdir), "%s/shots", work);
    host = vk_host_create(h * 16 / 9, h, fonts);
    if (!host) return 1;

    memset(&env, 0, sizeof(env));
    snprintf(env.data_dir, sizeof(env.data_dir), "%s", data);
    snprintf(env.asset_dir, sizeof(env.asset_dir), "%s", assets);
    env.log = hlog;
    g.app = app_create(&env);
    g.now = 1.0;
    g.in.connected = true;

    g_audio_run = 1;
    pthread_create(&audio, NULL, audio_sim, NULL);

    /* title */
    frames(200, 0, 0, "01_title.png"); /* past the loading splash */
    frames(2, PAD_DOWN, 0, NULL); /* no save yet: New Game, How to Play, High Scores, Settings, Exit */
    frames(2, PAD_CROSS, 0, NULL);
    for (int p = 0; p < 7; p++) {
        char name[64];
        snprintf(name, sizeof(name), "02_help_%d.png", p + 1);
        frames(30, 0, 0, name);
        frames(2, PAD_R1, 0, NULL);
    }
    frames(2, PAD_CIRCLE, 0, NULL);
    frames(2, PAD_DOWN, 0, NULL);
    frames(2, PAD_CROSS, 0, NULL);
    frames(40, 0, 0, "03_high_scores.png");
    frames(2, PAD_CIRCLE, 0, NULL);
    frames(2, PAD_DOWN, 0, NULL);
    frames(2, PAD_CROSS, 0, NULL);
    frames(40, 0, 0, "03b_settings.png");
    frames(2, PAD_CIRCLE, 0, NULL);
    frames(10, 0, 0, NULL);

    /* a run: cavern 1 */
    app_debug_start(g.app, "HEARTH-101", 1);
    frames(30, 0, 0, NULL);
    play(1.4, PAD_RIGHT, true);
    frames(2, 0, 0, "04_play_cavern1.png");
    {
        struct lm_game *gm = game();
        gm->power_time[PU_SPRING_BOOTS] = 8;
        gm->power_time[PU_STARLIGHT_MAGNET] = 6;
        gm->powers[PU_CLOCKWORK_SLOW] = 1;
        gm->player.has_shield = true;
        gm->perks[PERK_BRASS_LANTERN] = 1;
        gm->perks[PERK_GEM_RESONANCE] = 1;
        gm->relic_points = 1250;
        gm->score = 12340;
    }
    play(0.6, PAD_RIGHT, true);
    frames(2, 0, 0, "05_play_powerups.png");

    {
        struct lm_game *gm = game();
        frames(2, PAD_CIRCLE, 0, NULL);
        frames(20, 0, 0, "05b_effects.png");
        gm->player.lead = 6;
        gm->player.muddle = 6;
        frames(10, 0, 0, "05c_curses.png");
        fprintf(stderr, "[test] circle: slow carried=%d running=%.1f\n", gm->powers[PU_CLOCKWORK_SLOW], gm->power_time[PU_CLOCKWORK_SLOW]);
    }

    /* the spider in its first cavern drops when walked under, then climbs home */
    app_debug_start(g.app, "KETTLE-417", 5);
    {
        struct lm_game *gm = game();
        for (int i = 0; i < gm->n_enemies; i++) {
            if (gm->enemies[i].s.kind != EN_SPIDER)
                continue;
            double home = gm->enemies[i].s.y, low = home;
            gm->banner.active = false;
            for (int n = 0; n < 150; n++) {
                gm->player.invuln = 3600;
                gm->player.x = gm->enemies[i].s.x;
                frames(1, 0, 0, n == 55 ? "05d_spider.png" : NULL);
                if (gm->enemies[i].s.y > low)
                    low = gm->enemies[i].s.y;
            }
            gm->player.x = 40;
            frames(400, 0, 0, NULL);
            fprintf(stderr, "[test] spider: home %.0f lowest %.0f floor %.0f back at %.0f\n", home, low, gm->enemies[i].s.max_y,
                    gm->enemies[i].s.y);
        }
        /* the chest is not the same every time */
        int seen = 0, kinds = 0;
        for (int n = 0; n < 60; n++) {
            app_debug_start(g.app, "SAME", 5);
            for (int i = 0; i < game()->cavern.n_collect; i++)
                if (game()->cavern.collect[i].kind == COL_CHEST) {
                    seen++;
                    kinds |= 1 << game()->cavern.collect[i].powerup;
                }
        }
        fprintf(stderr, "[test] chests: %d of 60 loads had one, kinds bitmask %x\n", seen, kinds);
    }
    app_debug_start(g.app, "HEARTH-101", 1);
    frames(100, 0, 0, NULL);

    /* a splash costs a heart and throws the miner out; going straight back in costs another */
    {
        struct lm_game *gm = NULL;
        int col = -1, cav = 0;
        while (col < 0 && cav < 20) {
            app_debug_start(g.app, "HEARTH-101", ++cav);
            gm = game();
            for (int c = 1; c < GRID_COLS - 2 && col < 0; c++)
                if (gm->cavern.tiles[16][c] == T_PIT && gm->cavern.tiles[16][c + 1] == T_PIT && gm->cavern.tiles[16][c + 2] == T_PIT)
                    col = c;
        }
        gm->n_enemies = 0;
        gm->banner.active = false;
        gm->player.invuln = 0;
        gm->player.x = col * TILE_SIZE + 35;
        gm->player.y = 16 * TILE_SIZE - 40;
        gm->player.vx = 0;
        gm->player.vy = 100;
        gm->player.fall_top = gm->player.y;
        int lives = gm->lives, hearts = gm->hearts;
        double top = 1e9;
        for (int n = 0; n < 40; n++) {
            frames(1, 0, 0, n == 25 ? "05e_splash.png" : NULL);
            if (gm->player.y + gm->player.height < top)
                top = gm->player.y + gm->player.height;
        }
        fprintf(stderr, "[test] splash: cavern %d hearts %d -> %d, feet rose to %.0f (pit top %d)\n", cav, hearts, gm->hearts, top, 16 * TILE_SIZE);
        /* nothing to land on this time, so back in it goes */
        for (int row = 0; row < 16; row++)
            for (int c = 1; c < GRID_COLS - 1; c++)
                gm->cavern.tiles[row][c] = T_EMPTY;
        gm->n_movers = 0;
        gm->player.invuln = 0;
        gm->hearts = hearts;
        gm->player.bouncing = false;
        gm->player.x = col * TILE_SIZE + 35;
        gm->player.y = 16 * TILE_SIZE - 40;
        gm->player.vx = 0;
        gm->player.vy = 100;
        gm->player.fall_top = gm->player.y;
        frames(100, 0, 0, NULL);
        fprintf(stderr, "[test] second splash: lives %d -> %d, hearts %d, down for %.1f\n", lives, gm->lives, gm->hearts, gm->respawn_timer);
    }
    app_debug_start(g.app, "HEARTH-101", 22);
    {
        struct lm_game *gm = game();
        gm->powers[PU_SPRING_BOOTS] = gm->powers[PU_STARLIGHT_MAGNET] = gm->powers[PU_CLOCKWORK_SLOW] = 1;
        gm->relic_points = 14300;
        frames(150, 0, 0, "05f_buttons.png");
        frames(2, PAD_TRIANGLE, 0, NULL);
        frames(20, 0, 0, "05g_buttons_one_on.png");
    }
    app_debug_start(g.app, "HEARTH-101", 1);
    frames(100, 0, 0, NULL);

    /* pause */
    frames(2, PAD_OPTIONS, 0, NULL);
    frames(30, 0, 0, "06_pause.png");
    frames(2, PAD_DOWN, 0, NULL);
    frames(2, PAD_CROSS, 0, NULL);
    frames(30, 0, 0, "07_settings_pause.png");
    frames(2, PAD_CIRCLE, 0, NULL);
    frames(2, PAD_OPTIONS, 0, NULL);
    frames(30, 0, 0, NULL);

    /* the deeper caverns: every biome and guardian */
    static int const depths[] = {3, 6, 9, 12, 17, 24};
    for (unsigned i = 0; i < sizeof(depths) / sizeof(depths[0]); i++) {
        char name[64];
        app_debug_start(g.app, "KETTLE-417", depths[i]);
        game()->player.invuln = 3600;
        frames(100, 0, 0, NULL);
        play(1.2, i % 2 ? PAD_LEFT : PAD_RIGHT, true);
        snprintf(name, sizeof(name), "08_depth_%02d.png", depths[i]);
        frames(2, 0, 0, name);
    }

    /* the whole campaign, in order */
    for (int d = 1; d <= lm_level_count(); d++) {
        char name[64];
        app_debug_start(g.app, "KETTLE-417", d);
        game()->player.invuln = 3600;
        game()->banner.active = false;
        frames(150, 0, 0, NULL);
        game()->player.invuln = 0;
        snprintf(name, sizeof(name), "level_%02d.png", d);
        frames(2, 0, 0, name);
    }
    /* clear cavern 10 -> Old Barnaby's shop */
    app_debug_start(g.app, "WILLY-222", 10);
    game()->flawless = false;
    {
        struct lm_game *gm = game();
        gm->relic_points = 9000;
        gm->hearts = 2;
        gm->keys_collected = gm->cavern.keys_total;
        gm->portal_unlocked = true;
        gm->player.invuln = 3600;
        gm->player.x = gm->cavern.portal.x + 20;
        gm->player.y = gm->cavern.portal.y + 30;
        gm->player.vy = 0;
    }
    frames(120, 0, 0, NULL);
    fprintf(stderr, "[test] screen after portal: %d (3 = shop)\n", app_debug_screen(g.app));
    frames(60, 0, 0, "09_shop.png");
    frames(2, PAD_LEFT, 0, NULL);
    frames(2, PAD_LEFT, 0, NULL); /* from the Tea Thermos to Spring Boots */
    frames(2, PAD_CROSS, 0, NULL); /* buy it */
    frames(2, PAD_CROSS, 0, NULL); /* a second one must be refused */
    frames(2, PAD_RIGHT, 0, NULL);
    frames(2, PAD_CROSS, 0, NULL); /* buy Lantern Shield */
    frames(30, 0, 0, "10_shop_bought.png");
    fprintf(stderr, "[test] powers: spring=%d shield=%d points left=%d\n", game()->powers[PU_SPRING_BOOTS], game()->powers[PU_LANTERN_SHIELD], game()->relic_points);
    frames(2, PAD_L1, 0, NULL);
    frames(30, 0, 0, "10b_shop_gear.png");
    frames(2, PAD_DOWN, 0, NULL);
    frames(2, PAD_DOWN, 0, NULL);
    frames(2, PAD_DOWN, 0, NULL);
    frames(40, 0, 0, "11_shop_descend.png");
    frames(2, PAD_CROSS, 0, NULL);
    frames(40, 0, 0, "12_after_shop.png");

    frames(150, 0, 0, "12b_run_with_powers.png");
    frames(2, PAD_SQUARE, 0, NULL);
    frames(20, 0, 0, "12bb_spring_on.png");
    fprintf(stderr, "[test] square: spring carried=%d running=%.1f\n", game()->powers[PU_SPRING_BOOTS], game()->power_time[PU_SPRING_BOOTS]);
    /* a packed Tea Thermos saves the run once */
    game()->powers[PU_TEA_THERMOS] = 1;
    game()->air = 0.02;
    frames(30, 0, 0, NULL);
    fprintf(stderr, "[test] thermos: air=%.1f thermos left=%d running=%d\n", game()->air, game()->powers[PU_TEA_THERMOS], app_debug_screen(g.app));
    frames(40, 0, 0, "12c_thermos.png");
    /* save, quit to the title and Continue: the run must come back as it was */
    {
        struct lm_game *gm = game();
        int depth = gm->depth, score = gm->score, lives = gm->lives, pts = gm->relic_points;
        frames(2, PAD_OPTIONS, 0, NULL);
        for (int i = 0; i < 4; i++)
            frames(2, PAD_DOWN, 0, NULL);
        frames(2, PAD_CROSS, 0, NULL);
        frames(40, 0, 0, "15_title_continue.png");
        gm->depth = 1;
        gm->score = gm->lives = gm->relic_points = 0;
        frames(2, PAD_CROSS, 0, NULL);
        frames(40, 0, 0, NULL);
        fprintf(stderr, "[test] resume: depth %d/%d score %d/%d lives %d/%d points %d/%d screen %d\n", gm->depth, depth,
                gm->score, score, gm->lives, lives, gm->relic_points, pts, app_debug_screen(g.app));
    }
    /* out of lives with a Continue in hand: back to the start of the world */
    game()->lives = 1;
    game()->continues = 1;
    game()->air = 0.02;
    frames(150, 0, 0, "16_continue.png");
    fprintf(stderr, "[test] continue screen: %d (8 = continue)\n", app_debug_screen(g.app));
    frames(2, PAD_CROSS, 0, NULL);
    frames(100, 0, 0, NULL);
    fprintf(stderr, "[test] after continue: cavern %d lives %d continues %d\n", game()->depth, game()->lives, game()->continues);
    /* run out of air on the last life: game over */
    game()->score = 12340;
    game()->lives = 1;
    game()->air = 0.02;
    frames(120, 0, 0, NULL);
    /* a table-worthy score asks for a name first */
    frames(60, 0, 0, "13a_name_entry.png");
    frames(2, PAD_UP, 0, NULL);
    frames(2, PAD_CROSS, 0, NULL);
    frames(2, PAD_UP, 0, NULL);
    frames(2, PAD_UP, 0, NULL);
    frames(2, PAD_OPTIONS, 0, NULL);
    frames(40, 0, 0, "13b_high_scores.png");
    frames(2, PAD_CROSS, 0, NULL);
    frames(40, 0, 0, "13_game_over.png");
    frames(2, PAD_RIGHT, 0, NULL);
    frames(30, 0, 0, "14_game_over_sel.png");

    /* giving up a Continue ends the run; the last portal is a win */
    app_debug_start(g.app, "END", 3);
    game()->lives = 1;
    game()->continues = 1;
    game()->air = 0.02;
    frames(150, 0, 0, NULL);
    frames(2, PAD_RIGHT, 0, NULL);
    frames(2, PAD_CROSS, 0, NULL);
    frames(40, 0, 0, NULL);
    fprintf(stderr, "[test] gave up: screen %d running-down %d\n", app_debug_screen(g.app), game()->down);
    app_debug_start(g.app, "END", lm_level_count());
    {
        struct lm_game *gm = game();
        gm->score = 99999;
        gm->banner.active = false;
        gm->keys_collected = gm->cavern.keys_total;
        gm->portal_unlocked = true;
        gm->player.invuln = 3600;
        gm->player.x = gm->cavern.portal.x + 20;
        gm->player.y = gm->cavern.portal.y + 30;
        gm->player.vy = 0;
    }
    frames(150, 0, 0, NULL);
    fprintf(stderr, "[test] final portal: screen %d won %d\n", app_debug_screen(g.app), game()->summary.won);
    frames(2, PAD_OPTIONS, 0, NULL);
    frames(40, 0, 0, NULL);
    frames(2, PAD_CROSS, 0, NULL);
    frames(40, 0, 0, "17_the_end.png");

    /* World 2: every cavern at low water and at high */
    for (int d = 11; d <= 20; d++) {
        char name[64];
        app_debug_start(g.app, "WET", d);
        struct lm_game *gm = game();
        gm->player.invuln = 3600;
        snprintf(name, sizeof(name), "w2_%02d.png", d);
        frames(130, 0, 0, name);
        struct lm_tide const *t = &gm->cavern.tide;
        if (t->on && t->low_y > t->high_y) {
            gm->tide_clock = t->low_secs + TIDE_WARNING + (t->low_y - t->high_y) / TIDE_SPEED + 0.5;
            gm->player.invuln = 3600;
            gm->player.y = 40;
            gm->player.vy = 0;
            snprintf(name, sizeof(name), "w2_%02d_high.png", d);
            frames(20, 0, 0, name);
            gm->tide_clock = t->low_secs + 0.6;
            snprintf(name, sizeof(name), "w2_%02d_warn.png", d);
            frames(8, 0, 0, d == 11 ? name : NULL);
        }
    }
    /* cavern pictures for the README */
    static int const pictured[4] = {9, 10, 12, 19};
    for (int i = 0; i < 4; i++) {
        char name[64];
        app_debug_start(g.app, "PIC", pictured[i]);
        game()->player.invuln = 3600;
        snprintf(name, sizeof(name), "level_%02d.png", pictured[i]);
        frames(130, 0, 0, name);
    }
    /* the first rise comes the same short while after any water cavern appears */
    for (int d = 11; d <= 20; d += 5) {
        app_debug_start(g.app, "WET", d);
        struct lm_game *gm = game();
        int n = 0;
        while (gm->tide_phase != TIDE_RISING && n < 1200) {
            gm->player.invuln = 3600;
            frames(1, 0, 0, NULL);
            n++;
        }
        fprintf(stderr, "[test] first rise: cavern %d after %.1f s\n", d, n / 60.0);
    }
    /* the flood takes a heart from anyone left on the floor */
    app_debug_start(g.app, "WET", 11);
    {
        struct lm_game *gm = game();
        gm->n_enemies = 0;
        gm->banner.active = false;
        gm->player.invuln = 0;
        gm->player.x = 12 * TILE_SIZE;
        gm->player.y = 17 * TILE_SIZE - 26;
        gm->player.fall_top = gm->player.y;
        double t0 = gm->tide_clock;
        int h = gm->hearts;
        while (gm->hearts == h && gm->tide_clock - t0 < 40)
            frames(1, 0, 0, NULL);
        fprintf(stderr, "[test] tide: hit after %.1f s with water at %.0f\n", gm->tide_clock, gm->water_y);
        frames(14, 0, 0, "w2_splash.png");
    }
    /* a raft holds for a moment, then goes under */
    app_debug_start(g.app, "WET", 12);
    {
        struct lm_game *gm = game();
        gm->n_enemies = 0;
        gm->banner.active = false;
        gm->player.invuln = 0;
        gm->player.x = gm->movers[0].x + 30;
        gm->player.y = gm->movers[0].y - 28;
        gm->player.vy = 0;
        gm->player.fall_top = gm->player.y;
        int h = gm->hearts, n = 0;
        frames(75, 0, 0, "w2_raft.png");
        for (n = 75; n < 400 && gm->hearts == h; n++)
            frames(1, 0, 0, NULL);
        fprintf(stderr, "[test] raft: went under after %.2f s, hearts %d -> %d\n", n / 60.0, h, gm->hearts);
    }
    /* the plugs */
    app_debug_start(g.app, "WET", 19);
    {
        struct lm_game *gm = game();
        gm->n_enemies = 0;
        gm->banner.active = false;
        for (int pull = 0; pull < 2; pull++) {
            int left = pull;
            for (int i = 0; i < gm->cavern.n_collect; i++)
                if (gm->cavern.collect[i].kind == COL_PLUG && !gm->cavern.collect[i].collected && left-- <= 0 + pull) {
                    gm->player.invuln = 3600;
                    gm->player.x = gm->cavern.collect[i].x - 10;
                    gm->player.y = gm->cavern.collect[i].y - 13;
                    gm->player.vy = 0;
                    gm->player.fall_top = gm->player.y;
                    break;
                }
            frames(20, 0, 0, NULL);
            gm->player.x = 40;
            gm->player.y = 40;
            gm->player.vy = 0;
            gm->player.fall_top = 40;
            frames(280, 0, 0, pull ? "w2_19_drained2.png" : "w2_19_drained1.png");
            fprintf(stderr, "[test] plug %d: water now %.0f\n", pull + 1, gm->water_y);
        }
    }
    /* a fish leaps and falls back */
    app_debug_start(g.app, "WET", 11);
    {
        struct lm_game *gm = game();
        gm->banner.active = false;
        for (int i = 0; i < gm->n_enemies; i++) {
            if (gm->enemies[i].s.kind != EN_FISH)
                continue;
            double top = 1e9;
            int shot = 0;
            for (int n = 0; n < 420; n++) {
                gm->player.invuln = 3600;
                gm->tide_clock = gm->cavern.tide.low_secs + TIDE_WARNING + 2; /* held at high water */
                frames(1, 0, 0, !shot && gm->enemies[i].is_charging && gm->enemies[i].vy > -120 ? (shot = 1, "w2_fish.png") : NULL);
                if (gm->enemies[i].s.y < top)
                    top = gm->enemies[i].s.y;
            }
            fprintf(stderr, "[test] fish: leapt to %.0f (asked for %.0f)\n", top, gm->enemies[i].anchor_y);
            break;
        }
    }
    /* Cavern Select must leave the real save alone */
    app_debug_start(g.app, "REAL", 7);
    frames(10, 0, 0, NULL);
    frames(2, PAD_OPTIONS, 0, NULL);
    for (int i = 0; i < 4; i++)
        frames(2, PAD_DOWN, 0, NULL);
    frames(2, PAD_CROSS, 0, NULL); /* Save & Quit */
    frames(20, 0, 0, "w2_title.png");
    {
        char path[600], before[2048] = "", after[2048] = "";
        snprintf(path, sizeof(path), "%s/run.sav", argv[1]);
        FILE *f = fopen(path, "r");
        if (f) {
            size_t n = fread(before, 1, sizeof(before) - 1, f);
            before[n] = 0;
            fclose(f);
        }
        app_debug_goto(g.app, 10);
        frames(30, 0, 0, NULL);
        frames(2, PAD_SQUARE, 0, NULL);
        for (int i = 0; i < 14; i++)
            frames(2, PAD_DOWN, 0, NULL);
        frames(20, 0, 0, "w2_select.png");
        frames(2, PAD_CROSS, 0, NULL);
        frames(200, 0, 0, "w2_practice.png");
        fprintf(stderr, "[test] practice: cavern %d practice %d lives %d spring %d\n", game()->depth, game()->practice, game()->lives,
                game()->powers[PU_SPRING_BOOTS]);
        game()->air = 0.02;
        frames(120, 0, 0, NULL);
        frames(2, PAD_OPTIONS, 0, NULL);
        for (int i = 0; i < 4; i++)
            frames(2, PAD_DOWN, 0, NULL);
        frames(2, PAD_CROSS, 0, NULL);
        frames(20, 0, 0, NULL);
        f = fopen(path, "r");
        if (f) {
            size_t n = fread(after, 1, sizeof(after) - 1, f);
            after[n] = 0;
            fclose(f);
        }
        fprintf(stderr, "[test] practice: back on screen %d (10 = list), real save %s (%d bytes)\n", app_debug_screen(g.app),
                strcmp(before, after) ? "CHANGED" : "untouched", (int)strlen(before));
    }

    g_audio_run = 0;
    pthread_join(audio, NULL);
    app_shutdown(g.app);
    printf("shots in %s\n", outdir);
    return 0;
}
