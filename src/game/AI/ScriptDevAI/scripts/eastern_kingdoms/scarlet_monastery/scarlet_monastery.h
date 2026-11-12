/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 血色修道院（Scarlet Monastery）全局定义头文件

#ifndef DEF_SCARLETM_H                                      // 防止头文件重复包含
#define DEF_SCARLETM_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER                   = 2,                    // encounter 标识 ID

    TYPE_MOGRAINE_AND_WHITE_EVENT   = 1,                    // 莫格莱尼andwhite事件 遭遇战进度数据标识 ID
    TYPE_ASHBRINGER_EVENT           = 2,                    // ashbringer事件 遭遇战进度数据标识 ID

    NPC_MOGRAINE                    = 3976,                 // 莫格莱尼 Entry ID
    NPC_WHITEMANE                   = 3977,                 // 怀特迈恩 Entry ID
    NPC_VORREL                      = 3981,                 // vorrel Entry ID
    NPC_INTERROGATOR_VISHAS         = 3983,                 // 审讯官vishas Entry ID

    GO_WHITEMANE_DOOR               = 104600,               // 怀特迈恩门 游戏对象 Entry ID
    GO_CHAPEL_DOOR                  = 104591,               // chapel门 游戏对象 Entry ID

    SAY_TRIGGER_VORREL              = -1189015,             // 触发vorrel 台词广播 ID

    ITEM_CORRUPTED_ASHRBRINGER      = 22691,                // corruptedashrbringer 物品 Entry ID
    SAY_ASHBRINGER_ENTRANCE         = -1189036,             // ashbringerentrance 台词广播 ID
    AREATRIGGER_CATHEDRAL_ENTRANCE  = 4089, // used to trigger Corrupted Ashbringer event
    // these npcs are tracked for purposes of turning them friendly during the Corrupted Ashbringer event
    NPC_SORCERER    = 4294,                                 // 巫师 Entry ID
    NPC_MYRMIDON    = 4295,                                 // myrmidon Entry ID
    NPC_DEFENDER    = 4298,                                 // 防御者 Entry ID
    NPC_CHAPLAIN    = 4299,                                 // chaplain Entry ID
    NPC_WIZARD      = 4300,                                 // wizard Entry ID
    NPC_CENTURION   = 4301,                                 // centurion Entry ID
    NPC_CHAMPION    = 4302,                                 // 勇士 Entry ID
    NPC_ABBOT       = 4303,                                 // abbot Entry ID
    NPC_MONK        = 4540,                                 // monk Entry ID
    NPC_FAIRBANKS   = 4542,                                 // fairbanks Entry ID

    NPC_HEADLESS_HORSEMAN_EARTHQUAKE_BUNNY = 23758,         // 无头骑士earthquakebunny Entry ID
};

// 副本进度与状态机管理类：monastery（instance_scarlet_monastery）
class instance_scarlet_monastery : public ScriptedInstance
{
    public:
        instance_scarlet_monastery(Map* pMap);

        // 初始化副本数据结构
        void Initialize() override;

        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        void OnCreatureRespawn(Creature* creature) override;
        void OnObjectSpawn(GameObject* go) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiData) const override;

        bool DoHandleAreaTrigger(AreaTriggerEntry const* areaTrigger);

    private:
        GuidSet m_sAshbringerFriendlyGuids;                 // sAshbringerFriendlyGuids GUID 集合
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
};

#endif                                                      // 结束头文件防重复包含宏
