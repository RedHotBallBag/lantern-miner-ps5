/* Lantern Miner: catalogs (biomes, power-ups, perks, Old Barnaby's shop). Ported 1:1 from constants.ts. */
#include "lm.h"

#define Q "\xe2\x80\x99" /* right single quotation mark, as in the web version */

struct lm_biome const lm_biomes[BIOME_COUNT] = {
    [BIOME_TIMBER_HEARTH] = {
        "Timber Hearth", 0x16121b, 0x231b24, 0x3b3342, 0x625468, 0x231d29, 0xa76e42, 0x68422a, 0xf5a623, 0x22e4f5, 0xffd485,
        {"The Amber", "Warm Kettle", "Old Timber", "Hearthstone", "Cosy Lantern"},
        {"Gallery", "Drift", "Workings", "Landing", "Passage"}},
    [BIOME_AMETHYST_GROTTO] = {
        "Amethyst Grotto", 0x14111d, 0x231830, 0x3e3050, 0x6d5488, 0x241a31, 0x9c6654, 0x5d3b38, 0xff9f43, 0xf43fa5, 0xf8b4ff,
        {"Violet Quartz", "Velvet Crystal", "Eugene" Q "s", "Starlit", "Geode"},
        {"Grotto", "Parlour", "Chamber", "Sanctum", "Vault"}},
    [BIOME_CLOCKWORK_CELLAR] = {
        "Clockwork Cellar", 0x171416, 0x26201f, 0x493c37, 0x7c6556, 0x2a211e, 0xc48a3f, 0x754e24, 0xffb732, 0xffe347, 0xffe79e,
        {"Brass Cog", "Pendulum", "Central", "Processing", "Watchmaker" Q "s"},
        {"Cellar", "Cavern", "Workshop", "Foundry", "Conveyor"}},
    [BIOME_GLOW_MOSS] = {
        "Glow-Moss Hollow", 0x101819, 0x182827, 0x2f4544, 0x507570, 0x1b2b2b, 0x8d7348, 0x57462d, 0xf6b93b, 0x2ce87d, 0xa8ffcb,
        {"Emerald Fern", "Dewdrop", "Firefly", "Mossy Root", "Lantern-Moss"},
        {"Hollow", "Conservatory", "Grove", "Alcove", "Terrace"}},
    [BIOME_FROST_LANTERN] = {
        "Frost-Lantern Vault", 0x111622, 0x1b2639, 0x344761, 0x5d7ba1, 0x1e2a3c, 0x9c7858, 0x624a38, 0xf5a623, 0x22e4f5, 0xb5f3ff,
        {"The Cold", "Winter Tea", "Icicle", "Silver Frost", "Hearth-Frost"},
        {"Room", "Store", "Archway", "Ledge", "Gallery"}},
};

struct lm_powerup_def const lm_powerups[PU_COUNT] = {
    [PU_SPRING_BOOTS] = {"Spring Boots", "Spring", "Higher leaps and a mid-air spring jump (25s per timer).", 25, 0x2ce87d},
    [PU_LANTERN_SHIELD] = {"Lantern Shield", "Shield", "Warm golden ward absorbs the next hit and knocks foes back.", 0, 0xf5a623},
    [PU_TEA_THERMOS] = {"Tea Thermos", "Tea", "Restores +38% Lantern Air and awards +150 bonus points.", 0, 0xff9f43},
    [PU_STARLIGHT_MAGNET] = {"Starlight Magnet", "Magnet", "Pulls nearby keys and relics toward Willy (20s per timer).", 20, 0xffe347},
    [PU_CLOCKWORK_SLOW] = {"Clockwork Slow", "Slow", "Slows cavern guardians and conveyors to 75% speed (20s per timer).", 20, 0x22e4f5},
};

struct lm_perk_def const lm_perks[PERK_COUNT] = {
    [PERK_CRUMBLE_LIGHTFOOT] = {"Crumble Lightfoot", "Lightfoot", "Crumbling sandstone blocks hold steady 75% longer underfoot.", 0xff9f43, 3},
    [PERK_CONVEYOR_CLEATS] = {"Conveyor Cleats", "Cleats", "Move 12% brisker and resist 55% of Spectrum conveyor belt push.", 0x22e4f5, 2},
    [PERK_BRASS_LANTERN] = {"Big Brass Lantern", "Brass Lamp", "Slows Lantern Air drain by 22% and widens your warm headlamp glow.", 0xf5a623, 2},
    [PERK_GEM_RESONANCE] = {"Teacup Resonance", "Resonance", "Permanent mild treasure pull and +35% Relic Points & score.", 0xffe347, 2},
    [PERK_FEATHERSTEP] = {"Featherstep Slippers", "Featherstep", "Permanent mid-air flutter double-jump and extra ledge grace.", 0x2ce87d, 1},
    [PERK_EXTRA_HEARTH] = {"Heart of the Hearth", "+1 Hearth", "Gain +1 max Heart and restore all Hearts to full warmth.", 0xf43fa5, 2},
    [PERK_CHRONOMETER] = {"Master Chronometer", "Chronometer", "Permanently slows all cavern guardians & sparks by 20%.", 0x22e4f5, 1},
};

struct lm_shop_def const lm_shop[SHOP_COUNT] = {
    [SHOP_KETTLE_BREW] = {"Warm Kettle Brew", "+1 Heart", "Restores 1 missing Heart right away with spiced mine tea.", 300, TIER_SUPPLY, 0xf43fa5, 0, -1},
    [SHOP_LANTERN_WARD] = {"Lantern Ward Charm", "Next Shield", "Begin the next cavern wrapped in a protective Golden Shield.", 450, TIER_SUPPLY, 0xf5a623, 0, -1},
    [SHOP_CRUMBLE_LIGHTFOOT] = {"Crumble Lightfoot", "Lightfoot", "Crumbling ledges hold steady 75% longer under your boots.", 700, TIER_SUPPLY, 0xff9f43, 3, PERK_CRUMBLE_LIGHTFOOT},
    [SHOP_CONVEYOR_CLEATS] = {"Conveyor Cleats", "Cleats", "Run 12% brisker and resist 55% of fast conveyor belt push.", 1100, TIER_GEAR, 0x22e4f5, 2, PERK_CONVEYOR_CLEATS},
    [SHOP_BRASS_LANTERN] = {"Big Brass Lantern", "Brass Lamp", "Slows Lantern Air drain by 22% and brightens your headlamp beam.", 1400, TIER_GEAR, 0xf5a623, 2, PERK_BRASS_LANTERN},
    [SHOP_GEM_RESONANCE] = {"Teacup Resonance", "Resonance", "Permanent treasure magnet pull and +35% Relic Points & score.", 2400, TIER_GEAR, 0xffe347, 2, PERK_GEM_RESONANCE},
    [SHOP_FEATHERSTEP] = {"Featherstep Slippers", "Featherstep", "Permanent mid-air flutter double-jump and generous ledge coyote time.", 3200, TIER_MASTERWORK, 0x2ce87d, 1, PERK_FEATHERSTEP},
    [SHOP_EXTRA_HEARTH] = {"Heart of the Hearth", "+1 Hearth", "Permanently adds +1 Max Heart and restores all Hearts to full.", 4500, TIER_MASTERWORK, 0xf43fa5, 2, PERK_EXTRA_HEARTH},
    [SHOP_CHRONOMETER] = {"Master Chronometer", "Chronometer", "Permanently slows all guardians & sparks by 20%.", 6500, TIER_MASTERWORK, 0x22e4f5, 1, PERK_CHRONOMETER},
    [SHOP_SPRING_BOOTS] = {"Spring Boots", "Spring", "Higher leaps and a mid-air hop for 25s. Stack up to x3 for longer.", 1800, TIER_POWER, 0x2ce87d, 3, -1},
    [SHOP_LANTERN_SHIELD] = {"Lantern Shield", "Shield", "A golden ward that absorbs a hit. Renews for the next 3 caverns only.", 1200, TIER_POWER, 0xf5a623, 1, -1},
    [SHOP_TEA_THERMOS] = {"Tea Thermos", "Tea", "A flask that drinks itself when your Air runs out: +38% Air and +150 points.", 800, TIER_POWER, 0xff9f43, 3, -1},
    [SHOP_STARLIGHT_MAGNET] = {"Starlight Magnet", "Magnet", "Pulls nearby keys and relics to you for 20s. Stack up to x3 for longer.", 2600, TIER_POWER, 0xffe347, 3, -1},
    [SHOP_EXTRA_LIFE] = {"Spare Lantern", "+1 Life", "Adds a life. You have 5 to start and can carry up to 9.", 600, TIER_SUPPLY, 0xf43fa5, 0, -1},
    [SHOP_DELVERS_CACHE] = {"Delver\xe2\x80\x99s Cache", "Cache", "+2 Lives, full Hearts, and a fresh Tea flask. Unlocks at the 3rd stall.", 4000, TIER_SPECIAL, 0xf5a623, 0, -1},
    [SHOP_PHOENIX_FEATHER] = {"Phoenix Feather", "Phoenix", "When your last life goes you rise again with 3 lives. One per run. Unlocks at the 4th stall.", 9000, TIER_SPECIAL, 0xff6b3d, 1, -1},
    [SHOP_CONTINUE] = {"Continue Lantern", "Continue", "Out of lives? Carry on from the start of this world with 5 fresh ones. Hold up to 3.", 8000, TIER_SPECIAL, 0x2ce87d, 3, -1},
    [SHOP_CLOCKWORK_SLOW] = {"Clockwork Slow", "Slow", "Guardians and conveyors run at 75% speed for 20s. Stack up to x3.", 3000, TIER_POWER, 0x22e4f5, 3, -1},
};

