/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_RFD_H
#define DEF_RFD_H

/**
 * 剃刀高地 (Razorfen Downs) 副本常量与数据定义
 */

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER               = 1,
    MAX_WAVES                   = 3,
    MAX_COUNT_POS               = 2,

    TYPE_TUTEN_KASH             = 0,                        // 首领遭遇/事件状态标识

    NPC_TOMB_FIEND              = 7349,                     // NPC 条目 ID
    NPC_TOMB_REAVER             = 7351,                     // NPC 条目 ID
    NPC_TUTEN_KASH              = 7355,                     // NPC 条目 ID

    GO_GONG                     = 148917,                   // 游戏对象条目 ID
};

static const Position aSpawnLocations[MAX_COUNT_POS] =
{
    {2484.83f, 811.11f, 43.40f, 1.67f},                 // Right corridor (说明)
    {2546.03f, 902.77f, 47.16f, 5.04f},                 // Left corridor (说明)
};

struct RFDSummonInformation
{
    uint32 m_uiNpcEntry;
    uint8 m_uiNPCperWave;
};

static const RFDSummonInformation aWaveSummonInformation[] =
{
    {NPC_TOMB_FIEND,  8},
    {NPC_TOMB_REAVER, 4},
    {NPC_TUTEN_KASH,  1}
};

// instance_razorfen_downs 副本数据管理脚本
class instance_razorfen_downs : public ScriptedInstance
{
    public:
        instance_razorfen_downs(Map* pMap);
        ~instance_razorfen_downs() {}

        void Initialize() override;                         // 初始化副本数据与状态

        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建/加载至副本地图时的回调
        void OnObjectCreate(GameObject* pGo) override;      // 游戏对象创建/加载至副本地图时的回调
        void OnCreatureDeath(Creature* pCreature) override;    // 生物在副本地图中死亡回调

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置自定义状态数据
        uint32 GetData(uint32 uiType) const override;       // 获取自定义状态数据

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

        void DoSpawnWaveIfCan(GameObject* pGo);

    protected:
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        uint8 m_uiWaveCounter;

        GuidList m_lSpawnedMobsList;
};

#endif
