/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 黑翼之巢（Blackwing Lair）副本全局定义头文件

#ifndef DEF_BLACKWING_LAIR                                  // 防止头文件重复包含
#define DEF_BLACKWING_LAIR

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER               = 13,                       // encounter 标识 ID

    TYPE_RAZORGORE              = 0,                        // 拉佐格尔 遭遇战进度数据标识 ID
    TYPE_VAELASTRASZ            = 1,                        // 瓦拉斯塔兹 遭遇战进度数据标识 ID
    TYPE_LASHLAYER              = 2,                        // 勒西什尔 遭遇战进度数据标识 ID
    TYPE_FIREMAW                = 3,                        // 费尔默 遭遇战进度数据标识 ID
    TYPE_EBONROC                = 4,                        // 埃博诺克 遭遇战进度数据标识 ID
    TYPE_FLAMEGOR               = 5,                        // 弗莱格尔 遭遇战进度数据标识 ID
    TYPE_CHROMAGGUS             = 6,                        // 克洛玛古斯 遭遇战进度数据标识 ID
    TYPE_NEFARIAN               = 7,                        // 奈法利安 遭遇战进度数据标识 ID
    TYPE_QUEST_SCEPTER          = 8,                        // 任务scepter 遭遇战进度数据标识 ID
    TYPE_CHROMA_LBREATH         = 9,                        // Not real events: used to store the two random Chromaggus breaths for the instance lifetime
    TYPE_CHROMA_RBREATH         = 10,                       // chromarbreath 遭遇战进度数据标识 ID
    TYPE_NEFA_LTUNNEL           = 11,                       // Not real events: used to store the two random drakonids selection for the instance lifetime
    TYPE_NEFA_RTUNNEL           = 12,                       // nefartunnel 遭遇战进度数据标识 ID

    DATA_DRAGON_EGG             = 1,                        // track the used eggs

    NPC_RAZORGORE               = 12435,                    // 拉佐格尔 Entry ID
    NPC_VAELASTRASZ             = 13020,                    // 瓦拉斯塔兹 Entry ID
    NPC_LASHLAYER               = 12017,                    // 勒西什尔 Entry ID
    NPC_FIREMAW                 = 11983,                    // 费尔默 Entry ID
    NPC_EBONROC                 = 14601,                    // 埃博诺克 Entry ID
    NPC_FLAMEGOR                = 11981,                    // 弗莱格尔 Entry ID
    NPC_CHROMAGGUS              = 14020,                    // 克洛玛古斯 Entry ID
    NPC_NEFARIAN                = 11583,                    // 奈法利安 Entry ID
    NPC_LORD_VICTOR_NEFARIUS    = 10162,                    // lordvictor奈法利奥斯 Entry ID

    // Razorgore event related (拉佐格尔事件related)
    NPC_GRETHOK_CONTROLLER      = 12557,                    // grethokcontroller Entry ID
    NPC_BLACKWING_GUARDSMAN     = 14456,                    // blackwingguardsman Entry ID
    NPC_BLACKWING_ORB_TRIGGER   = 14449,                    // blackwingorb触发 Entry ID
    NPC_NEFARIANS_TROOPS        = 14459,                    // nefarianstroops Entry ID
    NPC_MONSTER_GENERATOR       = 12434,                    // monstergenerator Entry ID
    NPC_ORB_DOMINATION          = 14453,                    // orbdomination Entry ID
    NPC_BLACKWING_LEGIONNAIRE   = 12416,                    // one spawn per turn (每轮生成一只)
    NPC_BLACKWING_MAGE          = 12420,                    // one spawn per turn (每轮生成一只)
    NPC_DRAGONSPAWN             = 12422,                    // two spawns per turn

    GO_DOOR_RAZORGORE_ENTER     = 176964,                   // 门拉佐格尔enter 游戏对象 Entry ID
    GO_DOOR_RAZORGORE_EXIT      = 176965,                   // 门拉佐格尔exit 游戏对象 Entry ID
    GO_DOOR_NEFARIAN            = 176966,                   // 门奈法利安 游戏对象 Entry ID
    // GO_DOOR_CHROMAGGUS_ENTER  = 179115,
    // GO_DOOR_CHROMAGGUS_SIDE   = 179116,
    GO_DOOR_CHROMAGGUS_EXIT     = 179117,                   // 门克洛玛古斯exit 游戏对象 Entry ID
    GO_DOOR_VAELASTRASZ         = 179364,                   // 门瓦拉斯塔兹 游戏对象 Entry ID
    GO_DOOR_LASHLAYER           = 179365,                   // 门勒西什尔 游戏对象 Entry ID
    GO_ORB_OF_DOMINATION        = 177808,                   // trigger 19832 on Razorgore (触发19832on拉佐格尔)
    GO_BLACK_DRAGON_EGG         = 177807,                   // black巨龙龙蛋 游戏对象 Entry ID
    GO_DRAKONID_BONES           = 179804,                   // drakonidbones 游戏对象 Entry ID
    GO_SUPPRESSION_DEVICE       = 179784,                   // Traps in Suppression Room

    EMOTE_ORB_SHUT_OFF          = -1469035,                 // orbshutoff 表情动作 ID
    EMOTE_TROOPS_FLEE           = -1469033,                 // emote by Nefarian's Troops npc
    SAY_RAZORGORE_DEATH         = -1469025,                 // 拉佐格尔死亡 台词广播 ID

    // Spells used by the monster generator in Razorgore encounter
    // SPELL_SUMMON_LEGIONNAIRES   = 19824,                    // Periodically triggers 19826
    SPELL_SUMMON_LEGIONNAIRE    = 19826,                    // 召唤legionnaire 法术 ID
    SPELL_SUMMON_MAGE           = 19827,                    // 召唤法师 法术 ID
    SPELL_SUMMON_DRAGONSPAWN    = 19828,                    // 召唤dragonspawn 法术 ID
    SPELL_WARMING_FLAMES        = 23040,                    // Used by Razorgore to fully heal in in phase 1 to 2 transition
    SPELL_CONTROL_ORB           = 23018,                    // Visual used by Grethok the Controller
    SPELL_FIREBALL              = 23024,                    // Used by Razorgore to reset the event (and kill everyone like a badass)
    SPELL_EXPLODE_ORB           = 20037,                    // used if attacked without destroying the eggs - triggers 20038
    SPELL_POSSESS               = 19832,                    // possess 法术 ID

    MAX_EGGS_DEFENDERS          = 4,                        // eggsdefenders 标识 ID
    MAX_DRAGONSPAWN             = 12,                       // dragonspawn 标识 ID
    MAX_BLACKWING_DEFENDER      = 40,                       // blackwing防御者 标识 ID

    // Nefarian encounter (奈法利安遭遇战)
    NPC_BLACK_SPAWNER           = 14307,                    // blackspawner Entry ID
    NPC_RED_SPAWNER             = 14309,                    // redspawner Entry ID
    NPC_GREEN_SPAWNER           = 14310,                    // greenspawner Entry ID
    NPC_BRONZE_SPAWNER          = 14311,                    // bronzespawner Entry ID
    NPC_BLUE_SPAWNER            = 14312,                    // bluespawner Entry ID

    NPC_BLUE_DRAKONID           = 14261,                    // bluedrakonid Entry ID
    NPC_GREEN_DRAKONID          = 14262,                    // greendrakonid Entry ID
    NPC_BRONZE_DRAKONID         = 14263,                    // bronzedrakonid Entry ID
    NPC_RED_DRAKONID            = 14264,                    // reddrakonid Entry ID
    NPC_BLACK_DRAKONID          = 14265,                    // blackdrakonid Entry ID
    NPC_CHROMATIC_DRAKONID      = 14302,                    // chromaticdrakonid Entry ID

    MAX_DRAKONID_SUMMONS        = 42,                       // drakonidsummons 标识 ID

    SPELL_SHADOWBLINK_OUTRO     = 22681,                    // shadowblink尾声 法术 ID

    // Yells by Nefarian during the epic quest Nefarius' Corruption
    YELL_REDSHARD_TAUNT_1       = -1469036,                 // redshard嘲讽1 大喊台词广播 ID
    YELL_REDSHARD_TAUNT_2       = -1469037,                 // redshard嘲讽2 大喊台词广播 ID
    YELL_REDSHARD_TAUNT_3       = -1469039,                 // redshard嘲讽3 大喊台词广播 ID
    YELL_REDSHARD_TAUNT_4       = -1469040,                 // redshard嘲讽4 大喊台词广播 ID
    YELL_REDSHARD_TAUNT_5       = -1469041,                 // redshard嘲讽5 大喊台词广播 ID
    YELL_REDSHARD_TAUNT_6       = -1469042,                 // redshard嘲讽6 大喊台词广播 ID
    YELL_REDSHARD_TAUNT_7       = -1469043,                 // redshard嘲讽7 大喊台词广播 ID
    EMOTE_REDSHARD_TAUNT_1      = -1469038,                 // redshard嘲讽1 表情动作 ID
    EMOTE_REDSHARD_TAUNT_2      = -1469044,                 // redshard嘲讽2 表情动作 ID
};

static const Position aNefarianLocs[3] =
{
    { -7599.32f,  -1191.72f,  475.545f, 0.0},                      // opening where red/blue/black darknid spawner appear (ori 3.05433)
    { -7526.27f,  -1135.04f,  473.445f, 0.0},                      // same as above, closest to door (ori 5.75959)
    {-7348.849f, -1495.134f, 552.5152f, 0.0},                   // nefarian spawn location (ori 1.798)
};

// Coords used in intro event for Vaelastrasz to spawn Nefarius at the throne and sort the gobelins
static const float aNefariusSpawnLoc[4] = { -7466.16f, -1040.80f, 412.053f, 2.14675f};
static const float fVaelXPos = -7483.0f;

// 副本进度与状态机管理类：lair（instance_blackwing_lair）
class instance_blackwing_lair : public ScriptedInstance
{
    public:
        instance_blackwing_lair(Map* pMap);
        ~instance_blackwing_lair() {}

        // 初始化副本数据结构
        void Initialize() override;
        bool IsEncounterInProgress() const override;

        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;

        void OnCreatureEnterCombat(Creature* pCreature) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;

        void OnCreatureRespawn(Creature* creature) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        void SetData64(uint32 uiData, uint64 uiGuid) override;

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        void InitiateBreath(uint32 uiEventId);
        void InitiateDrakonid(uint32 uiEventId);

        bool CheckConditionCriteriaMeet(Player const* pPlayer, uint32 uiInstanceConditionId, WorldObject const* pConditionSource, uint32 conditionSourceType) const override;

    protected:
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组

        void CleanupNefarianStage(bool fullCleanup);

        uint32 m_uiResetTimer;                              // Reset 技能计时器
        uint32 m_uiDefenseTimer;                            // Defense 技能计时器
        uint32 m_uiScepterEpicTimer;                        // ScepterEpic 技能计时器
        uint32 m_uiNefarianSpawnTimer;                      // NefarianSpawn 技能计时器
        uint8 m_uiScepterQuestStep;
        uint8 m_uiDragonspawnCount;
        uint8 m_uiBlackwingDefCount;
        uint8 m_uiDeadDrakonidsCount;

        bool m_bIsMainGateOpen;                             // IsMainGateOpen 状态标记

        GuidList m_lDragonEggsGuids;
        GuidList m_lDrakonidBonesGuids;
        GuidList m_lDefendersGuids;
        GuidList m_lUsedEggsGuids;
        GuidList m_lDrakonidSpawnerGuids;
        GuidList m_drakonids;
        GuidVector m_vGeneratorGuids;
};

#endif                                                      // 结束头文件防重复包含宏
