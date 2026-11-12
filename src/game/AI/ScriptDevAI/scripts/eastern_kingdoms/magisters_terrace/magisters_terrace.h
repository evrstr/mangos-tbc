/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 魔导师平台（Magisters' Terrace）副本全局定义头文件

#ifndef DEF_MAGISTERS_TERRACE_H                             // 防止头文件重复包含
#define DEF_MAGISTERS_TERRACE_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER               = 4,                        // encounter 标识 ID
    MAX_DELRISSA_ADDS           = 4,                        // 德莉希亚adds 标识 ID

    TYPE_SELIN                  = 0,                        // 塞林 遭遇战进度数据标识 ID
    TYPE_VEXALLUS               = 1,                        // 维萨鲁斯 遭遇战进度数据标识 ID
    TYPE_DELRISSA               = 2,                        // 德莉希亚 遭遇战进度数据标识 ID
    TYPE_KAELTHAS               = 3,                        // 凯尔萨斯 遭遇战进度数据标识 ID

    NPC_SELIN_FIREHEART         = 24723,                    // 塞林fireheart Entry ID
    NPC_DELRISSA                = 24560,                    // 德莉希亚 Entry ID
    NPC_FEL_CRYSTAL             = 24722,                    // 魔能crystal Entry ID
    NPC_KALECGOS_DRAGON         = 24844,                    // 卡雷苟斯巨龙 Entry ID
    NPC_KAELTHAS                = 24664,                    // 凯尔萨斯 Entry ID

    NPC_SCRYERS_BUNNY           = 25042,                    // scryersbunny Entry ID

    // Delrissa adds (德莉希亚adds)
    NPC_KAGANI                  = 24557,                    // kagani Entry ID
    NPC_ELLRYS                  = 24558,                    // ellrys Entry ID
    NPC_ERAMAS                  = 24554,                    // eramas Entry ID
    NPC_YAZZAI                  = 24561,                    // yazzai Entry ID
    NPC_SALARIS                 = 24559,                    // salaris Entry ID
    NPC_GARAXXAS                = 24555,                    // garaxxas Entry ID
    NPC_APOKO                   = 24553,                    // apoko Entry ID
    NPC_ZELFAN                  = 24556,                    // zelfan Entry ID
    NPC_SLIVER                  = 24552,                    // sliver Entry ID

    GO_VEXALLUS_DOOR            = 187896,                   // 维萨鲁斯门 游戏对象 Entry ID
    GO_SELIN_DOOR               = 187979,                   // SunwellRaid Gate 02
    GO_DELRISSA_DOOR            = 187770,                   // 德莉希亚门 游戏对象 Entry ID
    GO_SELIN_ENCOUNTER_DOOR     = 188065,                   // Assembly Chamber Door

    GO_KAEL_DOOR                = 188064,                   // kael门 游戏对象 Entry ID
    // GO_KAEL_STATUE_LEFT       = 188165,                  // animation statues - they do not reset on fail
    // GO_KAEL_STATUE_RIGHT      = 188166,
    GO_ESCAPE_QUEL_DANAS        = 188173,                   // escapequeldanas 游戏对象 Entry ID

    SPELL_FEL_CRYSTAL_VISUAL    = 44355,                    // 魔能crystal视觉特效 法术 ID
};

static const int32 aDelrissaAddDeath[MAX_DELRISSA_ADDS] = { -1585013, -1585014, -1585015, -1585016};

// 副本进度与状态机管理类：terrace（instance_magisters_terrace）
class instance_magisters_terrace : public ScriptedInstance
{
    public:
        instance_magisters_terrace(Map* pMap);

        // 初始化副本数据结构
        void Initialize() override;

        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;

        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;
        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;

        void StartCrystalVisual();

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        bool IsKalecgosOrbHandled() const { return m_kalecgosOrbHandled; }
        void SetKalecgosOrbHandled() { m_kalecgosOrbHandled = true; }

    private:
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        GuidList m_lFelCrystalGuid;

        bool m_kalecgosOrbHandled;
};

#endif                                                      // 结束头文件防重复包含宏
