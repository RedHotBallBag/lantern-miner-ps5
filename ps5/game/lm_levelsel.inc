/*
 * Cavern Select, included by lm_app.c: start any cavern for testing. Shown only when LM_TEST_MENU is set.
 * Nothing played from here is saved or scored.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define LEVELS_PER_COLUMN 14

static void
update_levels(struct app *a, struct app_input const *in)
{
    uint32_t p = in->pressed;
    int n = lm_level_count(), sel = a->level_sel;
    if (p & PAD_UP) sel = (sel + n - 1) % n;
    if (p & PAD_DOWN) sel = (sel + 1) % n;
    if (p & PAD_LEFT) sel = sel >= LEVELS_PER_COLUMN ? sel - LEVELS_PER_COLUMN : sel;
    if (p & PAD_RIGHT) sel = sel + LEVELS_PER_COLUMN < n ? sel + LEVELS_PER_COLUMN : n - 1;
    /* shoulders jump a world at a time */
    if (p & PAD_L1) sel = sel >= WORLD_SIZE ? sel - WORLD_SIZE : 0;
    if (p & PAD_R1) sel = sel + WORLD_SIZE < n ? sel + WORLD_SIZE : n - 1;
    if (sel != a->level_sel) {
        a->level_sel = sel;
        lm_audio_ui_move();
    }
    if (p & PAD_SQUARE) {
        a->level_stocked = !a->level_stocked;
        lm_audio_ui_move();
    }
    if (p & (PAD_CIRCLE | PAD_OPTIONS)) {
        lm_audio_ui_back();
        a->title_sel = 0;
        go(a, SC_TITLE);
        return;
    }
    if (p & PAD_CROSS) {
        lm_audio_ui_select();
        lm_start_practice(&a->g, sel + 1, a->level_stocked);
        a->run_active = true;
        a->over_pending = false;
        a->jump_lock = true;
        a->run_t0 = a->now - 100;
        go(a, SC_PLAY);
    }
}

static void
draw_levels(struct app *a, struct vkr *r)
{
    static char names[64][96];
    static int named;
    int n = lm_level_count() < 64 ? lm_level_count() : 64;
    char b[160];
    if (!named) {
        static struct lm_cavern cv;
        for (int i = 0; i < n; i++) {
            lm_generate_cavern("LIST", i + 1, &cv);
            snprintf(names[i], sizeof(names[i]), "%s", cv.name);
        }
        named = 1;
    }
    draw_cover(a, r, IMG_CAMP, HEX(0x55555f));
    ui_text_c(r, VKR_DISPLAY, 62, 960, 30, HEX(C_CREAM), "Cavern Select");
    ui_text_c(r, VKR_SANS, 26, 960, 112, HEX(C_MUTED), "For testing. Nothing here is saved or scored.");
    for (int i = 0; i < n; i++) {
        bool on = i == a->level_sel;
        float x = 100 + (i / LEVELS_PER_COLUMN) * 870, y = 162 + (i % LEVELS_PER_COLUMN) * 55;
        vkr_rrect(r, x, y, 850, 49, 10, on ? HEX(C_AMBER) : HEXA(C_PANEL, 0.92));
        uint32_t world = lm_world_of(i + 1) == 2 ? HEX(C_CYAN) : lm_world_of(i + 1) == 1 ? HEX(C_YELLOW) : HEX(C_MUTED);
        snprintf(b, sizeof(b), "%02d", i + 1);
        ui_text(r, VKR_MONO_BOLD, 28, x + 20, y + 9, on ? HEX(C_INK) : world, b);
        ui_text(r, VKR_DISPLAY_SEMI, 28, x + 84, y + 8, on ? HEX(C_INK) : HEX(C_CREAM), names[i]);
        if (lm_world_of(i + 1) > 2)
            ui_text_r(r, VKR_MONO, 20, x + 832, y + 14, on ? HEX(C_INK) : HEX(C_DIM), "old map");
    }
    snprintf(b, sizeof(b), a->level_stocked ? "Stocked up: every power, 3 teas, 20,000 points" : "Empty pockets, as a new miner");
    ui_text_c(r, VKR_MONO_BOLD, 26, 960, 950, HEX(a->level_stocked ? C_GREEN : C_MUTED), b);
    static int const gl[5] = {G_CROSS, G_SQUARE, G_L1, G_R1, G_CIRCLE};
    static char const *const lb[5] = {"Play", "Stock Up", "Back a World", "On a World", "Title"};
    hintbar(r, 5, gl, lb);
}
