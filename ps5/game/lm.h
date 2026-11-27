/*
 * Lantern Miner: the game itself. Nothing in here is PS5 specific.
 *
 * Copyright (C) 2026 RedHotBallBag
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lm_math.h"

#define TILE_SIZE 30
#define GRID_COLS 32
#define GRID_ROWS 18
#define CANVAS_W (GRID_COLS * TILE_SIZE) /* 960 */
#define CANVAS_H (GRID_ROWS * TILE_SIZE) /* 540 */
#define LM_VERSION "0.10.0"  /* shown on the loading splash and the title screen */
#define LM_BUILD 19
#define LM_TEST_MENU 0       /* Cavern Select on the title menu, for testing. Set to 0 before a release */
#define START_LIVES 5
#define MAX_CONTINUES 3
#define WORLD_SIZE 10 /* caverns to a world */
#define MAX_LIVES 9
#define BASE_HEARTS 2 /* hits you can take before losing a life */
#define SHIELD_CAVERNS 3 /* a bought Lantern Shield renews this many times */
#define SHOP_INTERVAL 5 /* a shop every five caverns */

/* movement */
#define GRAVITY 1180.0
#define MAX_FALL_SPEED 560.0
#define MOVE_SPEED 218.0
#define MOVE_ACCEL_GROUND 1950.0
#define MOVE_ACCEL_AIR 1280.0
#define JUMP_VELOCITY (-465.0) /* climbs 3 rows with little to spare; the maps rely on that */
#define SPRING_JUMP_VELOCITY (-540.0)
#define DOUBLE_JUMP_VELOCITY (-420.0)
#define JUMP_CUT_MULTIPLIER 0.52
#define COYOTE_TIME 0.115
#define JUMP_BUFFER_TIME 0.125
#define CONVEYOR_PUSH_SPEED 78.0
#define INVULN_DURATION 1.65
#define PIT_BOUNCE_VELOCITY (-540.0) /* throws the miner about 4 rows up, to find somewhere dry */
#define CRUMBLE_SHAKE_TIME 0.72
#define CRUMBLE_RESPAWN_TIME 4.2
#define MAX_AIR 100.0
#define CLIMB_SPEED 125.0
/* Landing further than this below the highest point of a fall costs a life. A jump rises about 3 rows, so a jump
   off a ledge 5 rows up is safe, as is stepping off one 8 rows up. */
#define MAX_SAFE_FALL (TILE_SIZE * 8.5)
#define MOVER_THICKNESS 10
#define AIR_BOTTLE 30.0 /* the one thing in a cavern that tops the air up; each map sets how long a full tank lasts */
#define LIFE_PRICE_STEP 4000 /* each spare life bought makes the next one this much dearer */
/* cursed chests */
#define LEAD_TIME 12.0
#define LEAD_SPEED 0.6
#define MUDDLE_TIME 10.0
#define LEAK_AIR 22.0
#define POWER_WARNING 3.0 /* a running power flashes and ticks for this long before it ends */
/* water: a surface at row r sits just under a ledge in that row, so rows r and up stay dry */
#define WATER_LIP 9       /* how far below the top of its row the surface lies */
#define TIDE_SPEED 90.0   /* pixels a second, rising and falling */
#define TIDE_FIRST 5.0    /* seconds from the cavern appearing to the first rise */
#define TIDE_WARNING 2.0  /* seconds of bubbles before it rises */
#define DRAIN_SPEED 75.0
#define RAFT_FREEBOARD 10 /* a raft floats this far above the water */
#define RAFT_DEPTH 14     /* and goes under after sinking this far */
#define RAFT_GRACE 0.5    /* seconds a raft holds before it starts to go */
#define RAFT_SINK_TIME 1.3
#define FISH_GRAVITY 700.0
#define FISH_WARNING 0.6

enum lm_tile { T_EMPTY, T_SOLID, T_ONE_WAY, T_CONVEYOR_LEFT, T_CONVEYOR_RIGHT, T_CRUMBLE, T_BRAMBLE, T_PIT };

enum lm_biome_id { BIOME_TIMBER_HEARTH, BIOME_AMETHYST_GROTTO, BIOME_CLOCKWORK_CELLAR, BIOME_GLOW_MOSS, BIOME_FROST_LANTERN, BIOME_FLOODED, BIOME_COUNT };

enum lm_powerup { PU_SPRING_BOOTS, PU_LANTERN_SHIELD, PU_TEA_THERMOS, PU_STARLIGHT_MAGNET, PU_CLOCKWORK_SLOW, PU_COUNT };

enum lm_perk {
    PERK_FEATHERSTEP, PERK_EXTRA_HEARTH, PERK_BRASS_LANTERN, PERK_CONVEYOR_CLEATS, PERK_GEM_RESONANCE,
    PERK_CRUMBLE_LIGHTFOOT, PERK_CHRONOMETER, PERK_COUNT
};

/* shop items, in catalogue order; most of the early ones carry a perk */
enum lm_shop_item {
    SHOP_KETTLE_BREW, SHOP_LANTERN_WARD, SHOP_CRUMBLE_LIGHTFOOT, SHOP_CONVEYOR_CLEATS, SHOP_BRASS_LANTERN,
    SHOP_GEM_RESONANCE, SHOP_FEATHERSTEP, SHOP_EXTRA_HEARTH, SHOP_CHRONOMETER,
    /* Lantern Powers. Order matches enum lm_powerup. */
    SHOP_SPRING_BOOTS, SHOP_LANTERN_SHIELD, SHOP_TEA_THERMOS, SHOP_STARLIGHT_MAGNET, SHOP_CLOCKWORK_SLOW,
    SHOP_EXTRA_LIFE, SHOP_CONTINUE,
    SHOP_COUNT
};
enum lm_shop_tier { TIER_SUPPLY, TIER_GEAR, TIER_MASTERWORK, TIER_POWER, TIER_SPECIAL };

enum lm_enemy_kind { EN_BEETLE, EN_BOUNCER, EN_GLOW_BAT, EN_PISTON, EN_WISP, EN_CHARGER, EN_SPITTER, EN_STALKER, EN_SPIDER, EN_FISH };

enum lm_music { MUSIC_NONE, MUSIC_TITLE, MUSIC_CAVERN, MUSIC_CAMPFIRE };

/* colours are 0xRRGGBB */
struct lm_biome {
    char const *label;
    uint32_t bg_top, bg_bottom, wall_fill, wall_highlight, wall_shadow, timber_top, timber_body, lantern_glow, accent_spec, dust;
    char const *prefixes[5];
    char const *nouns[5];
};

struct lm_powerup_def { char const *name, *short_label, *description; double duration; uint32_t color; };
struct lm_perk_def { char const *name, *short_label, *description; uint32_t accent; int max_stacks; };
struct lm_shop_def {
    char const *name, *short_label, *description;
    int cost;
    enum lm_shop_tier tier;
    uint32_t accent;
    int max_stacks;   /* 0 = repeatable supply */
    int perk;         /* enum lm_perk or -1 */
};

extern struct lm_biome const lm_biomes[BIOME_COUNT];
extern struct lm_powerup_def const lm_powerups[PU_COUNT];
extern struct lm_perk_def const lm_perks[PERK_COUNT];
extern struct lm_shop_def const lm_shop[SHOP_COUNT];

/* ----------------------------------------------------------------- cavern */

enum lm_collect_kind { COL_KEY, COL_RELIC, COL_CHEST, COL_AIR, COL_PLUG }; /* a plug keeps the row it drains to in `powerup` */
/* what a chest holds: one of enum lm_powerup, or one of these */
enum { CHEST_JACKPOT = PU_COUNT, CHEST_LIFE, CHEST_CONTINUE,
       CHEST_LEAD, CHEST_MUDDLE, CHEST_LEAK, CHEST_GHOUL, /* the nasty ones */
       CHEST_KINDS };

struct lm_collectable {
    int kind;       /* enum lm_collect_kind */
    int powerup;    /* enum lm_powerup */
    int relic_variant;
    double x, y, base_y;
    bool collected;
    double phase;
};

struct lm_enemy_spawn {
    int kind;
    double x, y, min_x, max_x, min_y, max_y, speed;
    int direction;
    double phase;
};

struct lm_lantern { double x, y, sway_phase, chain_length; };

#define LM_MAX_COLLECT 32
#define LM_MAX_ENEMIES 32
#define LM_MAX_LANTERNS 5
#define LM_MAX_MOVERS 10

/* A one-way platform that slides between (min_x, min_y) and (max_x, max_y); x / y is its top-left corner. */
struct lm_mover {
    double x, y, width, min_x, max_x, min_y, max_y, speed;
    int direction;
    bool vertical;
    double dx, dy; /* movement during the last step, used to carry the miner */
    bool raft;     /* floats on the water, and sinks under a miner who lingers */
    double home_y, sink, linger; /* resting height when there is no water to lift it; how far under, 0 to 1 */
};

/* The cavern's water: it waits low, warns, rises, holds and falls. low_y == high_y is still water. */
struct lm_tide { bool on; double low_y, high_y, low_secs, high_secs; };
enum lm_tide_phase { TIDE_LOW, TIDE_WARN, TIDE_RISING, TIDE_HIGH, TIDE_FALLING };
double lm_tide_at(struct lm_tide const *t, double clock, int *phase); /* surface height at that moment */

struct lm_cavern {
    int depth;
    int biome;
    double air_secs; /* how long a full tank lasts here */
    char name[96];
    char hint[48];
    uint8_t tiles[GRID_ROWS][GRID_COLS];
    struct { double x, y; } spawn;
    struct { double x, y; int col, row; } portal;
    struct lm_collectable collect[LM_MAX_COLLECT];
    int n_collect;
    struct lm_enemy_spawn enemies[LM_MAX_ENEMIES];
    int n_enemies;
    struct lm_lantern lanterns[LM_MAX_LANTERNS];
    int n_lanterns;
    int keys_total;
    uint8_t ladders[GRID_ROWS][GRID_COLS]; /* 1 = ladder; laid over the tiles so it can pass through a ledge */
    struct lm_mover movers[LM_MAX_MOVERS]; /* starting positions; the live copy moves in struct lm_game */
    int n_movers;
    struct lm_tide tide;
};

void lm_generate_cavern(char const *seed, int depth, struct lm_cavern *out);
int lm_level_count(void);
void lm_build_level(int index, bool mirror, char const *seed, int depth, struct lm_cavern *out);

/* ------------------------------------------------------------------ engine */

struct lm_player {
    double x, y, vx, vy, width, height;
    int facing;
    bool grounded;
    double coyote, jump_buffer;
    bool can_air_jump, has_used_double_jump;
    bool bouncing; /* splashed a moment ago: going in again costs again */
    bool thrown; /* still rising from a splash, so letting go of jump does not cut it short */
    double invuln, squash_x, squash_y, walk_phase;
    int on_conveyor;
    bool climbing;
    int rider; /* moving platform being stood on, -1 for none */
    double fall_top; /* highest point since the miner last had hold of something */
    double lead, muddle; /* seconds left of a cursed chest: slowed down, left and right swapped */
    bool has_shield;
    double spring, magnet, slow; /* seconds left of a running power */
    struct { double x, y; } scarf[4];
};

struct lm_enemy {
    struct lm_enemy_spawn s;
    double vx, vy, width, height, timer, telegraph, cooldown, stun, anchor_x, anchor_y;
    bool is_jumping, is_charging;
};

struct lm_projectile { double x, y, vx, vy, radius, life; uint32_t color; };

enum lm_crumble_state { CR_NONE, CR_INTACT, CR_SHAKING, CR_BROKEN };
struct lm_crumble { uint8_t state; double timer, max_shake; };

struct lm_particle { double x, y, vx, vy, size, alpha, life, max_life, gravity; uint32_t color; };
struct lm_float_text { double x, y, vy, life, max_life; uint32_t color; char text[40]; };
struct lm_mote { double x, y, vx, vy, radius, alpha, phase; };
struct lm_banner { char title[96], subtitle[160]; double remaining, duration; bool active; };

#define LM_MAX_PARTICLES 160
#define LM_MAX_FLOATS 24
#define LM_MAX_PROJ 48
#define LM_MOTES 34

/* One-shot events the app consumes after each update. */
enum lm_event { EV_NONE = 0, EV_CAVERN_ADVANCED = 1, EV_OPEN_SHOP = 2, EV_GAME_OVER = 4, EV_CONTINUE = 8 };

struct lm_run_summary {
    int depth_reached, caverns_cleared, score, high_score;
    bool new_high_score, new_best_depth;
    int relics, relic_points_left, keys;
    char seed[40];
    bool air; /* true = ran out of air, false = out of hearts */
    bool won; /* every cavern built so far has been cleared */
};

struct lm_game {
    /* input */
    bool in_left, in_right, in_up, in_down, in_jump, prev_jump;
    /* run state */
    char seed[40];
    int depth, score, relic_points, high_score, best_depth, hearts, max_hearts;
    double air;
    int total_relics_run, total_keys_run, relics_this_cavern;
    int perks[PERK_COUNT];
    int powers[PU_COUNT]; /* Spring, Magnet, Slow: 1 = carried, waiting for its button. Tea: flasks. Shield: caverns left */
    double power_time[PU_COUNT]; /* seconds left on a running Spring, Magnet or Slow; they end with the cavern */
    int hurt_depth;              /* last cavern the miner was hit or lost a life in: no clean bonus there */
    bool next_shield;
    int lives;           /* lives left, this one included */
    int continues, continues_bought;
    int lives_bought;    /* spare lives bought this run: sets the price of the next */
    int world_score;     /* score on entering this world: a continue goes back to it */
    bool flawless;       /* no life lost in this world yet: finishing it earns a continue */
    bool flawless_award; /* set when that continue has just been handed out */
    bool down;           /* out of lives, waiting for the player to use a continue or give up */
    int run_start_high, run_start_depth; /* records when the run began, for the "new best" banners */
    double respawn_timer; /* >0 while the miner is down and about to restart the cavern */
    /* cavern */
    struct lm_cavern cavern;
    struct lm_player player;
    struct lm_enemy enemies[LM_MAX_ENEMIES];
    int n_enemies;
    struct lm_projectile proj[LM_MAX_PROJ];
    int n_proj;
    struct lm_crumble crumble[GRID_ROWS][GRID_COLS];
    struct lm_mover movers[LM_MAX_MOVERS];
    int n_movers;
    double tide_clock, water_y;  /* time into the tide, and where the surface is now (off the bottom if dry) */
    double drain_y, drain_to;    /* pulled plugs hold the water at or below this */
    int tide_phase;
    bool practice;               /* started from Cavern Select: nothing is saved or scored */
    int chest_taken;       /* depth whose chest has been opened, so a retry does not refill it */
    int paid_depth;        /* the cavern `paid` belongs to */
    uint32_t paid;         /* one bit per collectable already scored there, so a retry cannot score it twice */
    struct lm_particle particles[LM_MAX_PARTICLES];
    int n_particles;
    struct lm_float_text floats[LM_MAX_FLOATS];
    int n_floats;
    struct lm_mote motes[LM_MOTES];
    int keys_collected;
    bool portal_unlocked, transition_locked;
    double pending_advance;
    struct lm_banner banner;
    /* feel */
    double hitstop, shake_intensity, shake_x, shake_y, hit_flash;
    double elapsed;
    bool paused, running;
    /* events for the app */
    unsigned events;
    struct { int depth_cleared, relics_in_cavern, total_score, air_bonus, relic_bonus; bool clean; } adv;
    struct lm_run_summary summary;
    /* request for haptics: strength 0..1 and duration in seconds */
    double rumble_strength, rumble_time;
    /* settings */
    bool shake_enabled;
    uint32_t rng_state;
};

void lm_init(struct lm_game *g, uint32_t rng_seed);
void lm_set_records(struct lm_game *g, int high_score, int best_depth);
void lm_start_run(struct lm_game *g, char const *seed);
void lm_start_practice(struct lm_game *g, int depth, bool stocked); /* straight to any cavern, for testing */
void lm_reset_water(struct lm_game *g); /* tide, drain and rafts back to how a cavern starts */
void lm_set_paused(struct lm_game *g, bool paused);
void lm_set_input(struct lm_game *g, bool left, bool right, bool up, bool down, bool jump);
void lm_step(struct lm_game *g, double dt);
bool lm_use_power(struct lm_game *g, int power); /* starts a carried Spring, Magnet or Slow */
void lm_leave_shop(struct lm_game *g);
int lm_world_of(int depth);        /* 1, 2, 3 ... */
void lm_use_continue(struct lm_game *g);
void lm_give_up(struct lm_game *g);
/* A run in progress, as text. `in_shop` says where to resume. lm_load_run returns false if the file is no good. */
void lm_save_run(struct lm_game const *g, FILE *f, bool in_shop);
bool lm_load_run(struct lm_game *g, FILE *f, bool *in_shop);
/* Shop: status of an item for the current wallet. */
struct lm_shop_status { bool can_buy; char status[48]; char owned[32]; bool is_owned; };
void lm_shop_item_status(struct lm_game const *g, int item, struct lm_shop_status *out);
bool lm_shop_purchase(struct lm_game *g, int item);
int lm_cavern_until_shop(struct lm_game const *g);
double lm_rand(struct lm_game *g); /* 0 to 1 */
void lm_fmt_int(char *buf, size_t n, long v); /* 12,345 */

/* ------------------------------------------------------------------- audio */

void lm_audio_init(void);
bool lm_audio_load_title_track(char const *path);
void lm_audio_pull(int16_t *lr, size_t frames);
void lm_audio_set_muted(bool muted);
bool lm_audio_is_muted(void);
void lm_audio_set_volumes(float music, float sfx);
void lm_audio_set_music_mode(int mode, double tempo);
void lm_audio_update_tempo(double tempo);
void lm_audio_jump(bool dbl);
void lm_audio_land(double intensity);
void lm_audio_key(int combo);
void lm_audio_relic(void);
int lm_shop_cost(struct lm_game const *g, int item);
int lm_shop_discount(struct lm_game const *g); /* percent off at this stall */
int lm_shop_unlock_at(int item);              /* shop number needed (1 = always) */
void lm_audio_powerup(void);
void lm_audio_portal_open(void);
void lm_audio_portal_enter(void);
void lm_audio_crumble(void);
void lm_audio_hurt(bool shield_broken);
void lm_audio_perk_select(void);
void lm_audio_shop_buy(void);
void lm_audio_spark(void);
void lm_audio_splash(void);
void lm_audio_tide(void);
void lm_audio_game_over(void);
void lm_audio_ui_move(void);
void lm_audio_ui_select(void);
void lm_audio_ui_back(void);

/* ------------------------------------------------------------------ render */

struct vkr;
struct lm_world_opts {
    float ox, oy, scale; /* top-left of the 960x540 world and virtual pixels per world pixel */
    bool bloom;          /* extra soft glow */
};
void lm_draw_world(struct vkr *r, struct lm_game const *g, struct lm_world_opts const *o);
/* One sprite from the game, centred at (x,y) at `scale` virtual pixels per world pixel: for the How to Play key. */
enum lm_icon {
    ICON_MINER, ICON_KEY, ICON_GEM_CUP, ICON_GEM, ICON_AIR, ICON_CHEST, ICON_PORTAL_SHUT, ICON_PORTAL_OPEN, ICON_BEETLE,
    ICON_CHARGER, ICON_BOUNCER, ICON_BAT, ICON_PISTON, ICON_WISP, ICON_SPITTER, ICON_STALKER, ICON_SPARK, ICON_SPIKES,
    ICON_POOL, ICON_CRUMBLE, ICON_CONVEYOR, ICON_LADDER, ICON_LIFT, ICON_FALL, ICON_SPIDER,
    ICON_TIDE, ICON_RAFT, ICON_PLUG, ICON_FISH
};
void lm_draw_icon(struct vkr *r, int icon, float x, float y, float scale, double time);

