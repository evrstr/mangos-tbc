/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_RAMPARTS_H

// 地狱火城墙副本数据头文件
/* 遭遇战概述：
 * 定义地狱火城墙首领与门禁枚举。
 */
#define DEF_RAMPARTS_H

// 相关常量、法术与文本条目枚举
enum
{
    MAX_ENCOUNTER               = 2,

    TYPE_VAZRUDEN               = 1,
    TYPE_NAZAN                  = 2,                        // Do not change, used in ACID (SetData(SPECIAL) on death of 17517

    NPC_HELLFIRE_SENTRY         = 17517,
    NPC_VAZRUDEN_HERALD         = 17307,
    NPC_VAZRUDEN                = 17537,

    GO_FEL_IRON_CHEST           = 185168,
    GO_FEL_IRON_CHEST_H         = 185169,
};

// instance_ramparts 副本数据管理类
class instance_ramparts : public ScriptedInstance
{
    public:
        instance_ramparts(Map* pMap);

        // 初始化副本数据与状态
        void Initialize() override;

        // 副本生物创建登记
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本游戏对象创建登记
        void OnObjectCreate(GameObject* pGo) override;

        // 设置遭遇战状态数据
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战状态数据
        uint32 GetData(uint32 uiType) const override;

        // No need to save and load this instance (only one encounter needs special handling, no doors used)

    private:
        void DoFailVazruden();

        uint32 m_auiEncounter[MAX_ENCOUNTER];

        uint32 m_uiSentryCounter;
        std::vector<uint32> m_VazrudenPermanentGuids;
};

#endif
