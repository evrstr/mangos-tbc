/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_RFK_H
#define DEF_RFK_H

/**
 * 剃刀沼泽 (Razorfen Kraul) 副本常量与数据定义
 */

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER     = 1,

    TYPE_AGATHELOS    = 1,                                  // 首领遭遇/事件状态标识

    GO_AGATHELOS_WARD = 21099,                              // 游戏对象条目 ID

    NPC_WARD_KEEPER   = 4625
};

// instance_razorfen_kraul 副本数据管理脚本
class instance_razorfen_kraul : public ScriptedInstance
{
    public:
        instance_razorfen_kraul(Map* pMap);
        ~instance_razorfen_kraul() {}

        void Initialize() override;                         // 初始化副本数据与状态

        void OnObjectCreate(GameObject* pGo) override;      // 游戏对象创建/加载至副本地图时的回调
        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建/加载至副本地图时的回调

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置自定义状态数据
        uint32 GetData(uint32 uiType) const override;       // 获取自定义状态数据

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

    protected:
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        uint8 m_uiWardKeepersRemaining;
};
#endif
