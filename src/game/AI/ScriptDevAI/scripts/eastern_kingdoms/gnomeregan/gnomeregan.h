/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 诺莫瑞根（Gnomeregan）副本全局定义头文件

#ifndef DEF_GNOMEREGAN_H                                    // 防止头文件重复包含
#define DEF_GNOMEREGAN_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER               = 2,                        // Only Grubbis and Thermaplugg need treatment
    MAX_GNOME_FACES             = 6,                        // gnomefaces 标识 ID
    MAX_EXPLOSIVES_PER_SIDE     = 2,                        // explosivesperside 标识 ID

    TYPE_GRUBBIS                = 1,                        // grubbis 遭遇战进度数据标识 ID
    TYPE_THERMAPLUGG            = 2,                        // 瑟玛普拉格 遭遇战进度数据标识 ID
    TYPE_EXPLOSIVE_CHARGE       = 3,                        // explosive冲锋 遭遇战进度数据标识 ID

    DATA_EXPLOSIVE_CHARGE_1     = 1,                        // explosive冲锋1 遭遇战进度数据标识 ID
    DATA_EXPLOSIVE_CHARGE_2     = 2,                        // explosive冲锋2 遭遇战进度数据标识 ID
    DATA_EXPLOSIVE_CHARGE_3     = 3,                        // explosive冲锋3 遭遇战进度数据标识 ID
    DATA_EXPLOSIVE_CHARGE_4     = 4,                        // explosive冲锋4 遭遇战进度数据标识 ID
    DATA_EXPLOSIVE_CHARGE_USE   = 5,                        // explosive冲锋use 遭遇战进度数据标识 ID

    NPC_BLASTMASTER_SHORTFUSE   = 7998,                     // blastmastershortfuse Entry ID

    GO_RED_ROCKET               = 103820,                   // red火箭 游戏对象 Entry ID
    GO_CAVE_IN_NORTH            = 146085,                   // caveinnorth 游戏对象 Entry ID
    GO_CAVE_IN_SOUTH            = 146086,                   // caveinsouth 游戏对象 Entry ID
    GO_EXPLOSIVE_CHARGE         = 144065,                   // explosive冲锋 游戏对象 Entry ID
    GO_THE_FINAL_CHAMBER        = 142207,                   // thefinalchamber 游戏对象 Entry ID

    GO_GNOME_FACE_1             = 142211,                   // gnomeface1 游戏对象 Entry ID
    GO_GNOME_FACE_2             = 142210,                   // gnomeface2 游戏对象 Entry ID
    GO_GNOME_FACE_3             = 142209,                   // gnomeface3 游戏对象 Entry ID
    GO_GNOME_FACE_4             = 142208,                   // gnomeface4 游戏对象 Entry ID
    GO_GNOME_FACE_5             = 142213,                   // gnomeface5 游戏对象 Entry ID
    GO_GNOME_FACE_6             = 142212,                   // gnomeface6 游戏对象 Entry ID

    GO_BUTTON_1                 = 142214,                   // 按钮1 游戏对象 Entry ID
    GO_BUTTON_2                 = 142215,                   // 按钮2 游戏对象 Entry ID
    GO_BUTTON_3                 = 142216,                   // 按钮3 游戏对象 Entry ID
    GO_BUTTON_4                 = 142217,                   // 按钮4 游戏对象 Entry ID
    GO_BUTTON_5                 = 142218,                   // 按钮5 游戏对象 Entry ID
    GO_BUTTON_6                 = 142219                    // 按钮6 游戏对象 Entry ID
};

struct sBombFace
{
    ObjectGuid m_gnomeFaceGuid;                             // gnomeFace 实体 GUID
    bool m_bActivated;                                      // Activated 状态标记
    uint32 m_uiBombTimer;                                   // Bomb 技能计时器
};

// 副本进度与状态机管理类：标识 ID（instance_gnomeregan）
class instance_gnomeregan : public ScriptedInstance
{
    public:
        instance_gnomeregan(Map* pMap);
        ~instance_gnomeregan() {}

        // 初始化副本数据结构
        void Initialize() override;

        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        sBombFace* GetBombFaces();
        void DoActivateBombFace(uint8 uiIndex);
        void DoDeactivateBombFace(uint8 uiIndex);

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

    protected:
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        sBombFace m_asBombFaces[MAX_GNOME_FACES];
        ObjectGuid m_aExplosiveSortedGuids[2][MAX_EXPLOSIVES_PER_SIDE];

        GuidList m_luiExplosiveChargeGUIDs;
        GuidList m_luiSpawnedExplosiveChargeGUIDs;
        GuidList m_lRedRocketGUIDs;
};

#endif                                                      // 结束头文件防重复包含宏
