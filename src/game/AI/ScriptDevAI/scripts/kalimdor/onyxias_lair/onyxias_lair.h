/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_ONYXIA_H
#define DEF_ONYXIA_H

// 奥妮克希亚巢穴副本数据常量枚举
enum
{
    TYPE_ONYXIA                 = 0,                        // 奥妮克希亚遭遇战状态标识

    // Special data fields for Onyxia (奥妮克希亚特殊状态标识)
    DATA_LIFTOFF                = 4,                        // 升空起飞状态（进入第二阶段）

    NPC_ONYXIA_WHELP            = 11262,                    // 奥妮克希亚幼龙
    NPC_ONYXIA_TRIGGER          = 12758,                    // 奥妮克希亚触发器 NPC
    NPC_ONYXIAN_WARDER          = 12129,                    // 奥妮克希亚守卫

    GO_WHELP_SPAWNER            = 176510,                   // 幼龙刷新点游戏对象
};

// 奥妮克希亚巢穴副本数据管理类
class instance_onyxias_lair : public ScriptedInstance
{
    public:
        instance_onyxias_lair(Map* pMap);                   // 构造函数
        ~instance_onyxias_lair() {}

        void Initialize() override;                         // 初始化副本状态与计时器

        bool IsEncounterInProgress() const override;        // 检查战斗是否正在进行（含起飞阶段）

        void OnCreatureCreate(Creature* pCreature) override; // 生物加载回调（缓存触发器 GUID）
        void OnCreatureDeath(Creature* pCreature) override; // 生物死亡回调（记录守卫 GUID）

        void OnObjectCreate(GameObject* pGo) override;      // 游戏对象加载回调（幼龙刷新器触发召唤）

        void SetData(uint32 uiType, uint32 uiData) override; // 设置副本遭遇状态数据

    protected:
        uint32 m_uiEncounter;                               // 首领遭遇战当前状态

        GuidList m_lWarderGUIDList;                         // 奥妮克希亚守卫 GUID 列表
        time_t m_tPhaseTwoStart;                            // 第二阶段（起飞）开始时间戳
};

#endif
