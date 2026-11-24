/*
** Lantern Miner PS5 application: title, run, HUD, pause, Old Barnaby's shop, game over, settings. Platform independent;
** the platform feeds it input and a renderer each frame and calls app_audio_pull() from its audio thread.
*/
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../gfx/vkr.h"

/* scePad button bits */
enum {
    PAD_L3 = 0x2, PAD_R3 = 0x4, PAD_OPTIONS = 0x8,
    PAD_UP = 0x10, PAD_RIGHT = 0x20, PAD_DOWN = 0x40, PAD_LEFT = 0x80,
    PAD_L2 = 0x100, PAD_R2 = 0x200, PAD_L1 = 0x400, PAD_R1 = 0x800,
    PAD_TRIANGLE = 0x1000, PAD_CIRCLE = 0x2000, PAD_CROSS = 0x4000, PAD_SQUARE = 0x8000,
    PAD_TOUCHPAD = 0x100000,
};

struct app_input {
    uint32_t held;      /* buttons down now (the left stick counts as the D-pad) */
    uint32_t pressed;   /* new this frame; the four directions repeat while held */
    bool connected;
};

struct app_env {
    char data_dir[200];  /* e.g. /data/lantern-miner: save.cfg, logs/ */
    char asset_dir[200]; /* e.g. /app0/assets (art, fonts) */
    void (*log)(char const *fmt, ...);
};

struct app;

struct app *app_create(struct app_env const *env);
/* Updates and draws one frame. `now` is monotonic seconds. */
void app_frame(struct app *a, struct app_input const *in, double now, struct vkr *r);
/* Called from the audio thread: fills `frames` stereo S16 frames. */
void app_audio_pull(struct app *a, int16_t *lr, size_t frames);
/* Current haptics request (0..255 per motor); false when idle. */
bool app_rumble(struct app const *a, uint8_t *large, uint8_t *small);
/* True when the user chose "Exit to Home". */
bool app_wants_exit(struct app const *a);
/* Writes the save. Call before the process ends. */
void app_shutdown(struct app *a);

/* Test hooks for the desktop harness. */
enum { APP_SC_TITLE, APP_SC_PLAY, APP_SC_PAUSE, APP_SC_SHOP, APP_SC_OVER, APP_SC_SETTINGS, APP_SC_NAME, APP_SC_SCORES, APP_SC_CONTINUE };
struct lm_game;
struct lm_game *app_debug_game(struct app *a);
void app_debug_goto(struct app *a, int screen);
void app_debug_start(struct app *a, char const *seed, int depth);
int app_debug_screen(struct app const *a);
