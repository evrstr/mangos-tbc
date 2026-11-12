/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_ZULFARRAK_H
#define DEF_ZULFARRAK_H

/**
 * 祖尔法拉克 (Zul'Farrak) 副本常量与金字塔台阶进度定义
 */

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER                   = 9,

    TYPE_VELRATHA                   = 0,                    // 首领遭遇/事件状态标识
    TYPE_GAHZRILLA                  = 1,                    // 首领遭遇/事件状态标识
    TYPE_ANTUSUL                    = 2,                    // 首领遭遇/事件状态标识
    TYPE_THEKA                      = 3,                    // 首领遭遇/事件状态标识
    TYPE_ZUMRAH                     = 4,                    // 首领遭遇/事件状态标识
    TYPE_NEKRUM                     = 5,                    // 首领遭遇/事件状态标识
    TYPE_SEZZZIZ                    = 6,                    // 首领遭遇/事件状态标识
    TYPE_CHIEF_SANDSCALP            = 7,                    // 首领遭遇/事件状态标识
    TYPE_PYRAMID_EVENT              = 8,                    // 首领遭遇/事件状态标识

    NPC_VELRATHA                    = 7795,                 // NPC 条目 ID
    NPC_GAHZRILLA                   = 7273,                 // NPC 条目 ID
    NPC_ANTUSUL                     = 8127,                 // NPC 条目 ID
    NPC_THEKA                       = 7272,                 // NPC 条目 ID
    NPC_ZUMRAH                      = 7271,                 // NPC 条目 ID
    NPC_NEKRUM                      = 7796,                 // NPC 条目 ID
    NPC_SEZZZIZ                     = 7275,                 // NPC 条目 ID
    NPC_CHIEF_SANDSCALP             = 7267,                 // NPC 条目 ID

    NPC_SERGEANT_BLY                = 7604,                 // NPC 条目 ID
    NPC_SANDFURY_SLAVE              = 7787,                 // NPC 条目 ID
    NPC_SANDFURY_DRUDGE             = 7788,                 // NPC 条目 ID
    NPC_SANDFURY_CRETIN             = 7789,                 // NPC 条目 ID
    NPC_SANDFURY_ACOLYTE            = 8876,                 // NPC 条目 ID
    NPC_SANDFURY_ZEALOT             = 8877,                 // NPC 条目 ID

    GO_SHALLOW_GRAVE                = 128403,               // 游戏对象条目 ID
    GO_END_DOOR                     = 146084,               // 游戏对象条目 ID

    // EVENT_ID_GONG_ZULFARRAK      = 2488,                 // go 141832 (说明)
    // EVENT_ID_UNLOCKING           = 2609,                 // spell 10738 (法术)

    AREATRIGGER_ANTUSUL             = 1447,
};

// instance_zulfarrak 副本数据管理脚本
class instance_zulfarrak : public ScriptedInstance
{
    public:
        instance_zulfarrak(Map* pMap);
        ~instance_zulfarrak() {}

        void Initialize() override;                         // 初始化副本数据与状态

        void OnCreatureEnterCombat(Creature* pCreature) override;
        void OnCreatureEvade(Creature* pCreature) override;
        void OnCreatureDeath(Creature* pCreature) override;    // 生物在副本地图中死亡回调

        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建/加载至副本地图时的回调
        void OnObjectCreate(GameObject* pGo) override;      // 游戏对象创建/加载至副本地图时的回调

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置自定义状态数据
        uint32 GetData(uint32 uiType) const override;       // 获取自定义状态数据

        void GetShallowGravesGuidList(GuidList& lList) const { lList = m_lShallowGravesGuidList; }

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

        void Update(const uint32 diff) override;

    protected:
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        GuidList m_lShallowGravesGuidList;
        GuidList m_lPyramidTrollsGuidList;

        uint32 m_uiPyramidEventTimer;
};

#endif
