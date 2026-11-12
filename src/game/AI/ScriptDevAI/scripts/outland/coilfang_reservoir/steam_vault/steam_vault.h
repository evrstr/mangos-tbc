/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_STEAM_VAULT_H

// 蒸汽地窟副本数据头文件
/* 遭遇战概述：
 * 定义蒸汽地窟首领与水闸门禁枚举。
 */
#define DEF_STEAM_VAULT_H

// 相关常量、法术与文本条目枚举
enum
{
    MAX_ENCOUNTER                   = 3,

    TYPE_HYDROMANCER_THESPIA        = 0,
    TYPE_MEKGINEER_STEAMRIGGER      = 1,
    TYPE_WARLORD_KALITHRESH         = 2,

    NPC_NAGA_DISTILLER              = 17954,
    NPC_STEAMRIGGER                 = 17796,
    NPC_KALITHRESH                  = 17798,
    // NPC_THESPIA                   = 17797,

    GO_MAIN_CHAMBERS_DOOR           = 183049,
    GO_ACCESS_PANEL_HYDRO           = 184125,
    GO_ACCESS_PANEL_MEK             = 184126,
};

// instance_steam_vault 副本数据管理类
class instance_steam_vault : public ScriptedInstance
{
    public:
        instance_steam_vault(Map* pMap);

        // 初始化副本数据与状态
        void Initialize() override;

        // 副本生物创建登记
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本游戏对象创建登记
        void OnObjectCreate(GameObject* pGo) override;

        // 副本生物死亡处理
        void OnCreatureDeath(Creature* pCreature) override;

        // 设置遭遇战状态数据
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战状态数据
        uint32 GetData(uint32 uiType) const override;

        const char* Save() const override { return m_strInstData.c_str(); }
        void Load(const char* chrIn) override;

    private:
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        GuidList m_lNagaDistillerGuidList;
};

#endif
