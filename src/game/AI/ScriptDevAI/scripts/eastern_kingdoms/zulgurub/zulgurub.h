/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 祖尔格拉布（Zul'Gurub）全局定义头文件

#ifndef DEF_ZULGURUB_H                                      // 防止头文件重复包含
#define DEF_ZULGURUB_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER           = 8,                            // encounter 标识 ID
    MAX_PRIESTS             = 5,                            // priests 标识 ID

    TYPE_JEKLIK             = 0,                            // 耶克里克 遭遇战进度数据标识 ID
    TYPE_VENOXIS            = 1,                            // 温诺希斯 遭遇战进度数据标识 ID
    TYPE_MARLI              = 2,                            // 玛尔里 遭遇战进度数据标识 ID
    TYPE_THEKAL             = 3,                            // 塞卡尔 遭遇战进度数据标识 ID
    TYPE_ARLOKK             = 4,                            // 阿罗克 遭遇战进度数据标识 ID
    TYPE_OHGAN              = 5,                            // Do not change, used by Acid
    TYPE_LORKHAN            = 6,                            // lorkhan 遭遇战进度数据标识 ID
    TYPE_ZATH               = 7,                            // zath 遭遇战进度数据标识 ID

    NPC_LORKHAN             = 11347,                        // lorkhan Entry ID
    NPC_ZATH                = 11348,                        // zath Entry ID
    NPC_THEKAL              = 14509,                        // 塞卡尔 Entry ID
    NPC_JINDO               = 11380,                        // jindo Entry ID
    NPC_HAKKAR              = 14834,                        // 哈卡 Entry ID
    NPC_PANTHER_TRIGGER     = 15091,                        // panther触发 Entry ID
    NPC_BLOODLORD_MANDOKIR  = 11382,                        // bloodlord曼多基尔 Entry ID
    NPC_MARLI               = 14510,                        // 玛尔里 Entry ID

    GO_SPIDER_EGG           = 179985,                       // spider龙蛋 游戏对象 Entry ID
    GO_GONG_OF_BETHEKK      = 180526,                       // gongofbethekk 游戏对象 Entry ID
    GO_FORCEFIELD           = 180497,                       // forcefield 游戏对象 Entry ID

    SAY_MINION_DESTROY      = -1309022,                     // minion摧毁 台词广播 ID
    SAY_HAKKAR_PROTECT      = -1309023,                     // 哈卡protect 台词广播 ID

    AREATRIGGER_ENTER       = 3958,                         // enter 标识 ID
    AREATRIGGER_ALTAR       = 3960,                         // altar 标识 ID

    SPELL_HAKKAR_POWER      = 24692,                        // 哈卡power 法术 ID
    SPELL_HAKKAR_POWER_DOWN = 24693,                        // 哈卡powerdown 法术 ID
};

static const float aMandokirDownstairsPos[3] = { -12196.30f, -1948.37f, 130.31f};
static const float aArlokkWallShieldPos[3] = { -11494.76f, -1627.56f, 41.30f};

// 副本进度与状态机管理类：标识 ID（instance_zulgurub）
class instance_zulgurub : public ScriptedInstance
{
    public:
        instance_zulgurub(Map* map);
        ~instance_zulgurub() {}

        // 初始化副本数据结构
        void Initialize() override;
        // IsEncounterInProgress() const override { return false; }  // not active in Zul'Gurub

        void OnCreatureCreate(Creature* creature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* go) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 type, uint32 data) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 type) const override;

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        void DoYellAtTriggerIfCan(uint32 triggerId);

        Creature* SelectRandomPantherTrigger(bool bIsLeft);

    protected:
        void RemoveHakkarPowerStack();

        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        GuidList m_lRightPantherTriggerGUIDList;
        GuidList m_lLeftPantherTriggerGUIDList;
        GuidList m_lSpiderEggGUIDList;
        GuidList m_lProwlerGUIDList;

        bool m_bHasIntroYelled;                             // HasIntroYelled 状态标记
        bool m_bHasAltarYelled;                             // HasAltarYelled 状态标记
};

#endif                                                      // 结束头文件防重复包含宏
