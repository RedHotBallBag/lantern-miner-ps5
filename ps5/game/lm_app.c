/*
 * Lantern Miner: screens, controller handling, saving and the frame loop.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "app.h"
#include "lm.h"
#include "lm_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STB_IMAGE_IMPLEMENTATION
#include "../gfx/third_party/stb_image.h"

struct app;
static void scores_load(struct app *a);
enum { SC_TITLE = APP_SC_TITLE, SC_PLAY, SC_PAUSE, SC_SHOP, SC_OVER, SC_SETTINGS, SC_NAME, SC_SCORES, SC_CONTINUE, SC_HELP, SC_LEVELS };
enum { IMG_TITLE, IMG_CAMP, IMG_SHOP, IMG_QR, IMG_COUNT };

#define JUMP_BUTTONS (PAD_CROSS | PAD_R1 | PAD_L1 | PAD_R2 | PAD_L2) /* the other face buttons start powers */

struct settings {
    int music, sfx;   /* 0..10 */
    bool muted;
    int scan;         /* 0 off, 1 subtle, 2 strong */
    bool bloom, rumble, shake;
};

struct stats {
    int high_score, best_depth, total_relics, total_caverns;
};

struct dust { float x, y, vx, vy, ph, sz; };
struct hs_entry { char name[9]; int score, depth; };

struct app {
    struct app_env env;
    struct lm_game g;
    struct settings set;
    struct stats st;
    struct hs_entry hs[10]; /* high score table, best first */
    int hs_n, hs_new;       /* entries used; index of the entry just added (-1 none) */
    char hs_last[9];        /* last name typed, offered next time */
    char name_buf[9];
    int name_cur;
    bool hs_from_over;      /* table opened from the run summary flow (else from the title) */

    int screen, prev_screen;
    double now, last_now, scr_t0, run_t0;
    bool exit_req;

    /* images */
    int img[IMG_COUNT];
    int help_page, help_from; /* How to Play: page shown, and the screen to go back to */
    int img_loaded;
    uint32_t spare;

    /* the run in progress */
    bool run_active, has_save, confirm_new;
    int save_depth, save_lives, save_continues;

    /* title */
    int title_sel;
    float menu_anim[8];
    int level_sel;
    bool level_stocked; /* Cavern Select: start with every power and plenty of points */
    struct dust dust[80];

    /* pause / over / settings / shop */
    int pause_sel, over_sel, set_sel;
    float row_anim[16];
    int shop_sel, shop_page; /* page 0 = powers and lives, 1 = supplies and gear */
    int shop_air_bonus, shop_relic_bonus;
    char shop_line[220];
    double shop_line_t;
    double shop_flash[SHOP_COUNT];
    double over_timer;
    bool jump_lock;
    bool over_pending;
    bool dirty;
    double last_save;
    double boot_t0; /* first frame time, for the loading splash */
    double sim_acc; /* fixed-timestep accumulator */
    float toast_t;
    char toast[80];
    double buzz_t; /* >0: a test rumble from the settings screen */
};

/* ---------------------------------------------------------------- settings io */

static char *
data_path(struct app const *a, char const *name, char *buf, size_t n)
{
    snprintf(buf, n, "%s/%s", a->env.data_dir, name);
    return buf;
}

static void
save_all(struct app *a)
{
    char p[300], tmp[310];
    data_path(a, "save.cfg", p, sizeof(p));
    snprintf(tmp, sizeof(tmp), "%s.tmp", p);
    FILE *f = fopen(tmp, "w");
    if (!f)
        return;
    fprintf(f, "high_score=%d\nbest_depth=%d\ntotal_relics=%d\ntotal_caverns=%d\n", a->st.high_score, a->st.best_depth, a->st.total_relics, a->st.total_caverns);
    fprintf(f, "music=%d\nsfx=%d\nmuted=%d\nscan=%d\nbloom=%d\nrumble=%d\nshake=%d\n", a->set.music, a->set.sfx, a->set.muted, a->set.scan, a->set.bloom, a->set.rumble, a->set.shake);
    if (fclose(f) != 0)
        return;
    rename(tmp, p); /* a crash mid-write can no longer leave a half-empty save */
    a->dirty = false;
    a->last_save = a->now;
}

static void
load_all(struct app *a)
{
    char p[300], line[128];
    FILE *f = fopen(data_path(a, "save.cfg", p, sizeof(p)), "r");
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = 0;
        int v = atoi(eq + 1);
        if (!strcmp(line, "high_score")) a->st.high_score = v;
        else if (!strcmp(line, "best_depth")) a->st.best_depth = v;
        else if (!strcmp(line, "total_relics")) a->st.total_relics = v;
        else if (!strcmp(line, "total_caverns")) a->st.total_caverns = v;
        else if (!strcmp(line, "music")) a->set.music = (int)lm_clamp(v, 0, 10);
        else if (!strcmp(line, "sfx")) a->set.sfx = (int)lm_clamp(v, 0, 10);
        else if (!strcmp(line, "muted")) a->set.muted = v != 0;
        else if (!strcmp(line, "scan")) a->set.scan = (int)lm_clamp(v, 0, 2);
        else if (!strcmp(line, "bloom")) a->set.bloom = v != 0;
        else if (!strcmp(line, "rumble")) a->set.rumble = v != 0;
        else if (!strcmp(line, "shake")) a->set.shake = v != 0;
    }
    fclose(f);
}

static void
apply_settings(struct app *a)
{
    lm_audio_set_volumes(a->set.music / 10.0f, a->set.sfx / 10.0f);
    lm_audio_set_muted(a->set.muted);
    a->g.shake_enabled = a->set.shake;
    lm_set_records(&a->g, a->st.high_score, a->st.best_depth);
}

/* ------------------------------------------------------------------ helpers */

static void
go(struct app *a, int screen)
{
    a->prev_screen = a->screen;
    a->screen = screen;
    a->scr_t0 = a->now;
}

/* ---------------------------------------------------------------- the run */

/* Writes the run in progress so Continue can pick it up. */
static void
save_run(struct app *a)
{
    char p[300], tmp[310];
    if (!a->run_active || a->g.practice) /* a test run must never touch the real save */
        return;
    data_path(a, "run.sav", p, sizeof(p));
    snprintf(tmp, sizeof(tmp), "%s.tmp", p);
    FILE *f = fopen(tmp, "w");
    if (!f)
        return;
    lm_save_run(&a->g, f, a->screen == SC_SHOP);
    if (fclose(f) != 0)
        return;
    rename(tmp, p);
    a->has_save = true;
    a->save_depth = a->g.depth;
    a->save_lives = a->g.lives;
    a->save_continues = a->g.continues;
}

static void
end_run(struct app *a)
{
    char p[300];
    a->run_active = false;
    if (a->g.practice)
        return;
    a->has_save = false;
    remove(data_path(a, "run.sav", p, sizeof(p)));
}

static void
start_run(struct app *a)
{
    char seed[40];
    snprintf(seed, sizeof(seed), "RUN-%06d", (int)(lm_rand(&a->g) * 1000000));
    lm_start_run(&a->g, seed);
    a->jump_lock = true;
    a->run_t0 = a->now;
    a->over_pending = false;
    a->run_active = true;
    go(a, SC_PLAY);
    save_run(a);
}

static void open_shop(struct app *a);
static void open_help(struct app *a);

/* Back to the list after a test run, with the real records put back. */
static void
leave_practice(struct app *a)
{
    a->run_active = false;
    a->over_pending = false;
    a->g.running = false;
    a->g.events = 0;
    lm_set_records(&a->g, a->st.high_score, a->st.best_depth);
    lm_audio_set_music_mode(MUSIC_TITLE, 1.0);
    go(a, SC_LEVELS);
}

static void
resume_run(struct app *a)
{
    char p[300];
    bool in_shop = false;
    FILE *f = fopen(data_path(a, "run.sav", p, sizeof(p)), "r");
    bool ok = f && lm_load_run(&a->g, f, &in_shop);
    if (f)
        fclose(f);
    if (!ok) {
        end_run(a);
        return;
    }
    a->run_active = true;
    a->jump_lock = true;
    a->run_t0 = a->now - 100;
    a->over_pending = false;
    if (a->g.down) {
        a->over_sel = 0;
        go(a, SC_CONTINUE);
    } else if (in_shop) {
        open_shop(a);
    } else {
        go(a, SC_PLAY);
    }
}

static void
toast(struct app *a, char const *msg)
{
    snprintf(a->toast, sizeof(a->toast), "%s", msg);
    a->toast_t = 1.6f;
}

static void
load_images(struct app *a, struct vkr *r)
{
    static char const *const files[IMG_COUNT] = {"art/title.jpg", "art/camp.jpg", "art/shop.jpg", "art/qr.png"};
    if (a->img_loaded >= IMG_COUNT)
        return;
    int i = a->img_loaded++;
    char p[300];
    FILE *f;
    snprintf(p, sizeof(p), "%s/%s", a->env.asset_dir, files[i]);
    f = fopen(p, "rb");
    if (!f) {
        if (a->env.log) a->env.log("art: cannot open %s", p);
        return;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)len);
    if (data && fread(data, 1, (size_t)len, f) == (size_t)len) {
        int w, h, comp;
        unsigned char *pix = stbi_load_from_memory(data, (int)len, &w, &h, &comp, 4);
        if (pix) {
            a->img[i] = vkr_image_create(r, (uint32_t)w, (uint32_t)h, (uint32_t const *)pix);
            if (a->env.log) a->env.log("art: %s %dx%d -> texture %d", files[i], w, h, a->img[i]);
            stbi_image_free(pix);
        } else if (a->env.log) {
            a->env.log("art: decode failed %s: %s", files[i], stbi_failure_reason());
        }
    }
    free(data);
    fclose(f);
}

static bool
nav(struct app *a, struct app_input const *in, int *sel, int count)
{
    if (in->pressed & PAD_UP) {
        *sel = (*sel + count - 1) % count;
        lm_audio_ui_move();
        return true;
    }
    if (in->pressed & PAD_DOWN) {
        *sel = (*sel + 1) % count;
        lm_audio_ui_move();
        return true;
    }
    (void)a;
    return false;
}

static void
anim_rows(struct app *a, float *rows, int count, int sel, double dt)
{
    (void)a;
    for (int i = 0; i < count; i++) {
        float target = i == sel ? 1.0f : 0.0f;
        rows[i] += (target - rows[i]) * (float)lm_min(1.0, dt * 14.0);
    }
}

/* ------------------------------------------------------------------ create */

struct app *
app_create(struct app_env const *env)
{
    struct app *a = calloc(1, sizeof(*a));
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    if (!a)
        return NULL;
    a->env = *env;
    for (int i = 0; i < IMG_COUNT; i++)
        a->img[i] = -1;
    a->set = (struct settings){10, 10, false, 1, true, true, true};
    lm_audio_init();
    {
        char p[300];
        snprintf(p, sizeof(p), "%s/music/theme.wav", env->asset_dir);
        bool ok = lm_audio_load_title_track(p);
        if (env->log)
            env->log(ok ? "music: title theme %s" : "music: cannot load %s, using the synth tune", p);
    }
    lm_init(&a->g, (uint32_t)ts.tv_nsec ^ (uint32_t)ts.tv_sec * 2654435761u);
    load_all(a);
    scores_load(a);
    a->hs_new = -1;
    {
        char p[300];
        FILE *f = fopen(data_path(a, "run.sav", p, sizeof(p)), "r");
        a->has_save = f != NULL;
        if (f)
            fclose(f);
    }
    apply_settings(a);
    for (int i = 0; i < 80; i++) {
        a->dust[i] = (struct dust){(float)(lm_rand(&a->g) * 1920), (float)(lm_rand(&a->g) * 1080), (float)((lm_rand(&a->g) - 0.5) * 14),
                                   (float)(-6 - lm_rand(&a->g) * 12), (float)(lm_rand(&a->g) * LM_TAU), (float)(5 + lm_rand(&a->g) * 7)};
    }
    a->screen = SC_TITLE;
    lm_audio_set_music_mode(MUSIC_TITLE, 1.0);
    if (env->log)
        env->log("app: ready, save %s", env->data_dir);
    return a;
}

void
app_audio_pull(struct app *a, int16_t *lr, size_t frames)
{
    (void)a;
    lm_audio_pull(lr, frames);
}

bool
app_rumble(struct app const *a, uint8_t *large, uint8_t *small)
{
    double s = a->buzz_t > 0 ? 0.8 : a->g.rumble_time > 0 ? a->g.rumble_strength : 0;
    if (!a->set.rumble || s <= 0) {
        *large = *small = 0;
        return false;
    }
    /* the motors do not turn for tiny values, so the quietest event still starts well above zero */
    *large = (uint8_t)lm_min(255, 70 + s * 185);
    *small = (uint8_t)lm_min(255, 90 + s * 165);
    return true;
}

bool
app_wants_exit(struct app const *a)
{
    return a->exit_req;
}

void
app_shutdown(struct app *a)
{
    save_run(a);
    save_all(a);
}

struct lm_game *app_debug_game(struct app *a) { return &a->g; }
int app_debug_screen(struct app const *a) { return a->screen; }

void
app_debug_goto(struct app *a, int screen)
{
    if (screen == SC_PAUSE)
        lm_set_paused(&a->g, true);
    if (screen == SC_SHOP) {
        a->g.paused = true;
        a->shop_air_bonus = 1840;
        a->shop_sel = SHOP_SPRING_BOOTS;
        a->shop_page = 0;
        snprintf(a->shop_line, sizeof(a->shop_line), "Pull up a stool, miner! Powers, spare lanterns and more. Regulars get a discount, so keep coming back deeper.");
        a->shop_line_t = a->now - 10;
    }
    if (screen == SC_TITLE)
        lm_audio_set_music_mode(MUSIC_TITLE, 1.0);
    go(a, screen);
}

void
app_debug_start(struct app *a, char const *seed, int depth)
{
    lm_start_run(&a->g, seed);
    a->run_active = true;
    a->over_pending = false;
    a->jump_lock = true;
    a->run_t0 = a->now;
    go(a, SC_PLAY);
    if (depth > 1) {
        /* arrive through the normal path: leave-shop style load */
        a->g.depth = depth - 1;
        lm_leave_shop(&a->g);
    }
}

/* ----------------------------------------------------------------- events */

static void
update_records(struct app *a)
{
    lm_set_records(&a->g, a->st.high_score > a->g.high_score ? a->st.high_score : a->g.high_score,
                   a->st.best_depth > a->g.best_depth ? a->st.best_depth : a->g.best_depth);
}

static void
open_shop(struct app *a)
{
    a->shop_air_bonus = a->g.adv.air_bonus;
    a->shop_relic_bonus = a->shop_air_bonus > 0 ? a->g.adv.relic_bonus : 0;
    a->shop_sel = SHOP_TEA_THERMOS;
    a->shop_page = 0;
    if (a->g.flawless_award)
        snprintf(a->shop_line, sizeof(a->shop_line), "A whole world without losing a lantern! Take this Continue, you\xe2\x80\x99ve earned it.");
    else if (a->g.depth / SHOP_INTERVAL == 1)
        snprintf(a->shop_line, sizeof(a->shop_line), "Pull up a stool, miner! Just the basics today. I\xe2\x80\x99ll have more in at the next stall.");
    else
        snprintf(a->shop_line, sizeof(a->shop_line), "Back again! New stock in since last time, and a better price for a regular.");
    a->shop_line_t = a->now;
    memset(a->shop_flash, 0, sizeof(a->shop_flash));
    go(a, SC_SHOP);
    save_run(a);
}

static void
process_events(struct app *a)
{
    struct lm_game *g = &a->g;
    unsigned ev = g->events;
    g->events = 0;
    if (g->practice) { /* nothing is recorded */
        if (ev & EV_OPEN_SHOP)
            open_shop(a);
        if (ev & (EV_CONTINUE | EV_GAME_OVER)) {
            a->over_pending = true;
            a->over_timer = 1.1;
        }
        return;
    }
    if (ev & EV_CAVERN_ADVANCED) {
        if (g->adv.total_score > a->st.high_score) a->st.high_score = g->adv.total_score;
        if (g->adv.depth_cleared + 1 > a->st.best_depth) a->st.best_depth = g->adv.depth_cleared + 1;
        a->st.total_relics += g->adv.relics_in_cavern;
        a->st.total_caverns += 1;
        lm_set_records(g, a->st.high_score > g->high_score ? a->st.high_score : g->high_score, a->st.best_depth > g->best_depth ? a->st.best_depth : g->best_depth);
        save_all(a);
    }
    if (ev & EV_OPEN_SHOP)
        open_shop(a);
    if (ev & EV_CONTINUE) {
        a->over_pending = true;
        a->over_timer = 1.1;
        save_run(a);
    }
    if (ev & EV_GAME_OVER) {
        end_run(a);
        if (g->summary.score > a->st.high_score) a->st.high_score = g->summary.score;
        if (g->summary.depth_reached > a->st.best_depth) a->st.best_depth = g->summary.depth_reached;
        save_all(a);
        a->over_pending = true;
        a->over_timer = 1.1;
    }
}

/* ------------------------------------------------------------------ drawing */

static void
draw_cover(struct app *a, struct vkr *r, int which, uint32_t tint)
{
    if (a->img[which] >= 0)
        vkr_image(r, a->img[which], 0, 0, VKR_W, VKR_H, 0, tint);
    else
        vkr_rect_grad(r, 0, 0, VKR_W, VKR_H, HEX(0x16121b), HEX(0x231b24));
}

static void
draw_fade_in(struct app *a, struct vkr *r)
{
    double t = (a->now - a->scr_t0) / 0.30;
    if (t < 1.0)
        vkr_rect(r, 0, 0, VKR_W, VKR_H, HEXA(0x0a080d, 1.0 - t));
}

/* the version tag, bottom-right, on the title screen */
static void
draw_version(struct vkr *r, float alpha)
{
    char b[48];
    snprintf(b, sizeof(b), "v%s \xc2\xb7 Build %02d", LM_VERSION, LM_BUILD);
    ui_text_r(r, VKR_MONO, 24, 1856, 1031, HEXA(C_CREAM, alpha), b);
}

/* loading splash over the first couple of seconds after launch: logo, "Loading", version */
static void
draw_splash(struct app *a, struct vkr *r)
{
    if (a->boot_t0 == 0)
        a->boot_t0 = a->now;
    double age = a->now - a->boot_t0;
    const double hold = 1.8, fade = 0.5;
    if (age >= hold + fade)
        return;
    float al = age < hold ? 1.0f : (float)(1.0 - (age - hold) / fade);
    vkr_rect(r, 0, 0, VKR_W, VKR_H, HEXA(0x0a080d, al));
    ui_text_c(r, VKR_DISPLAY, 96, 960, 400, HEXA(C_CREAM, al), "Lantern Miner");
    int dots = (int)(age * 3.0) % 4;
    char b[48];
    snprintf(b, sizeof(b), "Loading%.*s", dots, "...");
    ui_text_c(r, VKR_SANS, 34, 960, 540, HEXA(C_AMBER, al), b);
    snprintf(b, sizeof(b), "v%s \xc2\xb7 Build %02d", LM_VERSION, LM_BUILD);
    ui_text_c(r, VKR_MONO, 26, 960, 960, HEXA(C_MUTED, al), b);
}

static void
hintbar(struct vkr *r, int n, int const *glyphs, char const *const *labels)
{
    vkr_rect_grad(r, 0, 1000, VKR_W, 80, HEXA(0x08060c, 0.0), HEXA(0x08060c, 0.82));
    float x = 64;
    for (int i = 0; i < n; i++)
        x += ui_hint(r, glyphs[i], x, 1046, labels[i], 38) + 44;
}

/* title-art lantern positions in the 1376x768 source image */
static float const k_title_lanterns[7][2] = {{132, 172}, {989, 150}, {876, 202}, {1241, 168}, {463, 530}, {696, 662}, {619, 617}};

enum { TM_CONTINUE, TM_NEW, TM_HELP, TM_SCORES, TM_SETTINGS, TM_LEVELS, TM_EXIT, TM_COUNT };

/* the entry under the cursor, allowing for the ones that are not shown */
static int
title_entry(struct app const *a)
{
    int e = a->title_sel + (a->has_save ? 0 : 1);
    return !LM_TEST_MENU && e >= TM_LEVELS ? e + 1 : e;
}

/* The title menu depends on whether there is a run to continue. Fills the labels; returns how many. */
static int
title_menu(struct app const *a, char const **labels)
{
    int n = 0;
    if (a->has_save)
        labels[n++] = "Continue";
    labels[n++] = a->confirm_new ? "Lose your save? Press again" : "New Game";
    labels[n++] = "How to Play";
    labels[n++] = "High Scores";
    labels[n++] = "Settings";
    if (LM_TEST_MENU)
        labels[n++] = "Cavern Select (testing)";
    labels[n++] = "Exit to Home";
    return n;
}

static void
draw_title(struct app *a, struct vkr *r, double dt)
{
    double t = a->now;
    int sel = a->title_sel;

    /* slow drift over the title art */
    float z = 1.06f + 0.025f * (float)lm_sin(t * 0.11);
    float sw = 1920.0f / z, sh = 1080.0f / z;
    float cx = 960 + 40 * (float)lm_sin(t * 0.05), cy = 540 + 18 * (float)lm_cos(t * 0.043);
    float x0 = (float)lm_clamp(cx - sw / 2, 0, 1920 - sw), y0 = (float)lm_clamp(cy - sh / 2, 0, 1080 - sh);
    if (a->img[IMG_TITLE] >= 0)
        vkr_image_uv(r, a->img[IMG_TITLE], 0, 0, VKR_W, VKR_H, x0 / 1920, y0 / 1080, (x0 + sw) / 1920, (y0 + sh) / 1080, HEX(0xffffff));
    else
        vkr_rect_grad(r, 0, 0, VKR_W, VKR_H, HEX(0x16121b), HEX(0x231b24));

    /* flickering lanterns and the glowing portal */
    vkr_blend(r, true);
    for (int i = 0; i < 7; i++) {
        float tx = (((k_title_lanterns[i][0] / 1376.0f) - 0.0145f) / 0.971f) * 1961.0f - 20.0f;
        float ty = (((k_title_lanterns[i][1] / 768.0f) - 0.021f) / 0.958f) * 1080.0f;
        float sx = (tx - x0) * z, sy = (ty - y0) * z;
        float fl = 0.8f + 0.2f * (float)lm_sin(t * (7.0 + i) + i * 1.7) + 0.08f * (float)lm_sin(t * 19.0 + i * 3.1);
        vkr_glow(r, sx, sy, 150 * z, HEXA(0xffb347, 0.26 * fl), 0.30f);
    }
    {
        float tx = (((1195.0f / 1376.0f) - 0.0145f) / 0.971f) * 1961.0f - 20.0f;
        float ty = (((385.0f / 768.0f) - 0.021f) / 0.958f) * 1080.0f;
        float p = 0.75f + 0.25f * (float)lm_sin(t * 2.2);
        vkr_glow(r, (tx - x0) * z, (ty - y0) * z, 300 * z, HEXA(0x2ce87d, 0.30 * p), 0.38f);
    }
    for (int i = 0; i < 80; i++) {
        struct dust *d = &a->dust[i];
        d->x += d->vx * (float)dt;
        d->y += d->vy * (float)dt;
        if (d->y < -20) { d->y = 1100; d->x = (float)(lm_rand(&a->g) * 1920); }
        if (d->x < -20) d->x = 1940;
        if (d->x > 1940) d->x = -20;
        float tw = 0.4f + 0.6f * (float)lm_sin(t * 1.9 + d->ph);
        vkr_glow(r, d->x, d->y, d->sz, HEXA(0xffd485, 0.5 * tw), 0.4f);
    }
    vkr_blend(r, false);

    /* darken the left so the menu reads well */
    vkr_rect_gradh(r, 0, 0, 730, 1080, HEXA(0x0e0b12, 0.95), HEXA(0x0e0b12, 0.84));
    vkr_rect_gradh(r, 730, 0, 576, 1080, HEXA(0x0e0b12, 0.84), HEXA(0x0e0b12, 0.34));
    vkr_rect_gradh(r, 1306, 0, 614, 1080, HEXA(0x0e0b12, 0.34), HEXA(0x0e0b12, 0.18));
    vkr_rect_grad(r, 0, 0, VKR_W, 280, HEXA(0x0e0b12, 0.45), HEXA(0x0e0b12, 0.0));
    vkr_rect_grad(r, 0, 800, VKR_W, 280, HEXA(0x0e0b12, 0.0), HEXA(0x0e0b12, 0.82));

    /* logo */
    float bob = (float)lm_sin(t * 1.3) * 4.0f;
    ui_emblem(r, 116, 118 + bob, 2.3f, t);
    ui_text_shadow(r, VKR_DISPLAY, 96, 322, 128 + bob * 0.4f, HEX(0xfff1d6), "Lantern Miner");
    ui_spectrum_rule(r, 326, 262, 740, 7, 1.0f);
    ui_text_spaced(r, VKR_SANS, 27, 326, 286, 7.2f, HEX(0xd4c7b7), "A COSY CAVERN ADVENTURE");

    ui_paragraph(r, VKR_SANS, 27, 120, 372, 800, 40, HEX(0xe6dccb),
                 "Journey through lantern-lit caverns inspired by the 1983 ZX Spectrum classic. Gather flashing keys, watch "
                 "your air, dodge the guardians, and save Relic Points for Old Barnaby\xe2\x80\x99s Shop, which has something new every time you reach it.");

    /* menu */
    char const *labels[TM_COUNT];
    int count = title_menu(a, labels);
    float mx = 120, my = LM_TEST_MENU ? 524 : 548, mw = 560, mh = LM_TEST_MENU ? 52 : 56, gap = 8;
    for (int i = 0; i < count; i++) {
        float y = my + i * (mh + gap);
        ui_button(r, mx, y, mw, mh, labels[i], sel == i, i == 0, a->menu_anim[i], t);
        if (i == 0) {
            float ax = mx + mw - 64 + a->menu_anim[i] * 14;
            if (sel == 0)
                vkr_tri(r, ax, y + mh / 2 - 12, ax, y + mh / 2 + 12, ax + 22, y + mh / 2, HEX(C_INK));
            else
                vkr_tri(r, ax, y + mh / 2 - 12, ax, y + mh / 2 + 12, ax + 22, y + mh / 2, HEXA(C_AMBER, 0.8));
        }
    }

    /* best scores */
    {
        float x = 120, y = my + count * (mh + gap) + 2;
        char b[64];
        if (a->st.best_depth > 0 || a->st.high_score > 0) {
            snprintf(b, sizeof(b), "Best Depth ");
            ui_text(r, VKR_MONO, 24, x, y, HEX(0xc4b8a8), b);
            x += ui_text_w(r, VKR_MONO, 24, b);
            snprintf(b, sizeof(b), "%02d", a->st.best_depth);
            ui_text(r, VKR_MONO_BOLD, 24, x, y, HEX(C_CREAM), b);
            x += ui_text_w(r, VKR_MONO_BOLD, 24, b) + 18;
            ui_text(r, VKR_MONO, 24, x, y, HEXA(C_CREAM, 0.3), "\xc2\xb7");
            x += 30;
            ui_text(r, VKR_MONO, 24, x, y, HEX(0xc4b8a8), "High Score ");
            x += ui_text_w(r, VKR_MONO, 24, "High Score ");
            lm_fmt_int(b, sizeof(b), a->st.high_score);
            ui_text(r, VKR_MONO_BOLD, 24, x, y, HEX(C_AMBER), b);
            x += ui_text_w(r, VKR_MONO_BOLD, 24, b) + 18;
            ui_text(r, VKR_MONO, 24, x, y, HEXA(C_CREAM, 0.3), "\xc2\xb7");
            x += 30;
            ui_text(r, VKR_MONO, 24, x, y, HEX(0xc4b8a8), "Relics ");
            x += ui_text_w(r, VKR_MONO, 24, "Relics ");
            snprintf(b, sizeof(b), "%d", a->st.total_relics);
            ui_text(r, VKR_MONO_BOLD, 24, x, y, HEX(C_YELLOW), b);
        } else {
            ui_text(r, VKR_MONO, 24, x, y, HEX(0xc4b8a8), "Fresh Lantern \xc2\xb7 Ready for Cavern 01");
        }
    }

    {
        static int const gl[2] = {G_CROSS, G_DPAD_UD};
        static char const *const lb[2] = {"Select", "Move"};
        hintbar(r, 2, gl, lb);
        draw_version(r, 0.7f);
    }
}

/* Buy me a coffee, above the build number. Drawn after the scanlines so phones can still read it. */
static void
draw_qr(struct app *a, struct vkr *r)
{
    if (a->img[IMG_QR] >= 0) {
        float const s = 184, qx = 1856 - s, qy = 806;
        vkr_shadow(r, qx - 8, qy - 8, s + 16, s + 16, 14, 24, HEXA(0x000000, 0.6));
        vkr_rrect(r, qx - 8, qy - 8, s + 16, s + 16, 14, HEX(0xffffff));
        vkr_image(r, a->img[IMG_QR], qx, qy, s, s, 6, HEX(0xffffff));
        ui_text_c(r, VKR_SANS_BOLD, 24, qx + s / 2, qy - 46, HEX(C_YELLOW), "Buy me a coffee");
    }
}

static void
draw_hud(struct app *a, struct vkr *r)
{
    struct lm_game const *g = &a->g;
    char b[96];
    double t = g->elapsed;
    struct lm_biome const *bio = &lm_biomes[g->cavern.biome];

    /* top band over the cavern's ceiling course */
    vkr_rect_grad(r, 0, 0, VKR_W, 78, HEXA(0x08060c, 0.82), HEXA(0x08060c, 0.0));
    float lx = 52;
    snprintf(b, sizeof(b), "Cavern %02d", g->depth);
    ui_text(r, VKR_MONO_BOLD, 27, lx, 14, HEX(C_AMBER), b);
    lx += ui_text_w(r, VKR_MONO_BOLD, 27, b) + 16;
    ui_text(r, VKR_MONO, 27, lx, 14, HEXA(C_CREAM, 0.3), "\xc2\xb7");
    lx += 30;
    snprintf(b, sizeof(b), "World %d \xc2\xb7 %s", lm_world_of(g->depth), bio->label);
    ui_text(r, VKR_SANS, 22, lx, 17, HEX(C_DIM), b);
    lx += ui_text_w(r, VKR_SANS, 22, b) + 40;

    float rx = 1868;
    lm_fmt_int(b, sizeof(b), g->score);
    ui_text_r(r, VKR_MONO_BOLD, 34, rx, 8, HEX(C_YELLOW), b);
    rx -= ui_text_w(r, VKR_MONO_BOLD, 34, b) + 12;
    ui_text_r(r, VKR_SANS, 22, rx, 18, HEX(C_DIM), "Score");
    rx -= ui_text_w(r, VKR_SANS, 22, "Score") + 36;

    /* curses from a bad chest, counting down */
    if (g->player.lead > 0) {
        snprintf(b, sizeof(b), "Lead Boots %ds", (int)(g->player.lead + 0.99));
        ui_text_r(r, VKR_MONO_BOLD, 24, rx, 14, HEX(0xb45cff), b);
        rx -= ui_text_w(r, VKR_MONO_BOLD, 24, b) + 28;
    }
    if (g->player.muddle > 0) {
        snprintf(b, sizeof(b), "Muddled %ds", (int)(g->player.muddle + 0.99));
        ui_text_r(r, VKR_MONO_BOLD, 24, rx, 14, HEX(0xb45cff), b);
        rx -= ui_text_w(r, VKR_MONO_BOLD, 24, b) + 28;
    }
    /* Lantern Powers that look after themselves */
    {
        struct lm_player const *p = &g->player;
        for (int i = PU_COUNT - 1; i >= 0; i--) {
            if (i == PU_SPRING_BOOTS || i == PU_STARLIGHT_MAGNET || i == PU_CLOCKWORK_SLOW || g->powers[i] <= 0)
                continue; /* the button powers live along the bottom */
            if (i == PU_LANTERN_SHIELD)
                snprintf(b, sizeof(b), p->has_shield ? "Shield +%d" : "Shield \xe2\x80\x94 +%d", g->powers[i]);
            else
                snprintf(b, sizeof(b), "Tea \xc3\x97%d", g->powers[i]);
            ui_text_r(r, VKR_MONO_BOLD, 24, rx, 14, HEX(lm_powerups[i].color), b);
            rx -= ui_text_w(r, VKR_MONO_BOLD, 24, b) + 28;
        }
        if (p->has_shield && g->powers[PU_LANTERN_SHIELD] <= 0) {
            ui_text_r(r, VKR_MONO_BOLD, 24, rx, 14, HEX(C_AMBER), "Shield Ready");
            rx -= ui_text_w(r, VKR_MONO_BOLD, 24, "Shield Ready") + 28;
        }
    }
    /* keys / portal */
    if (g->portal_unlocked) {
        float pul = 0.7f + 0.3f * (float)lm_sin(t * 6);
        ui_text_r(r, VKR_MONO_BOLD, 26, rx, 13, HEXA(C_GREEN, pul), "Portal Open");
        rx -= ui_text_w(r, VKR_MONO_BOLD, 26, "Portal Open") + 14;
        ui_key_icon(r, rx - 14, 37, 26, true);
        rx -= 62;
    } else {
        snprintf(b, sizeof(b), "%d/%d", g->keys_collected, g->cavern.keys_total);
        ui_text_r(r, VKR_MONO_BOLD, 28, rx, 12, HEX(C_CREAM), b);
        rx -= ui_text_w(r, VKR_MONO_BOLD, 28, b) + 14;
        ui_key_icon(r, rx - 14, 36, 26, false);
        rx -= 62;
    }
    /* hearts */
    for (int i = g->max_hearts - 1; i >= 0; i--) {
        double pulse = (i + 1 == g->hearts && g->hearts == 1) ? 0.5 + 0.5 * lm_sin(t * 8) : 0.0;
        ui_heart(r, rx - 16, 36, 30, i < g->hearts, pulse);
        rx -= 38;
    }
    /* lives */
    snprintf(b, sizeof(b), "Lives x%d", g->lives);
    ui_text_r(r, VKR_MONO_BOLD, 26, rx - 12, 14, g->lives <= 1 ? HEX(C_PINK) : HEX(C_CREAM), b);
    rx -= ui_text_w(r, VKR_MONO_BOLD, 26, b) + 40;
    /* permanent gear, as far as there is room before the right-hand readouts */
    for (int p = 0; p < PERK_COUNT; p++) {
        if (g->perks[p] <= 0)
            continue;
        snprintf(b, sizeof(b), "%s", lm_perks[p].short_label);
        if (lx + ui_text_w(r, VKR_SANS_BOLD, 22, b) > rx)
            break;
        ui_text(r, VKR_SANS_BOLD, 22, lx, 17, HEX(lm_perks[p].accent), b);
        lx += ui_text_w(r, VKR_SANS_BOLD, 22, b) + 22;
    }
    if (g->continues > 0) {
        snprintf(b, sizeof(b), "Continues x%d", g->continues);
        ui_text_r(r, VKR_MONO_BOLD, 22, 1868, 50, HEX(C_GREEN), b);
    }

    /* bottom band: Lantern Air + run meta */
    vkr_rect_grad(r, 0, 1004, VKR_W, 76, HEXA(0x08060c, 0.0), HEXA(0x08060c, 0.85));
    {
        int air = (int)lm_round(g->air);
        bool low = air <= 25;
        float pulse = low ? 0.65f + 0.35f * (float)lm_sin(t * 9) : 1.0f;
        snprintf(b, sizeof(b), low ? "Low Air %d%%" : "Air %d%%", air);
        ui_text(r, VKR_MONO_BOLD, 26, 52, 1031, low ? HEXA(C_PINK, pulse) : HEX(C_AMBER), b);
        float bx = 52 + (low ? 214.0f : 150.0f), bw = 360, by = 1040;
        vkr_rrect(r, bx, by, bw, 18, 5, HEXA(C_PANEL2, 0.95));
        float fillw = bw * (float)lm_clamp(g->air / MAX_AIR, 0, 1);
        if (fillw > 1) {
            if (low)
                vkr_rrect_grad(r, bx, by, fillw, 18, 5, HEX(C_PINK), HEX(0xff7b54));
            else
                vkr_rrect_grad(r, bx, by, fillw, 18, 5, HEX(0xf5b13a), HEX(0xe07b24));
            vkr_rrect(r, bx + 2, by + 2, fillw > 6 ? fillw - 4 : 2, 4, 2, HEXA(0xffffff, 0.22));
        }
        vkr_ring(r, bx, by, bw, 18, 5, 1.5f, HEXA(C_CREAM, 0.14));
    }
    {
        /* points over the shop count, tucked into the corner */
        int until = lm_cavern_until_shop(g);
        if (until == 0) {
            ui_text_r(r, VKR_MONO_BOLD, 24, 1868, 1046, HEX(C_GREEN), "Shop After Portal");
        } else {
            snprintf(b, sizeof(b), "Shop in %d", until);
            ui_text_r(r, VKR_MONO, 24, 1868, 1046, HEX(C_MUTED), b);
        }
        float x = 1868;
        ui_text_r(r, VKR_MONO, 22, x, 1018, HEX(C_MUTED), "Relic Pts");
        x -= ui_text_w(r, VKR_MONO, 22, "Relic Pts") + 10;
        lm_fmt_int(b, sizeof(b), g->relic_points);
        ui_text_r(r, VKR_MONO_BOLD, 26, x, 1014, HEX(C_YELLOW), b);
    }
    float title_end;
    {
        /* the room's name, between the air bar and the power buttons */
        float size = 36, w = ui_text_w(r, VKR_DISPLAY_SEMI, size, g->cavern.name);
        if (w > 500) {
            size *= 500 / w;
            w = ui_text_w(r, VKR_DISPLAY_SEMI, size, g->cavern.name);
        }
        vkr_rrect(r, 920 - w / 2 - 22, 1022, w + 44, 52, 10, HEXA(0x000000, 0.82));
        ui_text_c(r, VKR_DISPLAY_SEMI, size, 920, 1028 + (36 - size) * 0.5f, HEX(C_YELLOW), g->cavern.name);
        title_end = 920 + w / 2 + 22;
    }
    {
        /* each power held, with the button that starts it; once running, the seconds left */
        static int const held[3] = {PU_SPRING_BOOTS, PU_STARLIGHT_MAGNET, PU_CLOCKWORK_SLOW};
        static int const button[3] = {G_SQUARE, G_TRIANGLE, G_CIRCLE};
        float x = title_end + 18;
        for (int i = 0; i < 3; i++) {
            int pu = held[i];
            char const *name = lm_powerups[pu].short_label;
            bool running = g->power_time[pu] > 0;
            if (!running && g->powers[pu] <= 0)
                continue;
            float w;
            if (running) {
                snprintf(b, sizeof(b), "%s %ds", name, (int)(g->power_time[pu] + 0.99));
                w = ui_text_w(r, VKR_MONO_BOLD, 24, b);
            } else {
                w = ui_hint_w(r, button[i], name, 30);
            }
            vkr_rrect(r, x, 1022, w + 24, 52, 10, HEXA(0x000000, 0.82));
            if (running)
                ui_text(r, VKR_MONO_BOLD, 24, x + 12, 1033, HEX(lm_powerups[pu].color), b);
            else
                ui_hint(r, button[i], x + 12, 1048, name, 30);
            x += w + 34;
        }
    }

    /* first seconds: how to play */
    {
        double age = a->now - a->run_t0;
        if (age < 9.0 && g->depth == 1) {
            float al = (float)lm_clamp(age < 0.8 ? age / 0.8 : (9.0 - age) / 1.2, 0, 1);
            float gw = ui_hint_w(r, G_DPAD_LR, "Move", 40) + ui_hint_w(r, G_CROSS, "Jump (hold = higher)", 40) + ui_hint_w(r, G_DPAD_UD, "Climb", 40) +
                       ui_hint_w(r, G_OPTIONS, "Pause", 40) + 150;
            float x = (VKR_W - gw) / 2, y = 150; /* up by the top bar, clear of Willy */
            vkr_rrect(r, x - 36, y - 38, gw + 72, 76, 20, HEXA(0x0a080d, 0.78 * al));
            if (al > 0.45f) { /* the glyphs have no alpha of their own: they leave as the pill fades */
                x += ui_hint(r, G_DPAD_LR, x, y, "Move", 40) + 50;
                x += ui_hint(r, G_CROSS, x, y, "Jump (hold = higher)", 40) + 50;
                x += ui_hint(r, G_DPAD_UD, x, y, "Climb", 40) + 50;
                ui_hint(r, G_OPTIONS, x, y, "Pause", 40);
            }
        }
    }
    if (a->toast_t > 0)
        ui_text_c(r, VKR_SANS_BOLD, 30, 960, 120, HEXA(C_CREAM, lm_min(1.0, a->toast_t)), a->toast);
}

static void
draw_dim(struct vkr *r, float alpha)
{
    vkr_rect(r, 0, 0, VKR_W, VKR_H, HEXA(0x0a080d, alpha));
    vkr_vignette(r, 0, 0, VKR_W, VKR_H, 960, 540, 380, 1100, HEX(0x000000), 0.5f);
}

static void
draw_pause(struct app *a, struct vkr *r)
{
    struct lm_game const *g = &a->g;
    char b[96];
    draw_dim(r, 0.66f);
    float pw = 760, ph = 762, px = (VKR_W - pw) / 2, py = 150;
    ui_panel(r, px, py, pw, ph, 28, 0.94f);
    snprintf(b, sizeof(b), "Cavern %02d \xc2\xb7 %s", g->depth, g->cavern.name);
    ui_text_c(r, VKR_MONO, 26, 960, py + 40, HEX(C_AMBER), b);
    ui_text_c(r, VKR_DISPLAY, 76, 960, py + 80, HEX(C_CREAM), "Lantern Paused");
    ui_spectrum_rule(r, px + 160, py + 184, pw - 320, 5, 0.9f);
    ui_text_c(r, VKR_MONO, 24, 960, py + 206, HEX(C_MUTED),
              g->practice ? "Test run: nothing is saved" : "Your progress is saved after every cavern");

    char const *labels[5] = {"Resume Expedition", "Settings", "Sound: On", "How to Play", g->practice ? "Back to Cavern Select" : "Save & Quit to Title"};
    for (int i = 0; i < 5; i++) {
        char lab[48];
        snprintf(lab, sizeof(lab), "%s", i == 2 ? (a->set.muted ? "Sound: Off" : "Sound: On") : labels[i]);
        ui_button(r, px + 70, py + 270 + i * 88, pw - 140, 72, lab, a->pause_sel == i, i == 0, a->row_anim[i], a->now);
    }
    static int const gl[4] = {G_DPAD_UD, G_CROSS, G_CIRCLE, G_R3};
    static char const *const lb[4] = {"Move", "Select", "Resume", "Mute"};
    hintbar(r, 4, gl, lb);
}

/* Shop layout, rows of three: page 0 = Lantern Powers and lives (7 items), page 1 = supplies and gear (9). */
static void
shop_pos(int item, int *page, int *row, int *col)
{
    if (item >= SHOP_SPRING_BOOTS) {
        int k = item - SHOP_SPRING_BOOTS;
        *page = 0; *row = k / 3; *col = k % 3;
    } else {
        *page = 1; *row = item / 3; *col = item % 3;
    }
}

static int
shop_rows(int page)
{
    (void)page;
    return 3;
}

static int
shop_rowlen(int page, int row)
{
    return page == 0 && row == 2 ? SHOP_COUNT - SHOP_SPRING_BOOTS - 6 : 3;
}

/* item at a grid position; the column is clamped to the length of that row */
static int
shop_item_at(int page, int row, int col)
{
    int len = shop_rowlen(page, row);
    if (col >= len)
        col = len - 1;
    return (page == 0 ? SHOP_SPRING_BOOTS : 0) + row * 3 + col;
}

static void
draw_shop(struct app *a, struct vkr *r)
{
    struct lm_game const *g = &a->g;
    char b[160], c2[64];
    double t = a->now;

    draw_cover(a, r, IMG_CAMP, HEX(0xb4b4c0));
    vkr_rect(r, 0, 0, VKR_W, VKR_H, HEXA(0x0a080d, 0.5));
    vkr_blend(r, true);
    vkr_glow(r, 590, 880, 520, HEXA(0xf5a623, 0.10 + 0.02 * lm_sin(t * 6)), 0.35f);
    vkr_blend(r, false);
    vkr_vignette(r, 0, 0, VKR_W, VKR_H, 960, 540, 520, 1250, HEX(0x000000), 0.6f);

    /* header */
    vkr_rect_grad(r, 0, 0, VKR_W, 122, HEXA(0x07050a, 0.85), HEXA(0x07050a, 0.0));
    snprintf(b, sizeof(b), "Cavern %02d Cleared", g->adv.depth_cleared);
    if (a->shop_air_bonus > 0) {
        lm_fmt_int(c2, sizeof(c2), a->shop_air_bonus);
        snprintf(b + strlen(b), sizeof(b) - strlen(b), " \xc2\xb7 Air Bonus +%s", c2);
    }
    if (a->shop_relic_bonus > 0)
        snprintf(b + strlen(b), sizeof(b) - strlen(b), " \xc2\xb7 Relic Points +%d", a->shop_relic_bonus);
    ui_text(r, VKR_MONO, 26, 80, 20, HEX(C_GREEN), b);
    ui_text(r, VKR_DISPLAY, 58, 80, 46, HEX(C_CREAM), "Old Barnaby\xe2\x80\x99s Subterranean Stall");
    lm_fmt_int(c2, sizeof(c2), g->relic_points);
    snprintf(b, sizeof(b), "%s pts", c2);
    ui_text_r(r, VKR_MONO, 22, 1840, 22, HEX(C_DIM), "Relic Points");
    ui_text_r(r, VKR_MONO_BOLD, 42, 1840, 46, HEX(C_YELLOW), b);
    if (lm_shop_discount(g) > 0) {
        snprintf(b, sizeof(b), "Regulars' discount: %d%% off supplies and gear", lm_shop_discount(g));
        ui_text_r(r, VKR_SANS_BOLD, 22, 1840, 98, HEX(C_GREEN), b);
    }
    for (int i = g->max_hearts - 1; i >= 0; i--)
        ui_heart(r, 1500 - (g->max_hearts - 1 - i) * 40, 60, 30, i < g->hearts, 0);
    snprintf(b, sizeof(b), "Lives x%d \xc2\xb7 Continues x%d", g->lives, g->continues);
    ui_text_r(r, VKR_MONO_BOLD, 24, 1516, 20, HEX(C_CREAM), b);

    /* left: portrait and dialogue */
    float px = 80, py = 150, pw = 640, ph = 478;
    vkr_shadow(r, px, py + 10, pw, ph, 18, 34, HEXA(0x000000, 0.7));
    if (a->img[IMG_SHOP] >= 0)
        vkr_image(r, a->img[IMG_SHOP], px, py, pw, ph, 18, HEX(0xffffff));
    else
        vkr_rrect(r, px, py, pw, ph, 18, HEX(C_PANEL));
    vkr_ring(r, px, py, pw, ph, 18, 2.0f, HEXA(C_AMBER, 0.45));
    ui_text(r, VKR_DISPLAY_SEMI, 34, px, py + ph + 24, HEX(C_AMBER), "Old Barnaby says:");
    {
        int chars = (int)((t - a->shop_line_t) * 70.0);
        char part[240];
        int n = 0, shown = 0;
        char const *s = a->shop_line;
        part[0] = '\xe2'; part[1] = '\x80'; part[2] = '\x9c';
        n = 3;
        while (*s && shown < chars && n < 230) {
            part[n++] = *s++;
            while (((unsigned char)*s & 0xC0) == 0x80 && n < 230)
                part[n++] = *s++;
            shown++;
        }
        if (!*s) {
            part[n++] = '\xe2'; part[n++] = '\x80'; part[n++] = '\x9d';
        }
        part[n] = 0;
        ui_paragraph(r, VKR_SANS, 27, px, py + ph + 72, pw, 38, HEX(0xe6dccb), part);
    }
    snprintf(b, sizeof(b), "Unspent Relic Points stay in your satchel for the next stall.");
    ui_paragraph(r, VKR_SANS, 21, px, 886, pw, 28, HEX(C_DIM), b);

    /* right: shelves (page 0 = Lantern Powers, page 1 = supplies and gear) */
    static char const *const heads[3] = {"Everyday Supplies", "Cavern Gear", "Masterwork Relics"};
    static char const *const notes[3] = {"Affordable remedies and light gear", "Sturdy upgrades for faster conveyors", "Rare heirlooms worth saving for"};
    float gx = 780, gw = 1060, cwid = (gw - 2 * 22) / 3, chh = 214;
    if (a->shop_page == 0) {
        ui_text(r, VKR_DISPLAY_SEMI, 34, gx, 128, HEX(C_CREAM), "Lantern Powers & Lives");
        ui_text_r(r, VKR_SANS, 21, gx + gw, 140, HEX(C_DIM), "Carry one of each, and start it in a cavern when you like");
    } else {
        for (int tier = 0; tier < 3; tier++) {
            float sy = 128 + tier * 272;
            ui_text(r, VKR_DISPLAY_SEMI, 34, gx, sy, HEX(C_CREAM), heads[tier]);
            ui_text_r(r, VKR_SANS, 21, gx + gw, sy + 12, HEX(C_DIM), notes[tier]);
        }
    }
    for (int i = 0; i < SHOP_COUNT; i++) {
        int page, row, col;
        shop_pos(i, &page, &row, &col);
        if (page != a->shop_page)
            continue;
        struct lm_shop_status st;
        lm_shop_item_status(g, i, &st);
        float x = gx + col * (cwid + 22), y = page == 0 ? 174 + row * (chh + 26) : 128 + row * 272 + 46;
        bool sel = a->shop_sel == i;
        float lift = sel ? -6.0f : 0.0f;
        float fl = (float)lm_max(0, a->shop_flash[i]);
        y += lift;
        if (sel)
            vkr_shadow(r, x, y + 8, cwid, chh, 16, 30, HEXA(C_AMBER, 0.30));
        vkr_rrect(r, x, y, cwid, chh, 16, st.can_buy || sel ? HEXA(C_PANEL2, 0.95) : HEXA(0x191520, 0.93));
        if (fl > 0)
            vkr_rrect(r, x, y, cwid, chh, 16, HEXA(C_GREEN, fl * 0.35));
        vkr_ring(r, x, y, cwid, chh, 16, sel ? 3.0f : 1.5f, sel ? HEX(C_AMBER) : HEXA(C_CREAM, st.can_buy ? 0.18 : 0.08));
        vkr_rrect(r, x + 16, y + 16, 52, 52, 12, HEX(0x14111a));
        vkr_ring(r, x + 16, y + 16, 52, 52, 12, 1.5f, HEXA(C_CREAM, 0.1));
        ui_item_icon(r, i, x + 42, y + 42, 30, lm_shop[i].accent);
        ui_text_r(r, VKR_MONO, 20, x + cwid - 18, y + 24, HEX(C_DIM), st.owned);
        float a2 = st.can_buy || st.is_owned ? 1.0f : 0.78f;
        ui_text(r, VKR_DISPLAY_SEMI, 29, x + 18, y + 74, HEXA(C_CREAM, a2), lm_shop[i].name);
        ui_paragraph(r, VKR_SANS, 19, x + 18, y + 112, cwid - 34, 24, HEXA(0xb6aa9c, a2), lm_shop[i].description);
        uint32_t sc = st.can_buy ? C_YELLOW : st.is_owned ? C_GREEN : C_DIM;
        vkr_rect(r, x + 18, y + chh - 40, cwid - 36, 1.5f, HEXA(C_CREAM, 0.08));
        ui_text(r, VKR_MONO_BOLD, 21, x + 18, y + chh - 32, HEX(sc), st.status);
    }
    /* shelf tabs and descend */
    {
        char lab[64];
        float bw = 520, bh = 66, bx = gx + gw - bw, by = 944;
        static char const *const tabs[2] = {"Powers", "Supplies"};
        float tw = 256;
        for (int k = 0; k < 2; k++) {
            float tx = gx + k * (tw + 8);
            bool on = a->shop_page == k;
            vkr_rrect(r, tx, by, tw, bh, 14, on ? HEXA(C_AMBER, 0.95) : HEXA(C_PANEL2, 0.90));
            vkr_ring(r, tx, by, tw, bh, 14, 1.5f, on ? HEX(C_AMBER) : HEXA(C_CREAM, 0.14));
            float w = ui_text_w(r, VKR_SANS_BOLD, 24, tabs[k]);
            ui_text(r, VKR_SANS_BOLD, 24, tx + (tw - w) * 0.5f, by + 18, on ? HEX(C_INK) : HEX(C_CREAM), tabs[k]);
        }
        snprintf(lab, sizeof(lab), "Descend to Cavern %02d", g->depth + 1);
        ui_text(r, VKR_MONO, 21, px, 970, HEX(C_DIM), "Lantern Air refills to 100% on descent");
        ui_button(r, bx, by, bw, bh, lab, a->shop_sel == SHOP_COUNT, true, a->shop_sel == SHOP_COUNT ? 1.0f : 0.0f, t);
        float ax = bx + bw - 60 + (a->shop_sel == SHOP_COUNT ? 14 : 0);
        vkr_tri(r, ax, by + bh / 2 - 12, ax, by + bh / 2 + 12, ax + 22, by + bh / 2, a->shop_sel == SHOP_COUNT ? HEX(C_INK) : HEX(C_AMBER));
    }
    {
        static int const gl[5] = {G_DPAD, G_CROSS, G_L1, G_OPTIONS, G_R3};
        static char const *const lb[5] = {"Browse", "Buy / Select", "Switch Shelf", "Descend", "Mute"};
        hintbar(r, 5, gl, lb);
    }
    (void)c2;
}

static void
draw_over(struct app *a, struct vkr *r)
{
    struct lm_run_summary const *s = &a->g.summary;
    char b[96];
    double t = a->now;
    draw_cover(a, r, IMG_CAMP, HEX(0xdcdce6));
    vkr_rect(r, 0, 0, VKR_W, VKR_H, HEXA(0x0a080d, 0.48));
    vkr_vignette(r, 0, 0, VKR_W, VKR_H, 960, 540, 420, 1150, HEX(0x000000), 0.6f);

    float pw = 1120, ph = 700, px = (VKR_W - pw) / 2, py = 150;
    ui_panel(r, px, py, pw, ph, 30, 0.94f);
    ui_text_c(r, VKR_MONO, 26, 960, py + 40, HEX(s->won ? C_GREEN : C_PINK),
              s->won ? "Expedition Complete" : s->air ? "Lantern Flame Faded" : "Time for Tea & Rest");
    ui_text_c(r, VKR_DISPLAY, 88, 960, py + 74, HEX(C_CREAM), s->won ? "To Be Continued" : "Game Over");
    ui_text_c(r, VKR_SANS, 27, 960, py + 188, HEX(C_MUTED),
              s->won ? "You have cleared every cavern built so far. More worlds are on the way."
                     : s->new_high_score ? "New personal best score etched into the mine log." : "Dust yourself off, miner. The caverns are waiting.");

    vkr_rect(r, px + 60, py + 250, pw - 120, 1.5f, HEXA(C_CREAM, 0.1));
    vkr_rect(r, px + 60, py + 420, pw - 120, 1.5f, HEXA(C_CREAM, 0.1));
    static char const *const lab[4] = {"Final Score", "Cavern Depth", "Keys Found", "Relics"};
    static uint32_t const colr[4] = {C_YELLOW, C_CREAM, C_PINK, C_GREEN};
    for (int i = 0; i < 4; i++) {
        float cx = px + 60 + (pw - 120) * (i + 0.5f) / 4.0f;
        switch (i) {
        case 0: lm_fmt_int(b, sizeof(b), s->score); break;
        case 1: snprintf(b, sizeof(b), "%02d", s->depth_reached); break;
        case 2: snprintf(b, sizeof(b), "%d", s->keys); break;
        default: snprintf(b, sizeof(b), "%d", s->relics); break;
        }
        ui_text_c(r, VKR_MONO, 22, cx, py + 276, HEX(C_DIM), lab[i]);
        ui_text_c(r, VKR_MONO_BOLD, 56, cx, py + 316, HEX(colr[i]), b);
    }
    {
        char hs[32];
        lm_fmt_int(hs, sizeof(hs), s->high_score);
        snprintf(b, sizeof(b), "Best Score %s", hs);
        ui_text_c(r, VKR_MONO, 24, 960, py + 446, HEX(C_MUTED), b);
        if (s->new_best_depth)
            ui_text_c(r, VKR_MONO_BOLD, 24, 960, py + 482, HEXA(C_GREEN, 0.8 + 0.2 * lm_sin(t * 5)), "New deepest cavern!");
    }
    static char const *const bl[2] = {"New Game", "Title Screen"};
    float bw = 360, bh = 76, gap = 40, bx0 = px + (pw - (2 * bw + gap)) / 2;
    for (int i = 0; i < 2; i++)
        ui_button_c(r, bx0 + i * (bw + gap), py + 560, bw, bh, bl[i], a->over_sel == i, i == 0, a->row_anim[i] * 0.5f, t);

    static int const gl[3] = {G_DPAD_LR, G_CROSS, G_CIRCLE};
    static char const *const lb[3] = {"Choose", "Confirm", "Title"};
    hintbar(r, 3, gl, lb);
}

static void
draw_continue(struct app *a, struct vkr *r)
{
    struct lm_game const *g = &a->g;
    char b[128];
    double t = a->now;
    draw_cover(a, r, IMG_CAMP, HEX(0x9a9aa6));
    vkr_rect(r, 0, 0, VKR_W, VKR_H, HEXA(0x0a080d, 0.62));
    float pw = 1000, ph = 560, px = (VKR_W - pw) / 2, py = 230;
    ui_panel(r, px, py, pw, ph, 30, 0.94f);
    ui_text_c(r, VKR_MONO, 26, 960, py + 40, HEX(C_PINK), "Out of Lives");
    ui_text_c(r, VKR_DISPLAY, 80, 960, py + 80, HEX(C_CREAM), "Carry On?");
    snprintf(b, sizeof(b), "A Continue takes you back to the start of World %d with %d lives.", lm_world_of(g->depth), START_LIVES);
    ui_text_c(r, VKR_SANS, 28, 960, py + 200, HEX(C_MUTED), b);
    ui_text_c(r, VKR_SANS, 24, 960, py + 246, HEX(C_DIM), "You keep your upgrades and Relic Points. Score returns to what it was when you arrived.");
    snprintf(b, sizeof(b), "Continues held: %d", g->continues);
    ui_text_c(r, VKR_MONO_BOLD, 30, 960, py + 310, HEX(C_GREEN), b);
    static char const *const bl[2] = {"Use a Continue", "Give Up"};
    float bw = 360, bh = 76, gap = 40, bx0 = px + (pw - (2 * bw + gap)) / 2;
    for (int i = 0; i < 2; i++)
        ui_button_c(r, bx0 + i * (bw + gap), py + 410, bw, bh, bl[i], a->over_sel == i, i == 0, a->row_anim[i] * 0.5f, t);
    static int const gl[2] = {G_DPAD_LR, G_CROSS};
    static char const *const lb[2] = {"Choose", "Confirm"};
    hintbar(r, 2, gl, lb);
}

enum { SET_MUSIC, SET_SFX, SET_MUTE, SET_SCAN, SET_BLOOM, SET_RUMBLE, SET_SHAKE, SET_BACK, SET_COUNT };

static void
draw_settings(struct app *a, struct vkr *r)
{
    char b[48];
    double t = a->now;
    if (a->prev_screen == SC_PAUSE || a->prev_screen == SC_PLAY) {
        draw_dim(r, 0.8f);
    } else {
        draw_cover(a, r, IMG_TITLE, HEX(0x707078));
        vkr_rect(r, 0, 0, VKR_W, VKR_H, HEXA(0x0a080d, 0.7));
    }
    float pw = 1000, ph = 820, px = (VKR_W - pw) / 2, py = 120;
    ui_panel(r, px, py, pw, ph, 28, 0.95f);
    ui_text_c(r, VKR_DISPLAY, 66, 960, py + 34, HEX(C_CREAM), "Settings");
    ui_spectrum_rule(r, px + 340, py + 124, pw - 680, 5, 0.9f);

    static char const *const names[SET_COUNT] = {"Music Volume", "Effects Volume", "Sound", "Scanlines", "Lantern Bloom", "Controller Rumble", "Screen Shake", "Back"};
    for (int i = 0; i < SET_COUNT; i++) {
        float y = py + 160 + i * 76, x = px + 50, w = pw - 100, h = 64;
        bool sel = a->set_sel == i;
        float an = a->row_anim[i];
        if (sel) {
            vkr_rrect_grad(r, x, y, w, h, 14, HEXA(C_AMBER, 0.20), HEXA(C_AMBER, 0.10));
            vkr_ring(r, x, y, w, h, 14, 2.0f, HEX(C_AMBER));
        }
        (void)an;
        ui_text(r, VKR_SANS_BOLD, 31, x + 26, y + 14, sel ? HEX(C_CREAM) : HEX(0xd4c7b7), names[i]);
        float vx = x + w - 28;
        switch (i) {
        case SET_MUSIC:
        case SET_SFX: {
            int v = i == SET_MUSIC ? a->set.music : a->set.sfx;
            for (int k = 9; k >= 0; k--) {
                float bx = vx - (9 - k) * 22 - 14;
                vkr_rrect(r, bx, y + 18, 14, 28, 4, k < v ? HEX(C_AMBER) : HEXA(C_CREAM, 0.16));
            }
            snprintf(b, sizeof(b), "%d", v);
            ui_text_r(r, VKR_MONO_BOLD, 28, vx - 240, y + 16, HEX(C_CREAM), b);
            break;
        }
        case SET_MUTE:
            ui_text_r(r, VKR_MONO_BOLD, 28, vx, y + 16, a->set.muted ? HEX(C_PINK) : HEX(C_GREEN), a->set.muted ? "Muted" : "On");
            break;
        case SET_SCAN: {
            static char const *const sn[3] = {"Off", "Subtle", "Strong"};
            ui_text_r(r, VKR_MONO_BOLD, 28, vx, y + 16, HEX(C_CREAM), sn[a->set.scan]);
            break;
        }
        case SET_BLOOM:
            ui_text_r(r, VKR_MONO_BOLD, 28, vx, y + 16, a->set.bloom ? HEX(C_GREEN) : HEX(C_DIM), a->set.bloom ? "On" : "Off");
            break;
        case SET_RUMBLE:
            ui_text_r(r, VKR_MONO_BOLD, 28, vx, y + 16, a->set.rumble ? HEX(C_GREEN) : HEX(C_DIM), a->set.rumble ? "On" : "Off");
            break;
        case SET_SHAKE:
            ui_text_r(r, VKR_MONO_BOLD, 28, vx, y + 16, a->set.shake ? HEX(C_GREEN) : HEX(C_DIM), a->set.shake ? "On" : "Off");
            break;
        default:
            break;
        }
    }
    static int const gl[3] = {G_DPAD_UD, G_DPAD_LR, G_CIRCLE};
    static char const *const lb[3] = {"Move", "Adjust", "Back"};
    hintbar(r, 3, gl, lb);
}

/* ------------------------------------------------------------------ frame */

static void
toggle_mute(struct app *a)
{
    a->set.muted = !a->set.muted;
    apply_settings(a);
    a->dirty = true;
    toast(a, a->set.muted ? "Sound muted" : "Sound on");
}

static void
adjust_setting(struct app *a, int row, int dir)
{
    switch (row) {
    case SET_MUSIC: a->set.music = (int)lm_clamp(a->set.music + dir, 0, 10); break;
    case SET_SFX: a->set.sfx = (int)lm_clamp(a->set.sfx + dir, 0, 10); break;
    case SET_MUTE: a->set.muted = !a->set.muted; break;
    case SET_SCAN: a->set.scan = (a->set.scan + 3 + dir) % 3; break;
    case SET_BLOOM: a->set.bloom = !a->set.bloom; break;
    case SET_RUMBLE:
        a->set.rumble = !a->set.rumble;
        a->buzz_t = a->set.rumble ? 0.35 : 0;
        break;
    case SET_SHAKE: a->set.shake = !a->set.shake; break;
    default: return;
    }
    apply_settings(a);
    a->dirty = true;
    lm_audio_ui_move();
}

#include "lm_scores.inc"
#include "lm_help.inc"

static void
update_title(struct app *a, struct app_input const *in)
{
    uint32_t p = in->pressed;
    char const *labels[TM_COUNT];
    int count = title_menu(a, labels);
    if (a->title_sel >= count)
        a->title_sel = 0;
    if (nav(a, in, &a->title_sel, count) || (p & PAD_CIRCLE))
        a->confirm_new = false;
    if (!(p & PAD_CROSS))
        return;
    switch (title_entry(a)) {
    case TM_CONTINUE:
        lm_audio_ui_select();
        resume_run(a);
        break;
    case TM_NEW:
        lm_audio_ui_select();
        if (a->has_save && !a->confirm_new) {
            a->confirm_new = true; /* a second press, so a save is not lost by accident */
        } else {
            a->confirm_new = false;
            save_all(a);
            start_run(a);
        }
        break;
    case TM_HELP:
        lm_audio_ui_select();
        open_help(a);
        break;
    case TM_SCORES:
        lm_audio_ui_select();
        a->hs_new = -1;
        a->hs_from_over = false;
        go(a, SC_SCORES);
        break;
    case TM_SETTINGS:
        lm_audio_ui_select();
        go(a, SC_SETTINGS);
        break;
    case TM_LEVELS:
        lm_audio_ui_select();
        go(a, SC_LEVELS);
        break;
    default:
        save_all(a);
        a->exit_req = true;
        break;
    }
}

static void
to_title(struct app *a)
{
    a->g.running = false;
    a->title_sel = 0;
    a->confirm_new = false;
    lm_audio_set_music_mode(MUSIC_TITLE, 1.0);
    go(a, SC_TITLE);
}

static void
update_pause(struct app *a, struct app_input const *in)
{
    uint32_t p = in->pressed;
    nav(a, in, &a->pause_sel, 5);
    if (p & PAD_R3)
        toggle_mute(a);
    if ((p & (PAD_OPTIONS | PAD_CIRCLE)) || ((p & PAD_CROSS) && a->pause_sel == 0)) {
        lm_audio_ui_back();
        lm_set_paused(&a->g, false);
        a->jump_lock = true;
        go(a, SC_PLAY);
        return;
    }
    if (p & PAD_CROSS) {
        lm_audio_ui_select();
        switch (a->pause_sel) {
        case 1:
            go(a, SC_SETTINGS);
            break;
        case 2:
            toggle_mute(a);
            break;
        case 3:
            open_help(a);
            break;
        case 4:
            lm_set_paused(&a->g, true);
            save_run(a);
            if (a->g.practice)
                leave_practice(a);
            else
                to_title(a);
            break;
        default:
            break;
        }
    }
}

static void
update_shop(struct app *a, struct app_input const *in)
{
    uint32_t p = in->pressed;
    /* grid navigation: two shelves (pages) of items, then the descend button (SHOP_COUNT) */
    int sel = a->shop_sel;
    int page = a->shop_page;
    if (p & (PAD_L1 | PAD_R1)) {
        int pg, row = 0, col = 0;
        if (sel < SHOP_COUNT)
            shop_pos(sel, &pg, &row, &col);
        a->shop_page = page = !page;
        if (row >= shop_rows(page))
            row = shop_rows(page) - 1;
        sel = shop_item_at(page, row, col);
        lm_audio_ui_move();
    } else if (sel < SHOP_COUNT) {
        int pg, row, col;
        shop_pos(sel, &pg, &row, &col);
        int rows = shop_rows(page), len = shop_rowlen(page, row);
        if (p & PAD_LEFT) { col = (col + len - 1) % len; sel = shop_item_at(page, row, col); lm_audio_ui_move(); }
        if (p & PAD_RIGHT) { col = (col + 1) % len; sel = shop_item_at(page, row, col); lm_audio_ui_move(); }
        if (p & PAD_UP) { sel = row == 0 ? SHOP_COUNT : shop_item_at(page, row - 1, col); lm_audio_ui_move(); }
        if (p & PAD_DOWN) {
            if (row == rows - 1) sel = SHOP_COUNT; else sel = shop_item_at(page, row + 1, col);
            lm_audio_ui_move();
        }
    } else {
        if (p & PAD_UP) { sel = shop_item_at(page, shop_rows(page) - 1, 0); lm_audio_ui_move(); }
        if (p & PAD_DOWN) { sel = shop_item_at(page, 0, 0); lm_audio_ui_move(); }
        if (p & (PAD_LEFT | PAD_RIGHT)) { sel = shop_item_at(page, shop_rows(page) - 1, 1); lm_audio_ui_move(); }
    }
    a->shop_sel = sel;
    if (p & PAD_R3)
        toggle_mute(a);
    if (p & PAD_OPTIONS)
        sel = SHOP_COUNT, p |= PAD_CROSS, a->shop_sel = SHOP_COUNT;
    if (p & PAD_CROSS) {
        if (a->shop_sel == SHOP_COUNT) {
            lm_audio_ui_select();
            lm_leave_shop(&a->g);
            a->jump_lock = true;
            a->run_t0 = a->now - 100;
            go(a, SC_PLAY);
            save_run(a);
            return;
        }
        struct lm_shop_status st;
        lm_shop_item_status(&a->g, a->shop_sel, &st);
        if (st.can_buy && lm_shop_purchase(&a->g, a->shop_sel)) {
            struct lm_shop_def const *it = &lm_shop[a->shop_sel];
            a->shop_flash[a->shop_sel] = 1.0;
            if (a->shop_sel == SHOP_CONTINUE)
                snprintf(a->shop_line, sizeof(a->shop_line), "A Continue Lantern. Costly, I know. Pray you never need it!");
            else if (a->shop_sel == SHOP_EXTRA_LIFE)
                snprintf(a->shop_line, sizeof(a->shop_line), "A spare lantern! That\xe2\x80\x99s %d lives now. The next one will cost you more.", a->g.lives);
            else if (a->shop_sel == SHOP_SPRING_BOOTS || a->shop_sel == SHOP_STARLIGHT_MAGNET || a->shop_sel == SHOP_CLOCKWORK_SLOW)
                snprintf(a->shop_line, sizeof(a->shop_line), "The %s is yours. Press %s down there when you want it. It only works the once!",
                         it->name, a->shop_sel == SHOP_SPRING_BOOTS ? "Square" : a->shop_sel == SHOP_STARLIGHT_MAGNET ? "Triangle" : "Circle");
            else if (it->tier == TIER_MASTERWORK)
                snprintf(a->shop_line, sizeof(a->shop_line), "By my beard\xe2\x80\x94the %s! You saved well for that heirloom. It\xe2\x80\x99ll serve you true in the deep dark.", it->name);
            else if (it->tier == TIER_GEAR)
                snprintf(a->shop_line, sizeof(a->shop_line), "Fine pick with the %s. Those deeper guardians won\xe2\x80\x99t know what hit \xe2\x80\x99" "em!", it->name);
            else
                snprintf(a->shop_line, sizeof(a->shop_line), "Here\xe2\x80\x99s your %s, fresh off the counter. Keep your lantern trimmed down below!", it->name);
            a->shop_line_t = a->now;
            save_all(a);
            save_run(a);
        } else {
            lm_audio_ui_back();
        }
    }
}

#include "lm_levelsel.inc"

static void
update_over(struct app *a, struct app_input const *in)
{
    uint32_t p = in->pressed;
    if (p & (PAD_LEFT | PAD_RIGHT)) { a->over_sel = !a->over_sel; lm_audio_ui_move(); }
    if (p & PAD_CIRCLE) {
        lm_audio_ui_back();
        to_title(a);
        return;
    }
    if (p & PAD_CROSS) {
        lm_audio_ui_select();
        if (a->over_sel == 0)
            start_run(a);
        else
            to_title(a);
    }
}

static void
update_continue(struct app *a, struct app_input const *in)
{
    uint32_t p = in->pressed;
    if (a->now - a->scr_t0 < 0.7) /* mashing on the way down must not spend a Continue */
        return;
    if (p & (PAD_LEFT | PAD_RIGHT)) { a->over_sel = !a->over_sel; lm_audio_ui_move(); }
    if (!(p & PAD_CROSS))
        return;
    lm_audio_ui_select();
    if (a->over_sel == 0) {
        lm_use_continue(&a->g);
        a->jump_lock = true;
        a->run_t0 = a->now - 100;
        go(a, SC_PLAY);
        save_run(a);
    } else if (a->g.practice) {
        leave_practice(a);
    } else {
        lm_give_up(&a->g);
        end_run(a);
        if (a->g.summary.score > a->st.high_score) a->st.high_score = a->g.summary.score;
        if (a->g.summary.depth_reached > a->st.best_depth) a->st.best_depth = a->g.summary.depth_reached;
        save_all(a);
        a->g.events = 0;
        a->over_sel = 0;
        finish_run(a);
    }
}

static void
update_settings(struct app *a, struct app_input const *in)
{
    uint32_t p = in->pressed;
    nav(a, in, &a->set_sel, SET_COUNT);
    if (p & PAD_LEFT) adjust_setting(a, a->set_sel, -1);
    if (p & PAD_RIGHT) adjust_setting(a, a->set_sel, +1);
    if (p & PAD_CROSS) {
        if (a->set_sel == SET_BACK) p |= PAD_CIRCLE;
        else if (a->set_sel >= SET_MUTE) adjust_setting(a, a->set_sel, +1);
    }
    if (p & PAD_CIRCLE) {
        lm_audio_ui_back();
        save_all(a);
        go(a, a->prev_screen == SC_SETTINGS ? SC_TITLE : a->prev_screen);
    }
}

void
app_frame(struct app *a, struct app_input const *in, double now, struct vkr *r)
{
    double dt = a->last_now > 0 ? now - a->last_now : 0.016;
    dt = lm_clamp(dt, 0.0, 0.05);
    a->last_now = now;
    a->now = now;
    if (a->scr_t0 == 0)
        a->scr_t0 = now;

    load_images(a, r);
    vkr_begin_frame(r);
    if (a->toast_t > 0)
        a->toast_t -= (float)dt;
    if (a->buzz_t > 0)
        a->buzz_t -= dt;
    for (int i = 0; i < SHOP_COUNT; i++)
        if (a->shop_flash[i] > 0)
            a->shop_flash[i] -= dt * 2.2;

    switch (a->screen) {
    case SC_TITLE:
        update_title(a, in);
        break;
    case SC_PLAY: {
        struct lm_game *g = &a->g;
        bool jump = (in->held & JUMP_BUTTONS) != 0;
        if (a->jump_lock) {
            if (!jump)
                a->jump_lock = false;
            jump = false;
        }
        if (in->pressed & PAD_R3)
            toggle_mute(a);
        if (!a->over_pending && (in->pressed & PAD_OPTIONS)) {
            lm_audio_ui_select();
            lm_set_paused(g, true);
            a->pause_sel = 0;
            go(a, SC_PAUSE);
            break;
        }
        if (!a->over_pending) {
            if (in->pressed & PAD_SQUARE)
                lm_use_power(g, PU_SPRING_BOOTS);
            if (in->pressed & PAD_TRIANGLE)
                lm_use_power(g, PU_STARLIGHT_MAGNET);
            if (in->pressed & PAD_CIRCLE)
                lm_use_power(g, PU_CLOCKWORK_SLOW);
        }
        lm_set_input(g, (in->held & PAD_LEFT) != 0, (in->held & PAD_RIGHT) != 0, (in->held & PAD_UP) != 0,
                     (in->held & PAD_DOWN) != 0, jump);
        break;
    }
    case SC_PAUSE:
        update_pause(a, in);
        break;
    case SC_SHOP:
        update_shop(a, in);
        break;
    case SC_OVER:
        update_over(a, in);
        break;
    case SC_CONTINUE:
        update_continue(a, in);
        break;
    case SC_SETTINGS:
        update_settings(a, in);
        break;
    case SC_NAME:
        update_name(a, in);
        break;
    case SC_SCORES:
        update_scores(a, in);
        break;
    case SC_HELP:
        update_help(a, in);
        break;
    case SC_LEVELS:
        update_levels(a, in);
        break;
    }

    /* the simulation: runs in play (and animates behind overlays); paused states are frozen inside */
    bool pad_lost = !in->connected && a->screen == SC_PLAY; /* nothing moves while the controller is gone */
    if (!pad_lost && (a->screen == SC_PLAY || a->screen == SC_PAUSE || a->screen == SC_SETTINGS || a->screen == SC_SHOP)) {
        /* fixed 120 Hz simulation: identical jump arcs at 60 Hz, 120 Hz or after a hitch */
        enum { MAX_CATCHUP = 12 };
        const double fixed_dt = 1.0 / 120.0;
        a->sim_acc += dt > 0.1 ? 0.1 : dt;
        for (int n = 0; a->sim_acc >= fixed_dt && n < MAX_CATCHUP; n++) {
            lm_step(&a->g, fixed_dt);
            a->sim_acc -= fixed_dt;
        }
        if (a->sim_acc > fixed_dt)
            a->sim_acc = 0;
    }
    if (a->screen != SC_PLAY || pad_lost) {
        /* rumble only belongs to live play: its timer is only counted inside lm_step, so clear it here */
        a->g.rumble_time = 0;
        a->g.rumble_strength = 0;
    }
    if (a->screen == SC_PLAY) {
        process_events(a);
        if (a->screen == SC_PLAY && a->run_active && !a->over_pending &&
            (a->g.depth != a->save_depth || a->g.lives != a->save_lives || a->g.continues != a->save_continues))
            save_run(a); /* a new cavern, a lost life or a found Continue */
        if (a->over_pending) {
            a->over_timer -= dt;
            if (a->over_timer <= 0) {
                a->over_pending = false;
                a->over_sel = 0;
                if (a->g.down)
                    go(a, SC_CONTINUE);
                else if (a->g.practice)
                    leave_practice(a);
                else
                    finish_run(a);
            }
        }
    }
    if (a->dirty && a->screen != SC_PLAY && a->now - a->last_save > 0.75) /* not on every slider tick */
        save_all(a);

    /* row highlight animation per screen */
    switch (a->screen) {
    case SC_TITLE: anim_rows(a, a->menu_anim, TM_COUNT, a->title_sel, dt); break;
    case SC_PAUSE: anim_rows(a, a->row_anim, 5, a->pause_sel, dt); break;
    case SC_OVER:
    case SC_CONTINUE: anim_rows(a, a->row_anim, 2, a->over_sel, dt); break;
    case SC_SETTINGS: anim_rows(a, a->row_anim, SET_COUNT, a->set_sel, dt); break;
    default: break;
    }

    /* draw */
    switch (a->screen) {
    case SC_TITLE:
        draw_title(a, r, dt);
        break;
    case SC_PLAY:
    case SC_PAUSE: {
        struct lm_world_opts o = {0, 0, 2.0f, a->set.bloom};
        lm_draw_world(r, &a->g, &o);
        if (a->screen == SC_PLAY)
            draw_hud(a, r);
        else
            draw_pause(a, r);
        break;
    }
    case SC_SETTINGS:
        if (a->prev_screen == SC_PAUSE) {
            struct lm_world_opts o = {0, 0, 2.0f, a->set.bloom};
            lm_draw_world(r, &a->g, &o);
        }
        draw_settings(a, r);
        break;
    case SC_SHOP:
        draw_shop(a, r);
        break;
    case SC_OVER:
        draw_over(a, r);
        break;
    case SC_CONTINUE:
        draw_continue(a, r);
        break;
    case SC_HELP:
        draw_help(a, r);
        break;
    case SC_LEVELS:
        draw_levels(a, r);
        break;
    case SC_NAME:
        draw_name(a, r);
        break;
    case SC_SCORES:
        draw_scores(a, r);
        break;
    }
    if (!in->connected) {
        vkr_rect(r, 0, 0, VKR_W, VKR_H, HEXA(0x0a080d, 0.72));
        ui_text_c(r, VKR_DISPLAY, 60, 960, 440, HEX(C_CREAM), "Controller disconnected");
        ui_text_c(r, VKR_SANS, 30, 960, 530, HEX(C_MUTED), "Reconnect a DualSense to continue");
    }
    /* over every screen, so a change shows straight away in Settings */
    if (a->set.scan)
        vkr_scanlines(r, 0, 0, VKR_W, VKR_H, vkr_scale(r) * 3.0f, HEX(0x000000), a->set.scan == 1 ? 0.30f : 0.55f);
    if (a->screen == SC_TITLE && in->connected)
        draw_qr(a, r);
    draw_fade_in(a, r);
    draw_splash(a, r);
}
