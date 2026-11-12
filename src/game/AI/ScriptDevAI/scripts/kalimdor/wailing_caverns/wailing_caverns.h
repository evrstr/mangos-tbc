/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_WAILING_CAVERNS_H
#define DEF_WAILING_CAVERNS_H

/**
 * 哀嚎洞穴 (Wailing Caverns) 副本常量与事件枚举定义
 */

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER   = 6,

    TYPE_ANACONDRA  = 0,                                    // 首领遭遇/事件状态标识
    TYPE_COBRAHN    = 1,                                    // 首领遭遇/事件状态标识
    TYPE_PYTHAS     = 2,                                    // 首领遭遇/事件状态标识
    TYPE_SERPENTIS  = 3,                                    // 首领遭遇/事件状态标识
    TYPE_DISCIPLE   = 4,                                    // 首领遭遇/事件状态标识
    TYPE_MUTANUS    = 5,                                    // 首领遭遇/事件状态标识

    NPC_NARALEX     = 3679,                                 // NPC 条目 ID
    NPC_DISCIPLE    = 3678,                                 // NPC 条目 ID

    SAY_INTRO       = 2101,                             // Say when the first 4 encounter are DONE (说明)

    GO_MYSTERIOUS_CHEST     = 180055,                       // used for quest 7944; spawns in the instance only if one of the players has the quest (说明)

    QUEST_FORTUNE_AWAITS    = 7944,                         // 任务 ID
};

// instance_wailing_caverns 副本数据管理脚本
class instance_wailing_caverns : public ScriptedInstance
{
    public:
        instance_wailing_caverns(Map* pMap);
        ~instance_wailing_caverns() {}

        void Initialize() override;                         // 初始化副本数据与状态

        void OnPlayerEnter(Player* pPlayer) override;
        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建/加载至副本地图时的回调
        void OnObjectCreate(GameObject* pGo) override;      // 游戏对象创建/加载至副本地图时的回调

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置自定义状态数据
        uint32 GetData(uint32 uiType) const override;       // 获取自定义状态数据

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

        void DespawnAll(); // after naralex leaves (说明)

    protected:
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        GuidVector m_spawns;
};
#endif
