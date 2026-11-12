/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_BFD_H
#define DEF_BFD_H

/**
 * 黑暗深渊 (Blackfathom Deeps) 副本常量与波次召唤定义
 */

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER               = 3,
    MAX_FIRES                   = 4,
    MAX_COUNT_POS               = 3,

    TYPE_KELRIS                 = 1,                        // 首领遭遇/事件状态标识
    TYPE_SHRINE                 = 2,                        // 首领遭遇/事件状态标识
    TYPE_AQUANIS                = 3,                        // 首领遭遇/事件状态标识

    NPC_KELRIS                  = 4832,                     // NPC 条目 ID
    NPC_BARON_AQUANIS           = 12876,                    // NPC 条目 ID

    // Shrine event (祭坛事件)
    NPC_AKUMAI_SERVANT          = 4978,                     // NPC 条目 ID
    NPC_AKUMAI_SNAPJAW          = 4825,                     // NPC 条目 ID
    NPC_BARBED_CRUSTACEAN       = 4823,                     // NPC 条目 ID
    NPC_MURKSHALLOW_SOFTSHELL   = 4977,                     // NPC 条目 ID

    GO_PORTAL_DOOR              = 21117,                    // 游戏对象条目 ID
    GO_SHRINE_1                 = 21118,                    // 游戏对象条目 ID
    GO_SHRINE_2                 = 21119,                    // 游戏对象条目 ID
    GO_SHRINE_3                 = 21120,                    // 游戏对象条目 ID
    GO_SHRINE_4                 = 21121,                    // 游戏对象条目 ID
    GO_FATHOM_STONE             = 177964,                   // 游戏对象条目 ID
};

/* This is the spawn pattern for the event mobs
*     D
* 0        3
* 1   S    4
* 2        5
*     E

* This event spawns 4 sets of mobs
* The order in whitch the fires are lit doesn't matter

* First:    3 Snapjaws:     Positions 0, 1, 5
* Second:   2 Servants:     Positions 1, 4
* Third:    4 Crabs:        Positions 0, 2, 3, 4
* Fourth:  10 Murkshallows: Positions 2*0, 1, 2*2; 3, 2*4, 2*5

* On wipe the mobs don't despawn; they stay there until player returns
*/

static const Position aSpawnLocations[6] =                  // Should be near the correct positions (接近正确的刷新坐标)
{
    { -768.949f, -174.413f, -25.87f, 3.09f},                // Left side (左侧)
    { -768.888f, -164.238f, -25.87f, 3.09f},
    { -768.951f, -153.911f, -25.88f, 3.09f},
    { -867.782f, -174.352f, -25.87f, 6.27f},                // Right side (右侧)
    { -867.875f, -164.089f, -25.87f, 6.27f},
    { -867.859f, -153.927f, -25.88f, 6.27f}
};

struct PosCount
{
    uint8 m_uiCount, m_uiSummonPosition;
};

struct SummonInformation
{
    uint8 m_uiWaveIndex;
    uint32 m_uiNpcEntry;
    PosCount m_aCountAndPos[MAX_COUNT_POS];
};

// ASSERT m_uiSummonPosition < 6 (see aSpawnLocations) (说明)
static const SummonInformation aWaveSummonInformation[] =
{
    {0, NPC_AKUMAI_SNAPJAW,         {{1, 0}, {1, 1}, {1, 5}}},
    {1, NPC_AKUMAI_SERVANT,         {{1, 1}, {1, 4}, {0, 0}}},
    {2, NPC_BARBED_CRUSTACEAN,      {{1, 0}, {1, 2}, {0, 0}}},
    {2, NPC_BARBED_CRUSTACEAN,      {{1, 3}, {1, 4}, {0, 0}}},
    {3, NPC_MURKSHALLOW_SOFTSHELL,  {{2, 0}, {1, 1}, {2, 2}}},
    {3, NPC_MURKSHALLOW_SOFTSHELL,  {{1, 3}, {2, 4}, {2, 5}}}
};

static const float afAquanisPos[4] = { -782.21f, -63.26f, -42.43f, 2.36f };

// instance_blackfathom_deeps 副本数据管理脚本
class instance_blackfathom_deeps : public ScriptedInstance
{
    public:
        instance_blackfathom_deeps(Map* pMap);
        ~instance_blackfathom_deeps() {}

        void Initialize() override;                         // 初始化副本数据与状态

        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建/加载至副本地图时的回调
        void OnObjectCreate(GameObject* pGo) override;      // 游戏对象创建/加载至副本地图时的回调
        void OnCreatureDeath(Creature* pCreature) override;    // 生物在副本地图中死亡回调

        void Update(const uint32 diff) override;

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置自定义状态数据
        uint32 GetData(uint32 uiType) const override;       // 获取自定义状态数据

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

    protected:
        void DoSpawnMobs(uint8 uiWaveIndex);
        bool IsWaveEventFinished() const;

        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        uint32 m_uiSpawnMobsTimer[MAX_FIRES];
        uint8 m_uiWaveCounter;

        GuidList m_lWaveMobsGuids[MAX_FIRES];
};

#endif
