/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_GRUULS_LAIR_H
#define DEF_GRUULS_LAIR_H

// 格鲁尔的巢穴遭遇战与对象枚举
enum
{
    MAX_ENCOUNTER                   = 2,                            // 首领遭遇战总数
    MAX_COUNCIL                     = 5,                            // 莫加尔议会成员数

    // Encounter Status (遭遇战状态)
    TYPE_MAULGAR_EVENT              = 0,                            // 莫加尔大王事件
    TYPE_GRUUL_EVENT                = 1,                            // 屠龙者格鲁尔事件

    GO_PORT_GRONN_1                 = 183817,               // 184468 not in use (戈隆大门 1)
    GO_PORT_GRONN_2                 = 184662,                       // 戈隆大门 2

    // NPC GUIDs (关键生物 Entry)
    NPC_MAULGAR                     = 18831,                        // 莫加尔大王
    // NPC_BLINDEYE                  = 18836,                       // 盲眼先知
    // NPC_KIGGLER                   = 18835,                       // 基格勒
    // NPC_KROSH                     = 18832,                       // 克洛什·火怒
    // NPC_OLM                       = 18834,                       // 召魔者奥鲁姆
};

// 格鲁尔的巢穴副本数据管理类
class instance_gruuls_lair : public ScriptedInstance
{
    public:
        instance_gruuls_lair(Map* pMap);

        void Initialize() override;                                 // 初始化副本数据
        bool IsEncounterInProgress() const override;                // 检查是否有首领遭遇战进行中

        void OnCreatureCreate(Creature* pCreature) override;        // 生物创建登记
        void OnObjectCreate(GameObject* pGo) override;              // 游戏对象创建登记

        void SetData(uint32 uiType, uint32 uiData) override;        // 设置遭遇战状态数据
        uint32 GetData(uint32 uiType) const override;               // 获取遭遇战状态数据

        const char* Save() const override { return m_strSaveData.c_str(); } // 存盘数据字符串
        void Load(const char* chrIn) override;                      // 读取副本存盘数据

    private:
        uint32 m_auiEncounter[MAX_ENCOUNTER];                       // 遭遇战状态数组
        std::string m_strSaveData;                                  // 存盘字符串
};

#endif
