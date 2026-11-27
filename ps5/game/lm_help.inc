/*
 * How to Play screens, included by lm_app.c. The sprites are drawn by the game itself
 * (lm_draw_icon), so the key always matches what is on screen.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

enum { HELP_BASICS, HELP_ITEMS, HELP_FOES, HELP_HAZARDS, HELP_WATER, HELP_CHESTS, HELP_SHOP, HELP_PAGES };

struct help_row {
    int icon; /* enum lm_icon, or HELP_WARE(shop item) */
    char const *name, *text;
    uint32_t col;
};

#define HELP_WARE(item) (-1 - (item))

static struct help_row const help_basics[] = {
    {ICON_MINER, "You", "Grab every key, then get to the open door. That's the cavern done.", C_AMBER},
    {ICON_AIR, "Air", "It runs out as you play. Only the little bottle tops it up, and there is one per cavern.", C_CYAN},
    {ICON_FALL, "The Drop", "Fall too far and you lose a life. Ladders break a fall. So does a mid-air second jump.", C_PINK},
    {HELP_WARE(SHOP_EXTRA_LIFE), "Lives", "You start with 5, each with 2 hearts. Touching nasties costs a heart.", C_PINK},
    {HELP_WARE(SHOP_CONTINUE), "Continues", "Lose all your lives with one in hand and you restart the world. Clear a world without dying to earn one.", C_GREEN},
    {ICON_PORTAL_OPEN, "Saving", "The game saves as you go. Pick Continue on the title screen.", C_GREEN},
};

static struct help_row const help_items[] = {
    {ICON_KEY, "Flashing Key", "Collect them all to open the door.", C_PINK},
    {ICON_PORTAL_SHUT, "Locked Door", "The dots above it show how many keys you have.", C_MUTED},
    {ICON_PORTAL_OPEN, "Open Door", "Walk in to finish. Air left over adds to your score.", C_GREEN},
    {ICON_AIR, "Air Bottle", "Gives back about a third of a tank. Pick your moment.", C_CYAN},
    {ICON_GEM_CUP, "Golden Teacup", "Relic Points to spend at Old Barnaby's shop.", C_YELLOW},
    {ICON_GEM, "Gems", "More Relic Points. Blue or green, same value.", C_CYAN},
    {ICON_CHEST, "Chest", "A gamble. Could be a treat, could be a curse.", C_AMBER},
};

static struct help_row const help_foes[] = {
    {ICON_BEETLE, "Clockwork Beetle", "Plods back and forth along its ledge.", C_AMBER},
    {ICON_CHARGER, "Charger", "Flashes when it spots you, then rushes. Jump or get out of its way.", C_PINK},
    {ICON_BOUNCER, "Bouncer", "Squats, then hops. Watch for the squat.", C_CREAM},
    {ICON_BAT, "Glow Bat", "Flies side to side, bobbing as it goes.", 0xb98cff},
    {ICON_PISTON, "Piston", "Pumps up and down. Slip past when it moves away.", C_CYAN},
    {ICON_WISP, "Wisp", "Drifts round in loops.", C_CYAN},
    {ICON_SPITTER, "Spitter", "Sits still and fires sparks along the row.", 0xb98cff},
    {ICON_SPIDER, "Drop Spider", "Twitches when you pass below, drops, then climbs back up. Wait it out.", 0xff9f43},
};

static struct help_row const help_hazards[] = {
    {ICON_SPIKES, "Brambles", "Costs a heart.", C_PINK},
    {ICON_POOL, "Deep Water", "Costs a heart and throws you out. Land somewhere dry: go in again and it costs another.", C_CYAN},
    {ICON_SPARK, "Spark", "Fired by Spitters. Costs a heart.", C_PINK},
    {ICON_CRUMBLE, "Crumbling Ledge", "Shakes, then gives way. It grows back after a few seconds.", C_AMBER},
    {ICON_CONVEYOR, "Conveyor", "Carries you the way the arrows point.", C_CYAN},
    {ICON_LADDER, "Ladder", "Up and Down to climb. Jump to hop off.", C_AMBER},
    {ICON_LIFT, "Moving Platform", "Lifts and sliders. The dotted line shows where they go.", C_YELLOW},
};

static struct help_row const help_water[] = {
    {ICON_TIDE, "The Tide", "Bubbles and arrows mean it is about to rise. The dotted line shows how far.", C_CYAN},
    {ICON_RAFT, "Raft", "Floats on the water. Stand still too long and it goes under, so keep hopping.", C_AMBER},
    {ICON_PLUG, "The Plug", "Pull it and the water drains. Some things can only be reached afterwards.", C_PINK},
    {ICON_FISH, "Leaping Fish", "Ripples first, then it leaps. Cross when it has dropped back in.", 0xff9f43},
};

static struct help_row const help_chests[] = {
    {ICON_CHEST, "The Good Stuff", "A shield, tea, a power to carry or treasure. Very rarely a life or Continue.", C_GREEN},
    {ICON_CHEST, "Lead Boots", "You move at a crawl for 12 seconds.", 0xb45cff},
    {ICON_CHEST, "Muddled", "Left is right and right is left for 10 seconds.", 0xb45cff},
    {ICON_CHEST, "Air Leak", "A fifth of your tank goes, just like that.", 0xb45cff},
    {ICON_STALKER, "Something Is Coming", "A ghost rises and follows you through walls until you leave.", 0xb45cff},
};

static struct help_row const help_shop[] = {
    {HELP_WARE(SHOP_KETTLE_BREW), "Kettle Brew", "Gives back a lost heart.", C_PINK},
    {HELP_WARE(SHOP_LANTERN_SHIELD), "Shields", "The Ward Charm and Lantern Shield soak up a hit.", C_AMBER},
    {HELP_WARE(SHOP_TEA_THERMOS), "Tea Thermos", "Drinks itself when your air runs out.", C_AMBER},
    {HELP_WARE(SHOP_SPRING_BOOTS), "Powers", "Square: Spring Boots. Triangle: Magnet. Circle: Slow. One use each.", C_GREEN},
    {HELP_WARE(SHOP_FEATHERSTEP), "Permanent Gear", "Slippers, lamp, cleats and more. Buy once, yours for the whole run.", C_GREEN},
    {HELP_WARE(SHOP_EXTRA_LIFE), "Spare Lantern", "An extra life. A luxury, and dearer every time.", C_PINK},
    {HELP_WARE(SHOP_CONTINUE), "Continue Lantern", "On sale from the second stall. Costly.", C_GREEN},
    {ICON_GEM_CUP, "Earning Points", "Gems, plus a bonus for each cavern cleared. More if not hit.", C_YELLOW},
};

#define HELP_PAGE(rows) rows, (int)(sizeof(rows) / sizeof(rows[0]))

static struct { char const *tab, *title; struct help_row const *rows; int n; } const help_pages[HELP_PAGES] = {
    [HELP_BASICS] = {"The Job", "How to Play", HELP_PAGE(help_basics)},
    [HELP_ITEMS] = {"Items", "Things to Collect", HELP_PAGE(help_items)},
    [HELP_FOES] = {"Guardians", "The Guardians", HELP_PAGE(help_foes)},
    [HELP_HAZARDS] = {"Hazards", "Hazards and Handy Things", HELP_PAGE(help_hazards)},
    [HELP_WATER] = {"Water", "The Flooded Hollow", HELP_PAGE(help_water)},
    [HELP_CHESTS] = {"Chests", "Chests: Think Twice", HELP_PAGE(help_chests)},
    [HELP_SHOP] = {"Shop", "Old Barnaby's Shop (every 5th cavern)", HELP_PAGE(help_shop)},
};

static void
open_help(struct app *a)
{
    a->help_from = a->screen;
    a->help_page = HELP_BASICS;
    go(a, SC_HELP);
}

static void
update_help(struct app *a, struct app_input const *in)
{
    uint32_t p = in->pressed;
    int step = (p & (PAD_R1 | PAD_RIGHT)) ? 1 : (p & (PAD_L1 | PAD_LEFT)) ? -1 : 0;
    if (step) {
        a->help_page = (a->help_page + step + HELP_PAGES) % HELP_PAGES;
        lm_audio_ui_move();
    }
    if (p & (PAD_CIRCLE | PAD_OPTIONS)) {
        lm_audio_ui_back();
        go(a, a->help_from);
    }
}

static void
draw_help(struct app *a, struct vkr *r)
{
    draw_cover(a, r, IMG_CAMP, HEX(0x6c6c78));
    int const page = a->help_page;

    ui_text_c(r, VKR_DISPLAY, 62, 960, 34, HEX(C_CREAM), help_pages[page].title);
    ui_spectrum_rule(r, 660, 118, 600, 5, 0.9f);

    /* page tabs */
    float const tw = 236, gap = 12, tx = 960 - (HELP_PAGES * tw + (HELP_PAGES - 1) * gap) / 2;
    for (int i = 0; i < HELP_PAGES; i++) {
        float x = tx + i * (tw + gap);
        vkr_rrect(r, x, 144, tw, 52, 12, i == page ? HEX(C_AMBER) : HEXA(C_PANEL, 0.92));
        ui_text_c(r, VKR_SANS_BOLD, 26, x + tw / 2, 154, i == page ? HEX(C_INK) : HEX(C_MUTED), help_pages[i].tab);
    }

    /* two columns of cards */
    int const n = help_pages[page].n, per = (n + 1) / 2;
    float const cw = 850, ch = per > 3 ? 164 : 184, cg = 20;
    for (int i = 0; i < n; i++) {
        struct help_row const *row = &help_pages[page].rows[i];
        float x = i < per ? 100 : 100 + cw + 20, y = 222 + (i % per) * (ch + cg);
        ui_panel(r, x, y, cw, ch, 18, 0.93f);
        float ix = x + 82, iy = y + ch / 2;
        vkr_rrect(r, x + 18, iy - 62, 128, 124, 14, HEXA(0x0c0a10, 0.9));
        if (row->icon >= 0)
            lm_draw_icon(r, row->icon, ix, iy, 2.8f, a->now);
        else
            ui_item_icon(r, -1 - row->icon, ix, iy, 72, row->col);
        ui_text(r, VKR_DISPLAY_SEMI, 34, x + 172, y + 20, HEX(row->col), row->name);
        ui_paragraph(r, VKR_SANS, 25, x + 172, y + 70, cw - 200, 34, HEX(C_CREAM), row->text);
    }

    if (page == HELP_BASICS) {
        float x = 190, y = 940;
        x += ui_hint(r, G_DPAD_LR, x, y, "Move", 40) + 60;
        x += ui_hint(r, G_CROSS, x, y, "Jump (hold = higher)", 40) + 60;
        x += ui_hint(r, G_DPAD_UD, x, y, "Climb", 40) + 60;
        x += ui_hint(r, G_SQUARE, x, y, "", 40);
        x += ui_hint(r, G_TRIANGLE, x, y, "", 40);
        x += ui_hint(r, G_CIRCLE, x, y, "Powers", 40) + 60;
        x += ui_hint(r, G_OPTIONS, x, y, "Pause", 40) + 60;
        ui_hint(r, G_R3, x, y, "Mute", 40);
    }

    static int const gl[3] = {G_L1, G_R1, G_CIRCLE};
    static char const *const lb[3] = {"Last Page", "Next Page", "Back"};
    hintbar(r, 3, gl, lb);
}
