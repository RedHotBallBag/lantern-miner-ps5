/*
** Lantern Miner: game core (C port of the TypeScript engine, generator and catalogs). Platform independent.
** Simulation uses doubles and the same operation order as the web version so a seed builds the same cavern.
*/
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lm_math.h"

#define TILE_SIZE 30
#define GRID_COLS 32
#define GRID_ROWS 18
#define CANVAS_W (GRID_COLS * TILE_SIZE) /* 960 */
#define CANVAS_H (GRID_ROWS * TILE_SIZE) /* 540 */
#define LM_VERSION "0.5"   /* shown on the loading splash and the title screen */
#define LM_BUILD 5
#define START_LIVES 5
#define MAX_LIVES 9
#define BASE_HEARTS 2 /* hits you can take before losing a life */
#define SHIELD_CAVERNS 3 /* a bought Lantern Shield renews this many times */
#define SHOP_INTERVAL 5 /* the web version had 10; power-ups are now only sold here, so the stall comes sooner */

/* "Cosy arcade" physics tuning (unchanged from the web version) */
#define GRAVITY 1180.0
#define MAX_FALL_SPEED 560.0
#define MOVE_SPEED 218.0
#define MOVE_ACCEL_GROUND 1950.0
#define MOVE_ACCEL_AIR 1280.0
#define JUMP_VELOCITY (-505.0)
#define SPRING_JUMP_VELOCITY (-585.0)
#define DOUBLE_JUMP_VELOCITY (-455.0)
#define JUMP_CUT_MULTIPLIER 0.52
#define COYOTE_TIME 0.115
#define JUMP_BUFFER_TIME 0.125
#define CONVEYOR_PUSH_SPEED 78.0
#define INVULN_DURATION 1.65
#define CRUMBLE_SHAKE_TIME 0.72
#define CRUMBLE_RESPAWN_TIME 4.2
#define MAX_AIR 100.0
#define BASE_AIR_DRAIN_PER_SEC 1.48

enum lm_tile { T_EMPTY, T_SOLID, T_ONE_WAY, T_CONVEYOR_LEFT, T_CONVEYOR_RIGHT, T_CRUMBLE, T_BRAMBLE };

enum lm_biome_id { BIOME_TIMBER_HEARTH, BIOME_AMETHYST_GROTTO, BIOME_CLOCKWORK_CELLAR, BIOME_GLOW_MOSS, BIOME_FROST_LANTERN, BIOME_COUNT };

enum lm_powerup { PU_SPRING_BOOTS, PU_LANTERN_SHIELD, PU_TEA_THERMOS, PU_STARLIGHT_MAGNET, PU_CLOCKWORK_SLOW, PU_COUNT };

enum lm_perk {
    PERK_FEATHERSTEP, PERK_EXTRA_HEARTH, PERK_BRASS_LANTERN, PERK_CONVEYOR_CLEATS, PERK_GEM_RESONANCE,
    PERK_CRUMBLE_LIGHTFOOT, PERK_CHRONOMETER, PERK_COUNT
};

/* Shop catalog order (as in the web version). Items 0 and 1 are supplies; 2.. carry a perk. */
enum lm_shop_item {
    SHOP_KETTLE_BREW, SHOP_LANTERN_WARD, SHOP_CRUMBLE_LIGHTFOOT, SHOP_CONVEYOR_CLEATS, SHOP_BRASS_LANTERN,
    SHOP_GEM_RESONANCE, SHOP_FEATHERSTEP, SHOP_EXTRA_HEARTH, SHOP_CHRONOMETER,
    /* Lantern Powers: bought with Relic Points, kept for the rest of the run. Order matches enum lm_powerup. */
    SHOP_SPRING_BOOTS, SHOP_LANTERN_SHIELD, SHOP_TEA_THERMOS, SHOP_STARLIGHT_MAGNET, SHOP_CLOCKWORK_SLOW,
    SHOP_EXTRA_LIFE,
    /* Special Wares: unlock at later shops */
    SHOP_DELVERS_CACHE, SHOP_PHOENIX_FEATHER,
    SHOP_COUNT
};
enum lm_shop_tier { TIER_SUPPLY, TIER_GEAR, TIER_MASTERWORK, TIER_POWER, TIER_SPECIAL };

enum lm_enemy_kind { EN_BEETLE, EN_BOUNCER, EN_GLOW_BAT, EN_PISTON, EN_WISP, EN_CHARGER, EN_SPITTER, EN_STALKER };

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
extern char const *const lm_seed_words[];
extern int const lm_seed_word_count;

/* ----------------------------------------------------------------- cavern */

enum lm_collect_kind { COL_KEY, COL_RELIC, COL_POWERUP };

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

struct lm_cavern {
    int depth;
    int biome;
    char name[96];
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
};

void lm_generate_cavern(char const *seed, int depth, struct lm_cavern *out);

/* ------------------------------------------------------------------ engine */

struct lm_player {
    double x, y, vx, vy, width, height;
    int facing;
    bool grounded;
    double coyote, jump_buffer;
    bool can_air_jump, has_used_double_jump;
    double invuln, squash_x, squash_y, walk_phase;
    int on_conveyor;
    bool has_shield;
    double spring, magnet, slow;
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
enum lm_event { EV_NONE = 0, EV_CAVERN_ADVANCED = 1, EV_OPEN_SHOP = 2, EV_GAME_OVER = 4 };

struct lm_run_summary {
    int depth_reached, caverns_cleared, score, high_score;
    bool new_high_score, new_best_depth;
    int relics, relic_points_left, keys;
    char seed[40];
    bool air; /* true = ran out of air, false = out of hearts */
};

struct lm_game {
    /* input */
    bool in_left, in_right, in_jump, prev_jump;
    /* run state */
    char seed[40];
    int depth, score, relic_points, high_score, best_depth, hearts, max_hearts;
    double air;
    int total_relics_run, total_keys_run, relics_this_cavern;
    int perks[PERK_COUNT];
    int powers[PU_COUNT]; /* Lantern Powers owned this run (Tea Thermos = number of flasks packed) */
    double power_time[PU_COUNT]; /* seconds left on the timed powers (Spring, Magnet, Slow); powers[] = stacks (the x2 / x3) */
    bool next_shield;
    int lives;           /* lives left, this one included */
    int run_start_high, run_start_depth; /* records when the run began, for the "new best" banners */
    bool phoenix;        /* Phoenix Feather packed: one free rising when the last life goes */
    double respawn_timer; /* >0 while the miner is down and about to restart the cavern */
    /* cavern */
    struct lm_cavern cavern;
    struct lm_player player;
    struct lm_enemy enemies[LM_MAX_ENEMIES];
    int n_enemies;
    struct lm_projectile proj[LM_MAX_PROJ];
    int n_proj;
    struct lm_crumble crumble[GRID_ROWS][GRID_COLS];
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
    struct { int depth_cleared, relics_in_cavern, total_score, air_bonus; } adv;
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
void lm_set_paused(struct lm_game *g, bool paused);
void lm_set_input(struct lm_game *g, bool left, bool right, bool jump);
void lm_step(struct lm_game *g, double dt);
void lm_leave_shop(struct lm_game *g);
/* Shop: status of an item for the current wallet. */
struct lm_shop_status { bool can_buy; char status[48]; char owned[32]; bool is_owned; };
void lm_shop_item_status(struct lm_game const *g, int item, struct lm_shop_status *out);
bool lm_shop_purchase(struct lm_game *g, int item);
int lm_cavern_until_shop(struct lm_game const *g);
double lm_rand(struct lm_game *g); /* Math.random() stand-in */
void lm_fmt_int(char *buf, size_t n, long v); /* 12,345 (toLocaleString) */

/* ------------------------------------------------------------------- audio */

void lm_audio_init(void);
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
/* World coordinates of the title-screen emblem etc. are drawn by the app. */

