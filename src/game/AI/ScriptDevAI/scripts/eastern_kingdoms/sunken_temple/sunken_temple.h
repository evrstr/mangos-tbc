/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 阿塔哈卡神庙（Sunken Temple）全局定义头文件

#ifndef DEF_SUNKEN_TEMPLE_H                                 // 防止头文件重复包含
#define DEF_SUNKEN_TEMPLE_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER         = 5,                              // encounter 标识 ID
    MAX_STATUES           = 6,                              // statues 标识 ID
    MAX_FLAMES            = 4,                              // flames 标识 ID

    TYPE_ATALARION        = 0,                              // atalarion 遭遇战进度数据标识 ID
    TYPE_PROTECTORS       = 1,                              // protectors 遭遇战进度数据标识 ID
    TYPE_JAMMALAN         = 2,                              // jammalan 遭遇战进度数据标识 ID
    TYPE_MALFURION        = 3,                              // malfurion 遭遇战进度数据标识 ID
    TYPE_AVATAR           = 4,                              // avatar 遭遇战进度数据标识 ID

    NPC_ATALARION         = 8580,                           // atalarion Entry ID
    NPC_DREAMSCYTH        = 5721,                           // dreamscyth Entry ID
    NPC_WEAVER            = 5720,                           // weaver Entry ID
    NPC_JAMMALAN          = 5710,                           // jammalan Entry ID
    NPC_OGOM              = 5711,                           // ogom Entry ID
    NPC_AVATAR_OF_HAKKAR  = 8443,                           // avatarof哈卡 Entry ID
    NPC_SHADE_OF_ERANIKUS = 5709,                           // shadeoferanikus Entry ID

    // Jammalain mini-bosses
    NPC_ZOLO              = 5712,                           // zolo Entry ID
    NPC_GASHER            = 5713,                           // gasher Entry ID
    NPC_LORO              = 5714,                           // loro Entry ID
    NPC_HUKKU             = 5715,                           // hukku Entry ID
    NPC_ZULLOR            = 5716,                           // zullor Entry ID
    NPC_MIJAN             = 5717,                           // mijan Entry ID

    // Avatar of hakkar mobs
    NPC_SHADE_OF_HAKKAR   = 8440,                           // Shade of Hakkar appears when the event starts; will despawn when avatar of hakkar is summoned
    NPC_BLOODKEEPER       = 8438,                           // Spawned rarely and contains the hakkari blood -> used to extinguish the flames
    NPC_HAKKARI_MINION    = 8437,                           // Npc randomly spawned during the event = trash
    NPC_SUPPRESSOR        = 8497,                           // Npc summoned at one of the two doors and moves to the boss;

    NPC_MALFURION         = 15362,                          // malfurion Entry ID

    GO_ALTAR_OF_HAKKAR    = 148836,                         // Used in order to show the player the order of the statue activation
    GO_IDOL_OF_HAKKAR     = 148838,                         // Appears when atalarion is summoned; this was removed in 4.0.1

    GO_ATALAI_LIGHT       = 148883,                         // Green light, activates when the correct statue is chosen
    GO_ATALAI_LIGHT_BIG   = 148937,                         // Big light, used at the altar event

    GO_ATALAI_TRAP_1      = 177484,                         // Trapps triggered if the wrong statue is activated
    GO_ATALAI_TRAP_2      = 177485,                         // The traps are spawned in DB randomly around the statues (we don't know exactly which statue has which trap)
    GO_ATALAI_TRAP_3      = 148837,                         // atalaitrap3 游戏对象 Entry ID

    GO_ETERNAL_FLAME_1    = 148418,                         // eternal烈焰1 游戏对象 Entry ID
    GO_ETERNAL_FLAME_2    = 148419,                         // eternal烈焰2 游戏对象 Entry ID
    GO_ETERNAL_FLAME_3    = 148420,                         // eternal烈焰3 游戏对象 Entry ID
    GO_ETERNAL_FLAME_4    = 148421,                         // eternal烈焰4 游戏对象 Entry ID

    GO_EVIL_CIRCLE        = 148998,                         // Objects used at the avatar event. they are spawned when the event starts, and the mobs are summon atop of them
    GO_HAKKAR_DOOR_1      = 149432,                         // Combat doors (战斗doors)
    GO_HAKKAR_DOOR_2      = 149433,                         // 哈卡门2 游戏对象 Entry ID

    GO_JAMMALAN_BARRIER   = 149431,                         // jammalanbarrier 游戏对象 Entry ID

    // Event ids related to the statue activation
    EVENT_ID_STATUE_1     = 3094,                           // IDstatue1 事件标识 ID
    EVENT_ID_STATUE_2     = 3095,                           // IDstatue2 事件标识 ID
    EVENT_ID_STATUE_3     = 3097,                           // IDstatue3 事件标识 ID
    EVENT_ID_STATUE_4     = 3098,                           // IDstatue4 事件标识 ID
    EVENT_ID_STATUE_5     = 3099,                           // IDstatue5 事件标识 ID
    EVENT_ID_STATUE_6     = 3100,                           // IDstatue6 事件标识 ID

    SPELL_SUPPRESSION     = 12623,                          // suppression 法术 ID
    SPELL_SUMMON_AVATAR   = 12639,                          // Cast by the shade of hakkar, updates entry to avatar (由 the shade of hakkar, updates entry to avatar 施放)

    SPELL_SPIRIT_SPAWN_IN = 17321,                          // 之魂生成in 法术 ID

    SAY_JAMMALAN_INTRO    = -1109005,                       // jammalan开场 台词广播 ID
    SAY_AVATAR_BRAZIER_1  = -1109006,                       // avatarbrazier1 台词广播 ID
    SAY_AVATAR_BRAZIER_2  = -1109007,                       // avatarbrazier2 台词广播 ID
    SAY_AVATAR_BRAZIER_3  = -1109008,                       // avatarbrazier3 台词广播 ID
    SAY_AVATAR_BRAZIER_4  = -1109009,                       // avatarbrazier4 台词广播 ID
    SAY_AVATAR_SPAWN      = -1109010,                       // avatar生成 台词广播 ID
    SAY_SUPPRESSOR_1      = -1109011,                       // suppressor1 台词广播 ID
    SAY_SUPPRESSOR_2      = -1109012,                       // suppressor2 台词广播 ID
    SAY_SUPPRESSOR_3      = -1109013,                       // suppressor3 台词广播 ID
    SAY_SUPPRESSOR_4      = -1109014,                       // suppressor4 台词广播 ID
    SAY_AVATAR_DESPAWN    = -1109015,                       // avatar消除 台词广播 ID
};

// This is also the needed order for activation: S, N, SW, SE, NW, NE
static const uint32 m_aAtalaiStatueEvents[MAX_STATUES] = {EVENT_ID_STATUE_1, EVENT_ID_STATUE_2, EVENT_ID_STATUE_3, EVENT_ID_STATUE_4, EVENT_ID_STATUE_5, EVENT_ID_STATUE_6};

struct SummonLocations
{
    float m_fX, m_fY, m_fZ, m_fO;
};

static const SummonLocations aSunkenTempleLocation[] =
{
    { -466.5130f, 95.19820f, -189.646f, 0.0349f},           // Atalarion summon loc
    { -466.8673f, 272.31204f, -90.7441f, 3.5255f},          // Shade of hakkar summon loc
    { -660.5277f, -16.7117f, -90.8357f, 1.6055f}            // Malfurion summon loc
};

// Summon location for the suppressors (召唤 location for the suppressors)
static const SummonLocations aHakkariDoorLocations[2] =
{
    { -420.629f, 276.682f, -90.827f, 0.0f},
    { -512.015f, 276.134f, -90.827f, 0.0f}
};

// 副本进度与状态机管理类：temple（instance_sunken_temple）
class instance_sunken_temple : public ScriptedInstance
{
    public:
        instance_sunken_temple(Map* pMap);
        ~instance_sunken_temple() {}

        // 初始化副本数据结构
        void Initialize() override;

        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;

        void OnCreatureEvade(Creature* pCreature) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        bool ProcessStatueEvent(uint32 uiEventId);

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

    protected:
        void DoSpawnAtalarionIfCan();
        void DoUpdateFlamesFlags(bool bRestore);

        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        uint8 m_uiProtectorsRemaining;                      // Jammalan door handling
        uint8 m_uiStatueCounter;                            // Atalarion Statue Event
        uint8 m_uiFlameCounter;                             // Avatar of Hakkar Event (Avatarof哈卡事件)
        uint32 m_uiAvatarSummonTimer;                       // AvatarSummon 技能计时器
        uint32 m_uiSupressorTimer;                          // Supressor 技能计时器
        uint32 m_suppressionTimer;
        bool m_bIsFirstHakkarWave;                          // IsFirstHakkarWave 状态标记
        bool m_bCanSummonBloodkeeper;                       // CanSummonBloodkeeper 状态标记

        GuidList m_luiFlameGUIDs;
        GuidList m_luiBigLightGUIDs;
        GuidVector m_evilCircleGuidList;
};

#endif                                                      // 结束头文件防重复包含宏
