/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_HYJAL_H
#define DEF_HYJAL_H

/**
 * 海加尔峰战役：副本常量、事件类型、实体条目定义
 */

#include "World/WorldStateDefines.h"
#include "Chat/Chat.h"
#include <array>

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER               = 5,

    TYPE_WINTERCHILL            = 0,                        // 首领遭遇/事件状态标识
    TYPE_ANETHERON              = 1,                        // 首领遭遇/事件状态标识
    TYPE_KAZROGAL               = 2,                        // 首领遭遇/事件状态标识
    TYPE_AZGALOR                = 3,                        // 首领遭遇/事件状态标识
    TYPE_ARCHIMONDE             = 4,                        // 首领遭遇/事件状态标识

    TYPE_WIN                    = 5,                        // 首领遭遇/事件状态标识

    // Alliance base (说明)
    NPC_JAINA                   = 17772,                    // NPC 条目 ID
    NPC_GUARDIAN_WATER_ELEMENTAL= 18001,                    // NPC 条目 ID
    NPC_ALLIANCE_PEASANT        = 17931,                    // NPC 条目 ID
    NPC_ALLIANCE_KNIGHT         = 17920,                    // NPC 条目 ID
    NPC_ALLIANCE_FOOTMAN        = 17919,                    // NPC 条目 ID
    NPC_ALLIANCE_RIFLEMAN       = 17921,                    // NPC 条目 ID
    NPC_ALLIANCE_PRIEST         = 17928,                    // NPC 条目 ID
    NPC_ALLIANCE_SORCERESS      = 17922,                    // NPC 条目 ID
    NPC_HYJAL_DESPAWN_TRIGGER   = 18410,                    // NPC 条目 ID
    NPC_BUILDING_TRIGGER        = 18304,                    // NPC 条目 ID

    // Horde base (说明)
    NPC_THRALL                  = 17852,                    // NPC 条目 ID
    NPC_DIRE_WOLF               = 17854,                    // NPC 条目 ID
    NPC_HORDE_HEADHUNTER        = 17934,                    // NPC 条目 ID
    NPC_HORDE_SHAMAN            = 17936,                    // NPC 条目 ID
    NPC_HORDE_GRUNT             = 17932,                    // NPC 条目 ID
    NPC_HORDE_HEALING_WARD      = 18036,                    // NPC 条目 ID
    NPC_TAUREN_WARRIOR          = 17933,                    // NPC 条目 ID
    NPC_HORDE_WITCH_DOCTOR      = 17935,                    // NPC 条目 ID
    NPC_HORDE_PEON              = 17937,                    // NPC 条目 ID
    NPC_INFERNAL_RELAY          = 18242,                    // NPC 条目 ID
    NPC_INFERNAL_TARGET         = 21075,                    // NPC 条目 ID

    // Elf base (说明)
    NPC_TYRANDE                 = 17948,                    // NPC 条目 ID
    NPC_NELF_WISP               = 18502,                    // NPC 条目 ID
    NPC_DRUID_OF_THE_TALON      = 3794,                     // NPC 条目 ID
    NPC_DRUID_OF_THE_CLAW       = 3795,                     // NPC 条目 ID
    NPC_NELF_ANCIENT_PROT       = 18487,                    // NPC 条目 ID
    NPC_NELF_ANCIENT_OF_LORE    = 18486,                    // NPC 条目 ID
    NPC_NELF_ANCIENT_OF_WAR     = 18485,                    // NPC 条目 ID
    NPC_NELF_ARCHER             = 17943,                    // NPC 条目 ID
    NPC_NELF_HUNTRESS           = 17945,                    // NPC 条目 ID
    NPC_DRYAD                   = 17944,                    // NPC 条目 ID

    // Bosses summoned after every 8 waves (说明)
    NPC_WINTERCHILL             = 17767,                    // NPC 条目 ID
    NPC_ANETHERON               = 17808,                    // NPC 条目 ID
    NPC_KAZROGAL                = 17888,                    // NPC 条目 ID
    NPC_AZGALOR                 = 17842,                    // NPC 条目 ID
    NPC_ARCHIMONDE              = 17968,                    // NPC 条目 ID

    // Trash Mobs summoned in waves (说明)
    NPC_NECRO                   = 17899,                    // NPC 条目 ID
    NPC_ABOMI                   = 17898,                    // NPC 条目 ID
    NPC_GHOUL                   = 17895,                    // NPC 条目 ID
    NPC_BANSH                   = 17905,                    // NPC 条目 ID
    NPC_CRYPT                   = 17897,                    // NPC 条目 ID
    NPC_GARGO                   = 17906,                    // NPC 条目 ID
    NPC_FROST                   = 17907,                    // NPC 条目 ID
    NPC_INFERNAL                = 17908,                    // NPC 条目 ID
    NPC_STALK                   = 17916,                    // NPC 条目 ID

    // Final Archimonde event (事件)
    NPC_ANCIENT_WISP            = 17946,                    // NPC 条目 ID

    // Anetheron infernal (说明)
    NPC_TOWERING_INFERNAL       = 17818,                    // NPC 条目 ID
    // Azgalor Doomguard (说明)
    NPC_LESSER_DOOMGUARD        = 17864,                    // NPC 条目 ID

    GO_ANCIENT_GEM              = 185557,                   // 游戏对象条目 ID
    GO_HORDE_ENCAMPMENT_PORTAL  = 182060,                   // 游戏对象条目 ID
    GO_NIGHT_ELF_VILLAGE_PORTAL = 182061,                   // 游戏对象条目 ID
    GO_ROARING_FLAME            = 182260,                   // 游戏对象条目 ID

    MAX_WAVES                   = 36,
    MAX_INVASION_WAVES          = 17,
    MAX_CONCURRENT_SPAWNS       = 30,

    SPELL_SUMMON_INFERNAL       = 32148,                    // 技能法术 ID

    SPELL_SOUL_CHARGE_YELLOW_CHARGE = 32045,                // 技能法术：冲锋
    SPELL_SOUL_CHARGE_GREEN_CHARGE  = 32051,                // 技能法术：冲锋
    SPELL_SOUL_CHARGE_RED_CHARGE    = 32052,                // 技能法术：冲锋

    SPELL_CHOOSE_TARGET_UNUSED      = 32121, // Currently unused. Unknown what its purpose is, but it's cast by Druid of the Claw c.3795

    // Used for Building creature (Base Invasion/Overrun events) (说明)
    SPELL_SUPER_INVIS           = 8149,                     // 技能法术 ID
    FACTION_SPAR                = 1692,                     // 阵营 ID
    FACTION_SPAR_BUDDY          = 1693,                     // 阵营 ID
};

// BaseArea 枚举定义
enum BaseArea
{
    BASE_ALLY  = 0,
    BASE_HORDE = 1,
    BASE_ELF   = 2,
    MAX_BASE   = 3,
};

// WavePaths 枚举定义
enum WavePaths
{
    PATH_NONE           = 0, // For stationary temp-spawned triggers (说明)
    ALLY_GROUND         = 1, // Normal ground mobs in Alliance Base (说明)
    HORDE_GROUND        = 2, // Normal ground mobs in Horde Base (说明)
    HORDE_GARGOYLE_1    = 3, // Gargoyles spawned behind the mountains close to the troll camp (说明)
    HORDE_GARGOYLE_2    = 4, // Gargoyles spawned behind the mountains close to the gate (说明)
    HORDE_FROST_WYRM_1  = 5, // Frost Wyrm spawned behind the mountains close to the troll camp (说明)
    HORDE_FROST_WYRM_2  = 6, // Frost Wyrm spawned behind the mountains close to the gate (说明)
    NELF_GROUND         = 7, // Waves spawned during Archimonde fight (说明)
};

// HyjalEvents 事件类型枚举
enum HyjalEvents
{
    JAINA_FIRST_BOSS,
    JAINA_SECOND_BOSS,
    JAINA_WIN,
    THRALL_FIRST_BOSS,
    THRALL_SECOND_BOSS,
    THRALL_WIN,
};

struct HyjalWaveMob
{
    uint32 mobEntry;
    float x, y, z, ori;
    WavePaths path;
};

struct HyjalWave
{
    std::vector<HyjalWaveMob> waveMobs;
    uint32 waveTimer;                                     // The timer before the next wave is summoned (说明)
};

struct InvasionWave
{
    std::vector<HyjalWaveMob> waveMobs;
    uint32 invasionTimer;                                 // The timer before the next wave is summoned (说明)
};


// instance_mount_hyjal 副本数据管理脚本
class instance_mount_hyjal : public ScriptedInstance
{
    friend class ChatHandler;
    public:
        instance_mount_hyjal(Map* pMap);

        void Initialize() override;                         // 初始化副本数据与状态
        bool IsEncounterInProgress() const override;        // 检查是否有首领战正在进行中

        void OnPlayerEnter(Player* pPlayer) override;
        void OnPlayerDeath(Player* player) override;

        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建/加载至副本地图时的回调
        void OnObjectCreate(GameObject* pGo) override;      // 游戏对象创建/加载至副本地图时的回调
        void OnCreatureRespawn(Creature* creature) override;

        void OnCreatureEnterCombat(Creature* pCreature) override;
        // void OnCreatureEvade(Creature* pCreature) override; (说明)
        void OnCreatureDeath(Creature* pCreature) override;    // 生物在副本地图中死亡回调

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置自定义状态数据
        uint32 GetData(uint32 uiType) const override;       // 获取自定义状态数据

        bool CheckConditionCriteriaMeet(Player const* pPlayer, uint32 uiInstanceConditionId, WorldObject const* pConditionSource, uint32 conditionSourceType) const override;

        const char* Save() const override { return m_saveData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

        void Update(const uint32 diff) override;

        void StartEvent(HyjalEvents eventId);
        void FailEvent();
        GuidVector GetOverrunSpawns(uint32 baseId) { return m_overrunSpawns[baseId]; }

        void ShowChatCommands(ChatHandler* handler) override;
        void ExecuteChatCommand(ChatHandler* handler, char* args) override;

    private:
        void SpawnNextWave();
        void SpawnNextInvasionWave();
        void SpawnWave(uint32 index, bool setTimer);
        void SpawnWaveInfernals(uint32 index);
        void SpawnInvasionInfernals(uint32 index);
        void SpawnInfernals(uint32 count);
        void SpawnInfernal();
        void SpawnOres(BaseArea index);
        void SpawnInvasionWave(uint32 index, bool setTimer);
        void SpawnArchimonde();
        void DespawnWaveSpawns();
        void DespawnBase(BaseArea index);
        void SpawnBase(BaseArea index, bool spawnLeader);
        void RetreatBase(BaseArea index);
        void OverrunBase(BaseArea index);
        void DespawnOverrun(BaseArea index);
        void RestartEvent();

        uint32 m_encounter[MAX_ENCOUNTER];
        std::string m_saveData;

        GuidVector m_ancientGems[MAX_BASE - 1];
        GuidVector m_roaringFlames[MAX_BASE - 1];
        GuidVector m_waveSpawns;
        GuidVector m_baseSpawns[MAX_BASE];
        GuidVector m_overrunSpawns[MAX_BASE];

        GuidVector m_additionalSpawns;

        uint32 m_hyjalWaves; // Script value (说明)
        uint32 m_invasionWaves;

        uint32 m_nextWaveTimer;
        uint32 m_nextInvasionWaveTimer;
        uint32 m_retreatTimer;
        uint32 m_infernalsTimer;
        uint32 m_infernalsCounter;

        uint32 m_failTimer;

        bool m_startCheck;

        static std::array<HyjalWave, MAX_WAVES> m_hyjalWavesData;
        static std::array<InvasionWave, MAX_INVASION_WAVES> m_invasionWavesData;

        // infernals data holders (说明)
        GuidVector m_infernalRelays;
        GuidVector m_infernalTargets;
        std::vector<uint32> m_infernalIndices;
};

#endif
