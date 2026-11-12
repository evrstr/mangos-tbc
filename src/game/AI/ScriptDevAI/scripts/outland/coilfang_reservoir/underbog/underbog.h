/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
* This program is free software licensed under GPL version 2
* Please see the included DOCS/LICENSE.TXT for more information */

#ifndef UNDERBOG_H

// 幽暗沼泽副本数据头文件
/* 遭遇战概述：
 * 定义幽暗沼泽首领与门禁枚举。
 */
#define UNDERBOG_H

// 相关常量、法术与文本条目枚举
enum
{
    NPC_SWAMPLORD_MUSELEK = 17826,
    NPC_CLAW              = 17827,
    NPC_CLAW_DRUID_FORM   = 17894,
    NPC_GHAZAN            = 18105,
    NPC_BLACK_STALKER     = 17882,

    NPC_UNDERBOG_FRENZY   = 20465,
    NPC_EARTHBINDER_RAYGE = 17885,

    SAY_RAYGE_WHISPER = 14618,

    SPELL_PERMANENT_FEIGH_DEATH = 31261,

    QUEST_LOST_IN_ACTION = 9738,

    DATA_GHAZAN_SURFACE   = 0,
    DATA_EARTHBINDER_RAYGE_WHISPER = 1,
};

// instance_underbog 副本数据管理类
class instance_underbog : public ScriptedInstance
{
    public:
        instance_underbog(Map* map);
        // 初始化副本数据与状态
        void Initialize() override;

        // 设置遭遇战状态数据
        void SetData(uint32 type, uint32 data) override;
        // 获取遭遇战状态数据
        uint32 GetData(uint32 type) const override;

        // 副本生物创建登记
        void OnCreatureCreate(Creature* creature) override;
        // 副本生物死亡处理
        void OnCreatureDeath(Creature* creature) override;

    private:
        bool m_bGhazanSurfaced;
        bool m_raygeWhispered;
};

#endif
