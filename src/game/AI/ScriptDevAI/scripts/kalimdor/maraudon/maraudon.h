/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_MAR_H
#define DEF_MAR_H

/**
 * 玛拉顿 (Maraudon) 副本常量与数据定义
 */

// 常量与事件枚举
/**
 * 玛拉顿 (Maraudon) 副本常量与数据定义
 */

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER               = 1,

    TYPE_NOXXION                = 0,                        // 遭遇/状态标识

    NPC_NOXXION                 = 13282,                    // NPC 条目 ID
    NPC_SPEWED_LARVA            = 13533,                    // NPC 条目 ID

    GO_LARVA_SPEWER             = 178559,                   // 游戏对象条目 ID
    GO_CORRUPTION_SPEWER        = 178570,                   // 游戏对象条目 ID
};

static const Position spawnLocation = {937.213f, -377.967f, -50.346f, 2.578f};

// instance_maraudon 副本数据管理脚本
class instance_maraudon : public ScriptedInstance
{
    public:
        instance_maraudon(Map* map);
        ~instance_maraudon() {}

        void Initialize() override;                         // 初始化副本数据与状态

        void OnObjectCreate(GameObject* go) override;       // 游戏对象创建/加载至副本地图时的回调
        void OnCreatureDeath(Creature* creature) override;    // 生物在副本地图中死亡回调
        void OnCreatureEvade(Creature* creature) override;

        void SetData(uint32 type, uint32 data) override;    // 设置自定义状态数据
        uint32 GetData(uint32 type) const override;         // 获取自定义状态数据

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

        void Update(const uint32 diff) override;

    protected:
        uint32 m_encounter[MAX_ENCOUNTER];
        uint32 m_spewLarvaTimer;
        std::string m_strInstData;
};


#endif
