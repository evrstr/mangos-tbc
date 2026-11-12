/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 黑石塔（Blackrock Spire）副本全局定义头文件

#ifndef DEF_BLACKROCK_SPIRE_H                               // 防止头文件重复包含
#define DEF_BLACKROCK_SPIRE_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER               = 6,                        // encounter 标识 ID
    MAX_ROOMS                   = 7,                        // rooms 标识 ID

    TYPE_ROOM_EVENT             = 0,                        // room事件 遭遇战进度数据标识 ID
    TYPE_EMBERSEER              = 1,                        // 艾博希尔 遭遇战进度数据标识 ID
    TYPE_FLAMEWREATH            = 2,                        // Only summon once per instance (每个副本进度仅召唤一次)
    TYPE_STADIUM                = 3,                        // stadium 遭遇战进度数据标识 ID
    TYPE_DRAKKISATH             = 4,                        // drakkisath 遭遇战进度数据标识 ID
    TYPE_VALTHALAK              = 5,                        // Only summon once per instance (每个副本进度仅召唤一次)

    NPC_SCARSHIELD_INFILTRATOR  = 10299,                    // scarshieldinfiltrator Entry ID
    NPC_BLACKHAND_SUMMONER      = 9818,                     // 黑手summoner Entry ID
    NPC_BLACKHAND_VETERAN       = 9819,                     // 黑手veteran Entry ID
    NPC_PYROGUARD_EMBERSEER     = 9816,                     // pyroguard艾博希尔 Entry ID
    NPC_SOLAKAR_FLAMEWREATH     = 10264,                    // solakarflamewreath Entry ID
    NPC_BLACKHAND_INCARCERATOR  = 10316,                    // 黑手监禁者 Entry ID
    NPC_BLACKHAND_ELITE         = 10317,                    // 黑手elite Entry ID
    NPC_LORD_VICTOR_NEFARIUS    = 10162,                    // lordvictor奈法利奥斯 Entry ID
    NPC_REND_BLACKHAND          = 10429,                    // 雷德黑手 Entry ID
    NPC_GYTH                    = 10339,                    // 盖斯 Entry ID
    NPC_THE_BEAST               = 10430,                    // thebeast Entry ID
    NPC_DRAKKISATH              = 10363,                    // drakkisath Entry ID
    NPC_CHROMATIC_WHELP         = 10442,                    // related to Gyth arena event
    NPC_CHROMATIC_DRAGON        = 10447,                    // chromatic巨龙 Entry ID
    NPC_BLACKHAND_HANDLER       = 10742,                    // 黑手handler Entry ID

    // Doors
    GO_EMBERSEER_IN             = 175244,                   // 艾博希尔in 游戏对象 Entry ID
    GO_DOORS                    = 175705,                   // doors 游戏对象 Entry ID
    GO_EMBERSEER_OUT            = 175153,                   // 艾博希尔out 游戏对象 Entry ID
    GO_FATHER_FLAME             = 175245,                   // father烈焰 游戏对象 Entry ID
    GO_GYTH_ENTRY_DOOR          = 164726,                   // 盖斯entry门 游戏对象 Entry ID
    GO_GYTH_COMBAT_DOOR         = 175185,                   // 盖斯战斗门 游戏对象 Entry ID
    GO_GYTH_EXIT_DOOR           = 175186,                   // 盖斯exit门 游戏对象 Entry ID
    GO_DRAKKISATH_DOOR_1        = 175946,                   // drakkisath门1 游戏对象 Entry ID
    GO_DRAKKISATH_DOOR_2        = 175947,                   // drakkisath门2 游戏对象 Entry ID

    // upper spire entrance
    GO_DRAGONSPINE              = 164725,                   // dragonspine 游戏对象 Entry ID
    GO_BRAZIER_1                = 175528,                   // brazier1 游戏对象 Entry ID
    GO_BRAZIER_2                = 175529,                   // brazier2 游戏对象 Entry ID
    GO_BRAZIER_3                = 175530,                   // brazier3 游戏对象 Entry ID
    GO_BRAZIER_4                = 175531,                   // brazier4 游戏对象 Entry ID
    GO_BRAZIER_5                = 175532,                   // brazier5 游戏对象 Entry ID
    GO_BRAZIER_6                = 175533,                   // brazier6 游戏对象 Entry ID

    GO_ROOM_7_RUNE              = 175194,                   // room7rune 游戏对象 Entry ID
    GO_ROOM_3_RUNE              = 175195,                   // room3rune 游戏对象 Entry ID
    GO_ROOM_6_RUNE              = 175196,                   // room6rune 游戏对象 Entry ID
    GO_ROOM_1_RUNE              = 175197,                   // room1rune 游戏对象 Entry ID
    GO_ROOM_5_RUNE              = 175198,                   // room5rune 游戏对象 Entry ID
    GO_ROOM_2_RUNE              = 175199,                   // room2rune 游戏对象 Entry ID
    GO_ROOM_4_RUNE              = 175200,                   // room4rune 游戏对象 Entry ID

    GO_ROOKERY_EGG              = 175124,                   // rookery龙蛋 游戏对象 Entry ID

    GO_EMBERSEER_RUNE_1         = 175266,                   // 艾博希尔rune1 游戏对象 Entry ID
    GO_EMBERSEER_RUNE_2         = 175267,                   // 艾博希尔rune2 游戏对象 Entry ID
    GO_EMBERSEER_RUNE_3         = 175268,                   // 艾博希尔rune3 游戏对象 Entry ID
    GO_EMBERSEER_RUNE_4         = 175269,                   // 艾博希尔rune4 游戏对象 Entry ID
    GO_EMBERSEER_RUNE_5         = 175270,                   // 艾博希尔rune5 游戏对象 Entry ID
    GO_EMBERSEER_RUNE_6         = 175271,                   // 艾博希尔rune6 游戏对象 Entry ID
    GO_EMBERSEER_RUNE_7         = 175272,                   // 艾博希尔rune7 游戏对象 Entry ID

    ITEM_SEAL_OF_ASCENSION      = 12344,                    // sealofascension 物品 Entry ID

    MAX_STADIUM_WAVES           = 7,                        // stadiumwaves 标识 ID
    MAX_STADIUM_MOBS_PER_WAVE   = 5,                        // stadiummobsperwave 标识 ID

    FACTION_BLACK_DRAGON        = 103                       // black巨龙 阵营 ID
};

static const Position aStadiumLocs[7] =
{
    {210.00f, -420.30f, 110.94f, 3.14f},                    // dragons summon location
    {210.14f, -397.54f, 111.1f,  0.f},                      // Gyth summon location (盖斯召唤location)
    {163.62f, -420.33f, 110.47f, 0.f},                      // center of the stadium location (for movement)
    {164.63f, -444.04f, 121.97f, 3.22f},                    // Lord Nefarius summon position (Lord奈法利奥斯召唤position)
    {161.01f, -443.73f, 121.97f, 6.26f},                    // Rend summon position (雷德召唤position)
    {164.64f, -443.30f, 121.97f, 1.61f},                    // Nefarius move position
    {165.74f, -466.46f, 116.80f, 0.f},                      // Rend move position
};

static const Position aBeastLocs[3] =
{
    {98.09f, -563.45f, 109.86f, 2.37f},                     // Summon points (召唤 points)
    {99.81f, -561.47f, 109.24f, 2.43f},
    {96.92f, -560.98f, 110.18f, 2.43f},
};

static const uint32 aStadiumSpectators[12] =
{
    NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_ELITE, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN,
    NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_ELITE, NPC_BLACKHAND_VETERAN, NPC_BLACKHAND_VETERAN
};

static const Position aSpectatorsSpawnLocs[12] =
{
    {163.3209f, -340.9818f, 111.0216f, 4.818223f},
    {164.2471f, -339.0313f, 111.0368f, 1.413717f},
    {161.124f, -339.5178f, 111.0381f, 3.001966f},
    {162.5045f, -337.8101f, 111.0367f, 4.13643f},
    {160.9896f, -337.7715f, 111.0368f, 1.117011f},
    {161.8347f, -335.7923f, 111.0352f, 2.286381f},
    {113.9726f, -366.0805f, 116.9195f, 6.252025f},
    {112.7245f, -368.9635f, 116.9307f, 4.677482f},
    {110.5757f, -368.2123f, 116.9278f, 4.310963f},
    {109.3343f, -366.4785f, 116.9261f, 2.740167f},
    {110.1331f, -363.9824f, 116.9272f, 0.5235988f},
    {111.9971f, -363.0948f, 116.929f, 5.951573f},
};

static const Position aSpectatorsTargetLocs[12] =
{
    {160.619f, -395.826f, 121.9752f, -1.502597f},
    {162.1428f, -395.1175f, 121.9751f, -1.67753f},
    {158.6822f, -395.7097f, 121.9753f, -1.787977f},
    {164.384f, -395.3787f, 121.9751f, -1.502597f},
    {156.9669f, -395.2188f, 121.9752f, -1.678662f},
    {166.2515f, -395.0366f, 121.975f, -1.791467f},
    {143.814f, -396.7092f, 121.9753f, -1.40136f},
    {145.3893f, -396.1959f, 121.9752f, -1.419479f},
    {142.1598f, -396.0284f, 121.9752f, -1.661444f},
    {147.7274f, -396.3042f, 121.9753f, -1.40136f},
    {139.9446f, -396.7277f, 121.9753f, -1.428414f},
    {149.3754f, -395.7497f, 121.9753f, -1.714769f},
};

// Stadium event description
static const uint32 aStadiumEventNpcs[MAX_STADIUM_WAVES][5] =
{
    {NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, 0},
    {NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, 0},
    {NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER, 0},
    {NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER, 0},
    {NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER},
    {NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER},
    {NPC_CHROMATIC_WHELP, NPC_CHROMATIC_WHELP, NPC_CHROMATIC_DRAGON, NPC_CHROMATIC_DRAGON, NPC_BLACKHAND_HANDLER},
};

// 副本进度与状态机管理类：spire（instance_blackrock_spire）
class instance_blackrock_spire : public ScriptedInstance, private DialogueHelper
{
    public:
        instance_blackrock_spire(Map* pMap);
        ~instance_blackrock_spire() {}

        // 初始化副本数据结构
        void Initialize() override;

        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;

        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;
        void OnCreatureEvade(Creature* pCreature) override;
        void OnCreatureEnterCombat(Creature* pCreature) override;
        void OnCreatureDespawn(Creature* pCreature) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        void DoUseEmberseerRunes(bool bReset = false);
        void DoProcessEmberseerEvent();

        void DoOpenUpperDoorIfCan(Player* pPlayer);
        void DoSortRoomEventMobs();
        void GetIncarceratorGUIDList(GuidList& lList) const { lList = m_lIncarceratorGUIDList; }

        void StartflamewreathEventIfCan();

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        bool m_bBeastIntroDone;                             // BeastIntroDone 状态标记
        bool m_bBeastOutOfLair;                             // BeastOutOfLair 状态标记

    private:
        void JustDidDialogueStep(int32 iEntry) override;
        void DoSendNextStadiumWave();
        void DoSendNextFlamewreathWave();

        bool m_bUpperDoorOpened;                            // UpperDoorOpened 状态标记

        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        uint32 m_uiDragonspineDoorTimer;                    // DragonspineDoor 技能计时器
        uint32 m_uiDragonspineGoCount;
        uint32 m_uiFlamewreathEventTimer;                   // FlamewreathEvent 技能计时器
        uint32 m_uiFlamewreathWaveCount;
        uint32 m_uiStadiumEventTimer;                       // StadiumEvent 技能计时器
        uint8 m_uiStadiumWaves;
        uint8 m_uiStadiumMobsAlive;

        ObjectGuid m_aRoomRuneGuid[MAX_ROOMS];
        GuidList m_alRoomEventMobGUIDSorted[MAX_ROOMS];
        GuidList m_lRoomEventMobGUIDList;
        GuidList m_lIncarceratorGUIDList;
        GuidList m_lEmberseerRunesGUIDList;
        GuidList m_lStadiumSpectatorsGUIDList;
};

#endif                                                      // 结束头文件防重复包含宏
