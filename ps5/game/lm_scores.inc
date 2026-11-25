/*
 * High scores: name entry and the table, included by lm_app.c. Saved in scores.cfg as
 * "last=NAME" followed by one "NAME|score|depth" line per entry, best first.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define HS_MAX 10
#define HS_NAME 8
static char const HS_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.!";

static void
name_trim(char *dst, char const *src)
{
    int n = HS_NAME;
    while (n > 0 && src[n - 1] == ' ')
        n--;
    memcpy(dst, src, (size_t)n);
    dst[n] = 0;
    if (n == 0)
        snprintf(dst, HS_NAME + 1, "MINER");
}

static void
scores_save(struct app *a)
{
    char p[300], tmp[310];
    data_path(a, "scores.cfg", p, sizeof(p));
    snprintf(tmp, sizeof(tmp), "%s.tmp", p);
    FILE *f = fopen(tmp, "w");
    if (!f)
        return;
    fprintf(f, "last=%s\n", a->hs_last);
    for (int i = 0; i < a->hs_n; i++)
        fprintf(f, "%s|%d|%d\n", a->hs[i].name, a->hs[i].score, a->hs[i].depth);
    if (fclose(f) != 0)
        return;
    rename(tmp, p); /* atomic: a crash cannot leave half a table */
}

static void
scores_load(struct app *a)
{
    char p[300], line[96];
    a->hs_n = 0;
    snprintf(a->hs_last, sizeof(a->hs_last), "MINER");
    FILE *f = fopen(data_path(a, "scores.cfg", p, sizeof(p)), "r");
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "last=", 5)) {
            snprintf(a->hs_last, sizeof(a->hs_last), "%.8s", line + 5);
            continue;
        }
        char *b1 = strchr(line, '|');
        if (!b1 || a->hs_n >= HS_MAX)
            continue;
        *b1 = 0;
        struct hs_entry *e = &a->hs[a->hs_n];
        snprintf(e->name, sizeof(e->name), "%.8s", line);
        e->score = atoi(b1 + 1);
        char *b2 = strchr(b1 + 1, '|');
        e->depth = b2 ? atoi(b2 + 1) : 0;
        a->hs_n++;
    }
    fclose(f);
    for (int i = 0; i < a->hs_n; i++) /* keep it sorted whatever the file says */
        for (int j = i + 1; j < a->hs_n; j++)
            if (a->hs[j].score > a->hs[i].score) {
                struct hs_entry t = a->hs[i];
                a->hs[i] = a->hs[j];
                a->hs[j] = t;
            }
}

static bool
hs_qualifies(struct app const *a, int score)
{
    return score > 0 && (a->hs_n < HS_MAX || score > a->hs[HS_MAX - 1].score);
}

/* returns the rank index of the new entry (ties rank below older scores) */
static int
hs_insert(struct app *a, char const *name, int score, int depth)
{
    int pos = 0;
    while (pos < a->hs_n && a->hs[pos].score >= score)
        pos++;
    if (pos >= HS_MAX)
        return -1;
    int last = a->hs_n < HS_MAX ? a->hs_n : HS_MAX - 1;
    for (int i = last; i > pos; i--)
        a->hs[i] = a->hs[i - 1];
    snprintf(a->hs[pos].name, sizeof(a->hs[pos].name), "%.8s", name);
    a->hs[pos].score = score;
    a->hs[pos].depth = depth;
    if (a->hs_n < HS_MAX)
        a->hs_n++;
    return pos;
}

/* after a run ends: a table-worthy score asks for a name first, otherwise straight to the summary */
static void
finish_run(struct app *a)
{
    if (hs_qualifies(a, a->g.summary.score)) {
        memset(a->name_buf, ' ', HS_NAME);
        a->name_buf[HS_NAME] = 0;
        memcpy(a->name_buf, a->hs_last, strlen(a->hs_last) < HS_NAME ? strlen(a->hs_last) : HS_NAME);
        a->name_cur = 0;
        go(a, SC_NAME);
    } else {
        go(a, SC_OVER);
    }
}

static void
update_name(struct app *a, struct app_input const *in)
{
    uint32_t p = in->pressed;
    if (a->now - a->scr_t0 < 0.7) /* the death-screen button mashing must not confirm a name */
        return;
    int n = (int)strlen(HS_CHARS);
    char *c = &a->name_buf[a->name_cur];
    char const *at = strchr(HS_CHARS, *c);
    int idx = at ? (int)(at - HS_CHARS) : 0;
    bool confirm = (p & PAD_OPTIONS) != 0;
    if (p & PAD_UP) { *c = HS_CHARS[(idx + 1) % n]; lm_audio_ui_move(); }
    if (p & PAD_DOWN) { *c = HS_CHARS[(idx + n - 1) % n]; lm_audio_ui_move(); }
    if ((p & PAD_RIGHT) && a->name_cur < HS_NAME - 1) { a->name_cur++; lm_audio_ui_move(); }
    if ((p & PAD_LEFT) && a->name_cur > 0) { a->name_cur--; lm_audio_ui_move(); }
    if (p & (PAD_CIRCLE | PAD_SQUARE)) { /* backspace */
        *c = ' ';
        if (a->name_cur > 0)
            a->name_cur--;
        lm_audio_ui_back();
    }
    if (p & PAD_CROSS) {
        if (a->name_cur < HS_NAME - 1) { a->name_cur++; lm_audio_ui_move(); }
        else confirm = true;
    }
    if (confirm) {
        char name[HS_NAME + 1];
        name_trim(name, a->name_buf);
        snprintf(a->hs_last, sizeof(a->hs_last), "%s", name);
        a->hs_new = hs_insert(a, name, a->g.summary.score, a->g.summary.depth_reached);
        scores_save(a);
        a->hs_from_over = true;
        lm_audio_ui_select();
        go(a, SC_SCORES);
    }
}

static void
update_scores(struct app *a, struct app_input const *in)
{
    if (a->now - a->scr_t0 < 0.4)
        return;
    if (in->pressed & (PAD_CROSS | PAD_CIRCLE | PAD_OPTIONS)) {
        lm_audio_ui_back();
        if (a->hs_from_over) {
            a->hs_from_over = false;
            a->hs_new = -1;
            a->over_sel = 0;
            go(a, SC_OVER);
        } else {
            go(a, SC_TITLE);
        }
    }
}

static void
draw_scores_backdrop(struct app *a, struct vkr *r)
{
    draw_cover(a, r, IMG_CAMP, HEX(0xa0a0aa));
    vkr_rect(r, 0, 0, VKR_W, VKR_H, HEXA(0x0a080d, 0.62));
    vkr_vignette(r, 0, 0, VKR_W, VKR_H, 960, 540, 420, 1150, HEX(0x000000), 0.6f);
}

static void
draw_name(struct app *a, struct vkr *r)
{
    char b[64];
    double t = a->now;
    draw_scores_backdrop(a, r);
    float pw = 1000, ph = 640, px = (VKR_W - pw) / 2, py = 190;
    ui_panel(r, px, py, pw, ph, 30, 0.94f);
    ui_text_c(r, VKR_MONO, 26, 960, py + 40, HEXA(C_GREEN, 0.8 + 0.2 * lm_sin(t * 5)), "NEW HIGH SCORE");
    lm_fmt_int(b, sizeof(b), a->g.summary.score);
    ui_text_c(r, VKR_MONO_BOLD, 84, 960, py + 84, HEX(C_YELLOW), b);
    snprintf(b, sizeof(b), "Cavern %02d", a->g.summary.depth_reached);
    ui_text_c(r, VKR_SANS, 28, 960, py + 190, HEX(C_MUTED), b);
    ui_text_c(r, VKR_DISPLAY_SEMI, 40, 960, py + 250, HEX(C_CREAM), "Enter your name");

    float sw = 84, gap = 12, total = HS_NAME * sw + (HS_NAME - 1) * gap, x0 = 960 - total / 2, y0 = py + 330;
    for (int i = 0; i < HS_NAME; i++) {
        float x = x0 + i * (sw + gap);
        bool cur = i == a->name_cur;
        vkr_rrect(r, x, y0, sw, 104, 14, cur ? HEXA(C_PANEL2, 1.0) : HEXA(0x191520, 0.95));
        vkr_ring(r, x, y0, sw, 104, 14, cur ? 3.0f : 1.5f, cur ? HEX(C_AMBER) : HEXA(C_CREAM, 0.14));
        char ch[2] = {a->name_buf[i], 0};
        if (ch[0] != ' ')
            ui_text_c(r, VKR_MONO_BOLD, 64, x + sw / 2, y0 + 20, HEX(cur ? C_AMBER : C_CREAM), ch);
        else if (cur)
            ui_text_c(r, VKR_MONO_BOLD, 64, x + sw / 2, y0 + 20, HEXA(C_AMBER, 0.35 + 0.35 * lm_sin(t * 8)), "_");
        if (cur) {
            float k = (float)lm_sin(t * 8) * 2;
            vkr_tri(r, x + sw / 2, y0 - 16 - k, x + sw / 2 - 12, y0 - 4 - k, x + sw / 2 + 12, y0 - 4 - k, HEX(C_AMBER));
            vkr_tri(r, x + sw / 2, y0 + 120 + k, x + sw / 2 - 12, y0 + 108 + k, x + sw / 2 + 12, y0 + 108 + k, HEX(C_AMBER));
        }
    }
    ui_text_c(r, VKR_SANS, 24, 960, py + 520, HEX(C_DIM), "Up / Down changes the letter. Press Options when you are done.");

    static int const gl[4] = {G_DPAD_UD, G_DPAD_LR, G_CROSS, G_OPTIONS};
    static char const *const lb[4] = {"Letter", "Move", "Next / Done", "Finish"};
    hintbar(r, 4, gl, lb);
}

static void
draw_scores(struct app *a, struct vkr *r)
{
    char b[64];
    double t = a->now;
    draw_scores_backdrop(a, r);
    float pw = 1100, ph = 880, px = (VKR_W - pw) / 2, py = 90;
    ui_panel(r, px, py, pw, ph, 30, 0.94f);
    ui_text_c(r, VKR_DISPLAY, 64, 960, py + 30, HEX(C_CREAM), "High Scores");
    vkr_rect(r, 800, py + 112, 320, 4, HEXA(C_AMBER, 0.8));

    float cx_rank = px + 70, cx_name = px + 170, cx_depth = px + 760, cx_score = px + pw - 70;
    float hy = py + 140;
    ui_text(r, VKR_MONO, 21, cx_rank, hy, HEX(C_DIM), "RANK");
    ui_text(r, VKR_MONO, 21, cx_name, hy, HEX(C_DIM), "NAME");
    ui_text_r(r, VKR_MONO, 21, cx_depth + 70, hy, HEX(C_DIM), "CAVERN");
    ui_text_r(r, VKR_MONO, 21, cx_score, hy, HEX(C_DIM), "SCORE");
    float y0 = py + 184, step = 62;
    for (int i = 0; i < HS_MAX; i++) {
        float y = y0 + i * step;
        bool fresh = i == a->hs_new;
        if (fresh) {
            float pulse = 0.5f + 0.5f * (float)lm_sin(t * 5);
            vkr_rrect(r, px + 36, y - 6, pw - 72, step - 6, 12, HEXA(C_AMBER, 0.12 + 0.10 * pulse));
            vkr_ring(r, px + 36, y - 6, pw - 72, step - 6, 12, 2.0f, HEX(C_AMBER));
        } else if (i % 2 == 0) {
            vkr_rrect(r, px + 36, y - 6, pw - 72, step - 6, 12, HEXA(C_CREAM, 0.04));
        }
        uint32_t col = fresh ? C_AMBER : i == 0 ? C_YELLOW : C_CREAM;
        snprintf(b, sizeof(b), "%d", i + 1);
        ui_text(r, VKR_MONO_BOLD, 34, cx_rank, y, HEX(i < 3 ? C_YELLOW : C_DIM), b);
        if (i < a->hs_n) {
            ui_text(r, VKR_MONO_BOLD, 34, cx_name, y, HEX(col), a->hs[i].name);
            snprintf(b, sizeof(b), "%02d", a->hs[i].depth);
            ui_text_r(r, VKR_MONO, 32, cx_depth + 70, y + 2, HEX(fresh ? C_AMBER : C_MUTED), b);
            lm_fmt_int(b, sizeof(b), a->hs[i].score);
            ui_text_r(r, VKR_MONO_BOLD, 34, cx_score, y, HEX(col), b);
        } else {
            ui_text(r, VKR_MONO, 34, cx_name, y, HEXA(C_DIM, 0.5), "--------");
        }
    }
    static int const gl[1] = {G_CROSS};
    static char const *const lb[1] = {"Continue"};
    static char const *const lb2[1] = {"Back"};
    hintbar(r, 1, gl, a->hs_from_over ? lb : lb2);
}
