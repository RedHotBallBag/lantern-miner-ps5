/* Scripted desktop session of the whole app (software Vulkan): writes PNGs of each screen and exercises the simulation,
 * the shop and game-over flow. Development only. */
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
    frames(2, PAD_DOWN, 0, NULL);
    frames(40, 0, 0, "02_title_seed.png");
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
        gm->player.spring = 8;
        gm->player.has_shield = true;
        gm->player.magnet = 5;
        gm->perks[PERK_BRASS_LANTERN] = 1;
        gm->perks[PERK_GEM_RESONANCE] = 2;
        gm->relic_points = 1250;
        gm->score = 12340;
    }
    play(0.6, PAD_RIGHT, true);
    frames(2, 0, 0, "05_play_powerups.png");

    /* pause */
    frames(2, PAD_OPTIONS, 0, NULL);
    frames(30, 0, 0, "06_pause.png");
    frames(2, PAD_DOWN, 0, NULL);
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

    /* every built cavern in the order this seed deals them */
    for (int d = 1; d <= 20; d++) {
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
    {
        struct lm_game *gm = game();
        gm->relic_points = 3000;
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
    frames(2, PAD_CROSS, 0, NULL); /* buy Spring Boots (900) */
    frames(2, PAD_RIGHT, 0, NULL);
    frames(2, PAD_CROSS, 0, NULL); /* buy Lantern Shield (1300) */
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

    frames(60, 0, 0, "12b_run_with_powers.png");
    /* a packed Tea Thermos saves the run once */
    game()->powers[PU_TEA_THERMOS] = 1;
    game()->air = 0.02;
    frames(30, 0, 0, NULL);
    fprintf(stderr, "[test] thermos: air=%.1f thermos left=%d running=%d\n", game()->air, game()->powers[PU_TEA_THERMOS], app_debug_screen(g.app));
    frames(40, 0, 0, "12c_thermos.png");
    /* run out of air on the last life: game over */
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

    g_audio_run = 0;
    pthread_join(audio, NULL);
    app_shutdown(g.app);
    printf("shots in %s\n", outdir);
    return 0;
}
