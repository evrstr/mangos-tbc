
#ifndef DEF_WORLD_MAP_KALIMDOR_SCRIPTS_H
#define DEF_WORLD_MAP_KALIMDOR_SCRIPTS_H

/**
 * 卡利姆多 (Kalimdor) 全局通用常量与实体定义
 */

// 常量与事件枚举
enum
{
    // TYPE_SHADE_OF_HEADLESS_HORSEMAN = 1000, // High number against collisions (说明)

    // Quest 4740 (任务 4740)
    NPC_GREYMIST_COASTRUNNNER   = 2202,                     // NPC 条目 ID
    NPC_GREYMIST_WARRIOR        = 2205,                     // NPC 条目 ID
    NPC_GREYMIST_HUNTER         = 2206,                     // NPC 条目 ID
    NPC_MURKDEEP                = 10323,                    // NPC 条目 ID
    QUEST_WANTED_MURKDEEP       = 4740,                     // 任务 ID

    // Quest 6134 (任务 6134)
    GO_GHOST_MAGNET             = 177746,                   // 游戏对象条目 ID
    NPC_MAGRAMI_SPECTRE         = 11560,                    // NPC 条目 ID
    // SPELL_SPIRIT_SPAWN_OUT      = 17680, (说明)

    // Quest 8868 (任务 8868)
    NPC_MINION_OMEN             = 15466,                    // NPC 条目 ID
    NPC_OMEN                    = 15467,                    // NPC 条目 ID
    GO_ROCKET_CLUSTER           = 180875,                   // 游戏对象条目 ID
    SPELL_OMEN_MOONLIGHT        = 26392,                    // 技能法术 ID
    MAX_ROCKETS                 = 30,

    // Quest 6132 (任务 6132)
    NPC_HORNIZZ                 = 6019,                     // NPC 条目 ID

    // War Effort (说明)
    NPC_COLOSSUS_OF_ZORA        = 15740,                    // NPC 条目 ID
    NPC_COLOSSUS_OF_REGAL       = 15741,                    // NPC 条目 ID
    NPC_COLOSSUS_OF_ASHI        = 15742,                    // NPC 条目 ID

    NPC_COLOSSUS_RESEARCHER_SOPHIA = 15797,                 // NPC 条目 ID
    NPC_COLOSSUS_RESEARCHER_NESTOR = 15798,                 // NPC 条目 ID
    NPC_COLOSSUS_RESEARCHER_EAZEL  = 15799,                 // NPC 条目 ID
};

// Encounters 枚举定义
enum Encounters
{
    TYPE_OMEN               = 0, // Quest 8868 (任务 8868)
    TYPE_TETHYR             = 2, // Quest 11198 - tbc+ (任务)
    TYPE_FREEDOM_CREATURES  = 3, // Quest 2969 (任务 2969)
    TYPE_GONG_TIME          = 4, // War Effort (说明)
    MAX_ENCOUNTER
};

// SpawnIndexes 枚举定义
enum SpawnIndexes
{
    POS_IDX_MURKDEEP_SPAWN      = 0,
    POS_IDX_MURKDEEP_MOVE       = 1,
    POS_IDX_OMEN_SPAWN          = 2,
    POS_IDX_OMEN_MOVE           = 3,
    POS_IDX_MINION_OMEN_START   = 4,
    POS_IDX_MINION_OMEN_STOP    = 10,
    POS_IDX_HIVE_DRONES_START   = 11,
    POS_IDX_HIVE_DRONES_STOP    = 13,
    POS_IDX_MAX
};

static const float aSpawnLocations[POS_IDX_MAX][4] =
{
    {4981.031f,    597.955f,   -1.361f, 4.82f},         // Murkdeep spawn, guesswork (刷新)
    {4988.970f,    547.002f,    5.379f,  0.0f},         // Murkdeep move, guesswork (说明)
    {7564.098f,  -2835.687f,  447.374f, 3.98f},         // Omen spawn, guesswork (刷新)
    {7528.430f,  -2889.838f,  459.365f,  0.0f},         // Omen move, guesswork (说明)
    {7669.430f,  -2869.980f,  464.904f, 2.60f},         // Minions of Omen (说明)
    {7621.790f,  -2861.740f,  459.043f, 4.15f},
    {7597.130f,  -2885.880f,  464.898f, 2.88f},
    {7594.170f,  -2856.490f,  459.403f, 3.17f},
    {7477.750f,  -2849.020f,  464.466f, 0.14f},
    {7507.560f,  -2818.230f,  459.486f, 4.63f},
    {7558.880f,  -2856.770f,  457.684f, 4.31f},
    {-7185.94f,     443.29f,  26.59f, 4.8458f},         // Hive'Ashi Drones spawn 1, guesswork
    {-7180.59f,     441.33f,  26.68f, 4.1798f},         // Hive'Ashi Drones spawn 2, guesswork
    {-7176.63f,     437.42f,  26.84f, 3.9348f},         // Hive'Ashi Drones spawn 3, guesswork
};

#endif