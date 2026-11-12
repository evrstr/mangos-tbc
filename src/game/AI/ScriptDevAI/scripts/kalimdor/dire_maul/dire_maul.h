/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_DM_H
#define DEF_DM_H

/**
 * 厄运之槌 (Dire Maul) 副本常量与事件状态定义
 */

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER               = 19,
    MAX_GENERATORS              = 5,
    MAX_PORTALS                 = 19,

    // East (东方)
    TYPE_ALZZIN                 = 0,                        // Do not change - Handled with Acid (说明)
    TYPE_ZEVRIM                 = 1,                        // 首领遭遇/事件状态标识
    TYPE_IRONBARK               = 2,                        // 首领遭遇/事件状态标识

    // West (西方)
    TYPE_WARPWOOD               = 3,                        // 首领遭遇/事件状态标识
    TYPE_IMMOLTHAR              = 4,                        // 首领遭遇/事件状态标识
    TYPE_PRINCE                 = 5,                        // 首领遭遇/事件状态标识
    TYPE_PYLON_1                = 6,                        // 首领遭遇/事件状态标识
    TYPE_PYLON_2                = TYPE_PYLON_1 + 1,         // 首领遭遇/事件状态标识
    TYPE_PYLON_3                = TYPE_PYLON_1 + 2,         // 首领遭遇/事件状态标识
    TYPE_PYLON_4                = TYPE_PYLON_1 + 3,         // 首领遭遇/事件状态标识
    TYPE_PYLON_5                = TYPE_PYLON_1 + 4,         // 首领遭遇/事件状态标识

    // North (北方)
    TYPE_KING_GORDOK            = 11,                       // 首领遭遇/事件状态标识
    TYPE_MOLDAR                 = 12,                       // 首领遭遇/事件状态标识
    TYPE_FENGUS                 = 13,                       // 首领遭遇/事件状态标识
    TYPE_SLIPKIK                = 14,                       // 首领遭遇/事件状态标识
    TYPE_KROMCRUSH              = 15,                       // 首领遭遇/事件状态标识
    TYPE_CHORUSH                = 16,                       // 首领遭遇/事件状态标识
    TYPE_STOMPER_KREEG          = 17,                       // 首领遭遇/事件状态标识

    // Other (说明)
    TYPE_DREADSTEED             = 18,                       // 首领遭遇/事件状态标识

    // East (东方)
    GO_CRUMBLE_WALL             = 177220,                   // 游戏对象条目 ID
    GO_CORRUPT_VINE             = 179502,                   // 游戏对象条目 ID
    GO_FELVINE_SHARD            = 179559,                   // 游戏对象条目 ID
    GO_CONSERVATORY_DOOR        = 176907,                   // Opened by Ironbark the redeemed (说明)

    NPC_ZEVRIM_THORNHOOF        = 11490,                    // NPC 条目 ID
    NPC_OLD_IRONBARK            = 11491,                    // NPC 条目 ID
    NPC_IRONBARK_REDEEMED       = 14241,                    // NPC 条目 ID

    // West (西方)
    NPC_TENDRIS_WARPWOOD        = 11489,                    // NPC 条目 ID
    NPC_PRINCE_TORTHELDRIN      = 11486,                    // NPC 条目 ID
    NPC_IMMOLTHAR               = 11496,                    // NPC 条目 ID
    NPC_ARCANE_ABERRATION       = 11480,                    // NPC 条目 ID
    NPC_MANA_REMNANT            = 11483,                    // NPC 条目 ID
    NPC_HIGHBORNE_SUMMONER      = 11466,                    // NPC 条目 ID
    NPC_WARLOCK_DUMMY_INFERNAL  = 14501,                    // NPC 条目 ID
    NPC_JEEVEE                  = 14500,                    // NPC 条目 ID

    GO_PRINCES_CHEST            = 179545,                   // 游戏对象条目 ID
    GO_PRINCES_CHEST_AURA       = 179563,                   // 游戏对象条目 ID
    GO_CRYSTAL_GENERATOR_1      = 177259,                   // 游戏对象条目 ID
    GO_CRYSTAL_GENERATOR_2      = 177257,                   // 游戏对象条目 ID
    GO_CRYSTAL_GENERATOR_3      = 177258,                   // 游戏对象条目 ID
    GO_CRYSTAL_GENERATOR_4      = 179504,                   // 游戏对象条目 ID
    GO_CRYSTAL_GENERATOR_5      = 179505,                   // 游戏对象条目 ID
    GO_FORCEFIELD               = 179503,                   // 游戏对象条目 ID
    GO_WARPWOOD_DOOR            = 177221,                   // 游戏对象条目 ID
    GO_WEST_LIBRARY_DOOR        = 179550,                   // 游戏对象条目 ID

    GO_BELL_OF_DETHMOORA        = 179674,                   // 游戏对象条目 ID
    GO_DOOMSDAY_CANDLE          = 179673,                   // 游戏对象条目 ID
    GO_WHEEL_OF_BLACK_MARCH     = 179672,                   // 游戏对象条目 ID
    GO_DREADSTEED_PORTAL        = 179681,                   // 游戏对象条目 ID
    GO_WARLOCK_RITUAL_CIRCLE    = 179668,                   // 游戏对象条目 ID
    GO_MOUNT_QUEST_SYMBOL1      = 179669,                   // 游戏对象条目 ID
    GO_MOUNT_QUEST_SYMBOL2      = 179670,                   // 游戏对象条目 ID
    GO_MOUNT_QUEST_SYMBOL3      = 179671,                   // 游戏对象条目 ID
    GO_RITUAL_CANDLE_AURA       = 179688, // invis trap - true caster of 23226 (说明)

    SPELL_BLACK_MARCH_BLESSING  = 23120,                    // 技能法术 ID
    SPELL_RITUAL_CANDLE_AURA    = 23226,                    // 技能法术 ID
    SPELL_RITUAL_BELL_AURA      = 23117,                    // 技能法术 ID

    SPELL_TELEPORT              = 29216,                    // 技能法术 ID

    NPC_XOROTHIAN_IMP           = 14482,                    // NPC 条目 ID
    NPC_DREAD_GUARD             = 14483,                    // NPC 条目 ID
    NPC_LORD_HELNURATH          = 14506,                    // NPC 条目 ID

    SAY_UNSUMMON_DEMON          = -1429005,                 // 剧情台词/喊话文本 ID

    // North (北方)
    NPC_GUARD_MOLDAR            = 14326,                    // NPC 条目 ID
    NPC_STOMPER_KREEG           = 14322,                    // NPC 条目 ID
    NPC_GUARD_FENGUS            = 14321,                    // NPC 条目 ID
    NPC_GUARD_SLIPKIK           = 14323,                    // NPC 条目 ID
    NPC_CAPTAIN_KROMCRUSH       = 14325,                    // NPC 条目 ID
    NPC_CHO_RUSH_THE_OBSERVER   = 14324,                    // NPC 条目 ID
    NPC_KING_GORDOK             = 11501,                    // NPC 条目 ID
    NPC_MIZZLE_THE_CRAFTY       = 14353,                    // NPC 条目 ID

    GO_KNOTS_CACHE              = 179501,                   // 游戏对象条目 ID
    GO_KNOTS_BALL_AND_CHAIN     = 179511,                   // 游戏对象条目 ID
    GO_GORDOK_TRIBUTE           = 179564,                   // 游戏对象条目 ID
    GO_NORTH_LIBRARY_DOOR       = 179549,                   // 游戏对象条目 ID

    SAY_FREE_IMMOLTHAR          = -1429000,                 // 剧情台词/喊话文本 ID
    SAY_KILL_IMMOLTHAR          = -1429001,                 // 剧情台词/喊话文本 ID
    SAY_IRONBARK_REDEEM         = -1429002,                 // 剧情台词/喊话文本 ID
    SAY_CHORUSH_KING_DEAD       = -1429003,                 // 剧情台词/喊话文本 ID
    SAY_SLIPKIK_TRAP            = -1429004,                 // 剧情台词/喊话文本 ID

    FACTION_HOSTILE             = 14,                       // 阵营 ID
    FACTION_FRIENDLY            = 35,                       // 阵营 ID
    FACTION_OGRE                = 45,                       // 阵营 ID

    SPELL_ICE_LOCK              = 22856,                    // 技能法术 ID
    SPELL_GUARD_SLIPKIK_TRIGGER = 31770,                    // 技能法术 ID
};

static const float afMizzleSpawnLoc[4] = {683.296f, 484.384f, 29.544f, 0.0174f};

// instance_dire_maul 副本数据管理脚本
class instance_dire_maul : public ScriptedInstance
{
    public:
        instance_dire_maul(Map* pMap);
        ~instance_dire_maul() {}

        void Initialize() override;                         // 初始化副本数据与状态

        void OnPlayerEnter(Player* pPlayer) override;

        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建/加载至副本地图时的回调
        void OnObjectCreate(GameObject* pGo) override;      // 游戏对象创建/加载至副本地图时的回调

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置自定义状态数据
        uint32 GetData(uint32 uiType) const override;       // 获取自定义状态数据

        void OnCreatureEnterCombat(Creature* pCreature) override;
        void OnCreatureDeath(Creature* pCreature) override;    // 生物在副本地图中死亡回调

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

        bool CheckConditionCriteriaMeet(Player const* pPlayer, uint32 uiInstanceConditionId, WorldObject const* pConditionSource, uint32 conditionSourceType) const override;

        void Update(const uint32 diff) override;
        GuidVector GetRitualSymbolGuids();
        void ProcessDreadsteedRitualStart();

    protected:
        bool CheckAllGeneratorsDestroyed();
        void ProcessForceFieldOpening();
        void SortPylonGuards();
        void PylonGuardJustDied(Creature* pCreature);

        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        // East (东方)
        bool m_bWallDestroyed;
        GuidList m_lFelvineShardGUIDs;

        // West (西方)
        ObjectGuid m_aCrystalGeneratorGuid[MAX_GENERATORS];

        GuidList m_luiHighborneSummonerGUIDs;
        GuidList m_lGeneratorGuardGUIDs;
        GuidList m_lDreadsteedPortalsGUIDs;
        GuidSet m_sSortedGeneratorGuards[MAX_GENERATORS];

        GuidVector m_lRitualSymbolGUIDs;

        // North (北方)
        bool m_bDoNorthBeforeWest;
};

#endif
