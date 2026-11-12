/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 黑石深渊（Blackrock Depths）副本全局常量与枚举定义头文件

#ifndef DEF_BRD_H                                           // 防止头文件重复包含
#define DEF_BRD_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER           = 13,                           // encounter 标识 ID
    MAX_RELIC_DOORS         = 12,                           // 遗物doors 标识 ID
    MAX_DWARFS              = 7,                            // dwarfs 标识 ID
    MAX_DWARF_RUNES         = 7,                            // 矮人符文 标识 ID
    MAX_CRONIES             = 3,                            // cronies 标识 ID

    TYPE_RING_OF_LAW        = 1,                            // 竞技场of法则 遭遇战进度数据标识 ID
    TYPE_VAULT              = 2,                            // 宝库 遭遇战进度数据标识 ID
    TYPE_ROCKNOT            = 3,                            // rocknot 遭遇战进度数据标识 ID
    TYPE_TOMB_OF_SEVEN      = 4,                            // 墓穴ofseven 遭遇战进度数据标识 ID
    TYPE_LYCEUM             = 5,                            // lyceum 遭遇战进度数据标识 ID
    TYPE_IRON_HALL          = 6,                            // 黑铁大厅 遭遇战进度数据标识 ID
    TYPE_QUEST_JAIL_BREAK   = 7,                            // 任务监狱越狱 遭遇战进度数据标识 ID
    TYPE_FLAMELASH          = 8,                            // 弗莱拉斯 遭遇战进度数据标识 ID
    TYPE_HURLEY             = 9,                            // hurley 遭遇战进度数据标识 ID
    TYPE_BRIDGE             = 10,                           // 桥梁 遭遇战进度数据标识 ID
    TYPE_BAR                = 11,                           // 酒吧 遭遇战进度数据标识 ID
    TYPE_PLUGGER            = 12,                           // plugger 遭遇战进度数据标识 ID
    TYPE_NAGMARA            = 13,                           // nagmara 遭遇战进度数据标识 ID

    NPC_EMPEROR             = 9019,                         // 皇帝 Entry ID
    NPC_PRINCESS            = 8929,                         // 公主 Entry ID
    NPC_PRIESTESS           = 10076,                        // 女祭司 Entry ID
    NPC_HATEREL             = 9034,                         // 仇恨者 Entry ID
    NPC_ANGERREL            = 9035,                         // 愤怒者 Entry ID
    NPC_VILEREL             = 9036,                         // 邪恶者 Entry ID
    NPC_GLOOMREL            = 9037,                         // 阴沉者 Entry ID
    NPC_SEETHREL            = 9038,                         // 狂怒者 Entry ID
    NPC_DOOMREL             = 9039,                         // 厄运者 Entry ID
    NPC_DOPEREL             = 9040,                         // 愚钝者 Entry ID
    NPC_MAGMUS              = 9938,                         // 麦格穆斯 Entry ID
    NPC_WATCHER_DOOMGRIP    = 9476,                         // 观察者末日之握 Entry ID
    NPC_WARBRINGER_CONST    = 8905,                         // Four of them in Relict Vault are related to Doomgrip summon event
    NPC_LOREGRAIN           = 9024,                         // 洛考尔 Entry ID
    NPC_FIREGUARD_DESTROYER = 8911,                         // 火誓毁灭者 Entry ID

    // Jail Break event related (监狱越狱事件related)
    NPC_OGRABISI            = 9677,                         // ograbisi Entry ID
    NPC_SHILL               = 9678,                         // shill Entry ID
    NPC_CREST               = 9680,                         // crest Entry ID
    NPC_JAZ                 = 9681,                         // jaz Entry ID
    NPC_TOBIAS              = 9679,                         // tobias Entry ID
    NPC_DUGHAL              = 9022,                         // dughal Entry ID

    // Arena crowd (竞技场观众)
    NPC_ARENA_SPECTATOR     = 8916,                         // arenaspectator Entry ID
    NPC_SHADOWFORGE_PEASANT = 8896,                         // shadowforgepeasant Entry ID
    NPC_SHADOWFORGE_CITIZEN = 8902,                         // shadowforgecitizen Entry ID
    NPC_SHADOWFORGE_SENATOR = 8904,                         // shadowforgesenator Entry ID
    NPC_ANVILRAGE_SOLDIER   = 8893,                         // anvilragesoldier Entry ID
    NPC_ANVILRAGE_MEDIC     = 8894,                         // anvilrage医师 Entry ID
    NPC_ANVILRAGE_OFFICER   = 8895,                         // anvilrageofficer Entry ID

    // Spawned on Shadowforge bridge
    NPC_ANVILRAGE_GUARDMAN  = 8891,                         // anvilrageguardman Entry ID

    // Grim Guzzler bar events (黑铁酒吧事件)
    NPC_PHALANX             = 9502,                         // phalanx Entry ID
    NPC_GRIM_PATRON         = 9545,                         // grimpatron Entry ID
    NPC_GUZZLING_PATRON     = 9547,                         // guzzlingpatron Entry ID
    NPC_HAMMERED_PATRON     = 9554,                         // hammeredpatron Entry ID
    NPC_HURLEY_BLACKBREATH  = 9537,                         // hurleyblackbreath Entry ID
    NPC_BLACKBREATH_CRONY   = 9541,                         // blackbreath同伙 Entry ID
    NPC_PLUGGER_SPAZZRING   = 9499,                         // pluggerspazzring Entry ID
    NPC_PRIVATE_ROCKNOT     = 9503,                         // privaterocknot Entry ID
    NPC_MISTRESS_NAGMARA    = 9500,                         // mistressnagmara Entry ID
    NPC_RIBBLY_SCREWSPIGGOT = 9543,                         // ribblyscrewspiggot Entry ID

    GO_ARENA_1              = 161525,                       // arena1 游戏对象 Entry ID
    GO_ARENA_2              = 161522,                       // arena2 游戏对象 Entry ID
    GO_ARENA_3              = 161524,                       // arena3 游戏对象 Entry ID
    GO_ARENA_4              = 161523,                       // arena4 游戏对象 Entry ID

    GO_SHADOW_LOCK          = 161460,                       // 暗影lock 游戏对象 Entry ID
    GO_SHADOW_MECHANISM     = 161461,                       // 暗影mechanism 游戏对象 Entry ID
    GO_SHADOW_GIANT_DOOR    = 157923,                       // 暗影giant门 游戏对象 Entry ID
    GO_SHADOW_DUMMY         = 161516,                       // 暗影dummy 游戏对象 Entry ID
    GO_BAR_KEG_SHOT         = 170607,                       // 酒吧酒桶shot 游戏对象 Entry ID
    GO_BAR_KEG_TRAP         = 171941,                       // 酒吧酒桶trap 游戏对象 Entry ID
    GO_BAR_DOOR             = 170571,                       // 酒吧门 游戏对象 Entry ID
    GO_TOMB_ENTER           = 170576,                       // 墓穴enter 游戏对象 Entry ID
    GO_TOMB_EXIT            = 170577,                       // 墓穴exit 游戏对象 Entry ID
    GO_LYCEUM               = 170558,                       // lyceum 游戏对象 Entry ID
    GO_GOLEM_ROOM_N         = 170573,                       // 魔像roomn 游戏对象 Entry ID
    GO_GOLEM_ROOM_S         = 170574,                       // 魔像rooms 游戏对象 Entry ID
    GO_THRONE_ROOM          = 170575,                       // throneroom 游戏对象 Entry ID

    GO_SPECTRAL_CHALICE     = 164869,                       // 幽灵chalice 游戏对象 Entry ID
    GO_CHEST_SEVEN          = 169243,                       // 宝箱seven 游戏对象 Entry ID
    GO_ARENA_SPOILS         = 181074,                       // arenaspoils 游戏对象 Entry ID
    GO_SECRET_DOOR          = 174553,                       // secret门 游戏对象 Entry ID
    GO_SECRET_SAFE          = 161495,                       // secretsafe 游戏对象 Entry ID

    // Jail break event related (监狱越狱事件related)
    GO_JAIL_DOOR_SUPPLY     = 170561,                       // 监狱门supply 游戏对象 Entry ID
    GO_JAIL_SUPPLY_CRATE    = 166872,                       // 监狱supplycrate 游戏对象 Entry ID

    GO_DWARFRUNE_A01        = 170578,                       // dwarfrunea01 游戏对象 Entry ID
    GO_DWARFRUNE_B01        = 170579,                       // dwarfruneb01 游戏对象 Entry ID
    GO_DWARFRUNE_C01        = 170580,                       // dwarfrunec01 游戏对象 Entry ID
    GO_DWARFRUNE_D01        = 170581,                       // dwarfruned01 游戏对象 Entry ID
    GO_DWARFRUNE_E01        = 170582,                       // dwarfrunee01 游戏对象 Entry ID
    GO_DWARFRUNE_F01        = 170583,                       // dwarfrunef01 游戏对象 Entry ID
    GO_DWARFRUNE_G01        = 170584,                       // dwarfruneg01 游戏对象 Entry ID

    QUEST_ROYAL_RESCUE      = 4003,                         // horde quest (horde任务)
    QUEST_FATE_KINGDOM      = 4362,                         // alliance quest (alliance任务)

    SPELL_STONED            = 10255,                        // Aura of Warbringer Constructs in Relict Vault

    FACTION_DWARF_HOSTILE   = 754,                          // Hostile faction for the Tomb of the Seven dwarfs
    FACTION_ARENA_NEUTRAL   = 15,                           // Neutral faction for NPC in top of Arena after event complete
    FACTION_DARK_IRON       = 54,                           // Hostile faction for the Grim Guzzler
    FACTION_IRONFORGE       = 122,                          // Faction used by Phalanx when Plugger is killed (yes, friendly to Alliance, Hostile to Horde, very strange)

    // enum used to handle the various Grim Guzzler  bar patron's reaction
    // depending on the actions and events triggered by players
    PATRON_EMOTE            = 0,                            // 表情 标识 ID
    PATRON_PISSED           = 1,                            // pissed 标识 ID
    PATRON_HOSTILE          = 2,                            // hostile 标识 ID
    SAY_PISSED_PATRON_1     = -1230037,                     // pissedpatron1 台词广播 ID
    SAY_PISSED_PATRON_2     = -1230038,                     // pissedpatron2 台词广播 ID
    SAY_PISSED_PATRON_3     = -1230039,                     // pissedpatron3 台词广播 ID
    SAY_ROCKNOT_DESPAWN     = -1230047,                     // rocknot消除 台词广播 ID
    YELL_PATROL_1           = -1230048,                     // patrol1 大喊台词广播 ID
    YELL_PATROL_2           = -1230049,                     // patrol2 大喊台词广播 ID
    SPELL_NAGMARA_VANISH    = 15341,                        // nagmaravanish 法术 ID

    // Emperor Dagran Thaurissan (皇帝Dagran索瑞森)
    YELL_SENATOR_1          = -1230060,                     // senator1 大喊台词广播 ID
    YELL_SENATOR_2          = -1230061,                     // senator2 大喊台词广播 ID
    YELL_SENATOR_3          = -1230062,                     // senator3 大喊台词广播 ID
    YELL_SENATOR_4          = -1230063,                     // senator4 大喊台词广播 ID

    YELL_MAGMUS_INTRO       = -1230070,                     // 麦格穆斯开场 大喊台词广播 ID

    NPC_DIREBREW_MINION     = 26776,                        // 烈酒minion Entry ID
    NPC_COREN_DIREBREW      = 23872,                        // 科林烈酒 Entry ID
};

// Random emotes for Grim Guzzler patrons
static const uint32 aPatronsEmotes[] =
{
    EMOTE_ONESHOT_EXCLAMATION, EMOTE_ONESHOT_CHEER, EMOTE_ONESHOT_CHEER, EMOTE_ONESHOT_LAUGH, EMOTE_ONESHOT_LAUGH, EMOTE_ONESHOT_LAUGH
};

struct ArenaCylinder
{
    float m_fCenterX;
    float m_fCenterY;
    float m_fCenterZ;
    uint32 m_uiRadius;
    uint32 m_uiHeight;
};

static const ArenaCylinder aArenaCrowdVolume = { 595.78f, -188.65f, -38.63f, 69, 10 };

// 枚举定义：法术技能、生物与环境事件标识
enum ArenaNPCs
{
    // Gladiators (角斗士)
    NPC_LEFTY               = 16049,                        // lefty Entry ID
    NPC_ROTFANG             = 16050,                        // rotfang Entry ID
    NPC_SNOKH               = 16051,                        // snokh Entry ID
    NPC_MALGEN              = 16052,                        // malgen Entry ID
    NPC_KORV                = 16053,                        // korv Entry ID
    NPC_REZZNIK             = 16054,                        // rezznik Entry ID
    NPC_VAJASHNI            = 16055,                        // vajashni Entry ID
    NPC_VOLIDA              = 16058,                        // volida Entry ID
    NPC_THELDREN            = 16059,                        // theldren Entry ID
    // Ring mobs (竞技场小怪)
    NPC_WORM                = 8925,                         // worm Entry ID
    NPC_STINGER             = 8926,                         // stinger Entry ID
    NPC_SCREECHER           = 8927,                         // screecher Entry ID
    NPC_THUNDERSNOUT        = 8928,                         // thundersnout Entry ID
    NPC_CREEPER             = 8933,                         // creeper Entry ID
    NPC_BEETLE              = 8932,                         // beetle Entry ID
    // Ring bosses (竞技场首领)
    NPC_GOROSH              = 9027,                         // gorosh Entry ID
    NPC_GRIZZLE             = 9028,                         // grizzle Entry ID
    NPC_EVISCERATOR         = 9029,                         // eviscerator Entry ID
    NPC_OKTHOR              = 9030,                         // okthor Entry ID
    NPC_ANUBSHIAH           = 9031,                         // anubshiah Entry ID
    NPC_HEDRUM              = 9032                          // hedrum Entry ID
};

static const uint32 aArenaNPCs[] =
{
    // Gladiators (角斗士)
    NPC_LEFTY, NPC_ROTFANG, NPC_SNOKH, NPC_MALGEN, NPC_KORV, NPC_REZZNIK, NPC_VAJASHNI, NPC_VOLIDA, NPC_THELDREN,
    // Ring mobs (竞技场小怪)
    NPC_WORM, NPC_STINGER, NPC_SCREECHER, NPC_THUNDERSNOUT, NPC_CREEPER, NPC_BEETLE,
    // Ring bosses (竞技场首领)
    NPC_GOROSH, NPC_GRIZZLE, NPC_EVISCERATOR, NPC_OKTHOR, NPC_ANUBSHIAH, NPC_HEDRUM
};

// Used to summon Watcher Doomgrip (Usedto召唤观察者末日之握)
static const float aVaultPositions[4] = {821.905f, -338.382f, -50.134f, 3.78736f};

// Used to summon Hurley Blackbreath
static const float aHurleyPositions[4] = {856.0867f, -149.7469f, -49.6719f, 0.05949629f};

// Used to summon the patrol in Grim Guzzler
static const float aBarPatrolPositions[2][4] =
{
    {872.7059f, -232.5491f, -43.7525f, 2.069044f},
    {865.5645f, -219.7471f, -43.7033f, 2.033881f}
};

static const uint32 aBarPatrolId[3] = {NPC_FIREGUARD_DESTROYER, NPC_ANVILRAGE_OFFICER, NPC_ANVILRAGE_OFFICER};

// Tomb of the Seven dwarfs
static const uint32 aTombDwarfes[MAX_DWARFS] = {NPC_ANGERREL, NPC_SEETHREL, NPC_DOPEREL, NPC_GLOOMREL, NPC_VILEREL, NPC_HATEREL, NPC_DOOMREL};

// 副本进度与状态机管理类：depths（instance_blackrock_depths）
class instance_blackrock_depths : public ScriptedInstance
{
    public:
        instance_blackrock_depths(Map* pMap);
        ~instance_blackrock_depths() {}

        // 初始化副本数据结构
        void Initialize() override;

        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        void OnCreatureEnterCombat(Creature* pCreature) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;
        void OnCreatureEvade(Creature* pCreature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        void OnCreatureRespawn(Creature* creature) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        // Arena Event (Arena事件)
        void SetArenaCenterCoords(float fX, float fY, float fZ) { m_fArenaCenterX = fX; m_fArenaCenterY = fY; m_fArenaCenterZ = fZ; }
        void GetArenaCenterCoords(float& fX, float& fY, float& fZ) const
        { fX = m_fArenaCenterX; fY = m_fArenaCenterY; fZ = m_fArenaCenterZ; }
        void GetArenaCrowdGuid(GuidSet& sCrowdSet) const { sCrowdSet = m_sArenaCrowdNpcGuids; }

        // Bar events (酒吧events)
        void SetBarDoorIsOpen() { m_bIsBarDoorOpen = true; }
        void GetBarDoorIsOpen(bool& bIsOpen) const { bIsOpen = m_bIsBarDoorOpen; }
        void HandleBarPatrons(uint8 uiEventType);
        void HandleBarPatrol(uint8 uiStep);

    private:
        void DoCallNextDwarf();
        bool CanReplacePrincess() const;

        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        // Grim Guzzler bar events (黑铁酒吧事件)
        bool  m_bIsBarDoorOpen;                             // IsBarDoorOpen 状态标记
        uint32 m_uiBarAleCount;
        uint32 m_uiPatronEmoteTimer;                        // PatronEmote 技能计时器
        uint8 m_uiBrokenKegs;
        uint32 m_uiPatrolTimer;                             // Patrol 技能计时器
        uint8 m_uiStolenAles;
        uint32 m_uiDagranTimer;                             // Dagran 技能计时器

        uint8 m_uiCofferDoorsOpened;

        uint8 m_uiDwarfRound;
        uint32 m_uiDwarfFightTimer;                         // DwarfFight 技能计时器

        float m_fArenaCenterX, m_fArenaCenterY, m_fArenaCenterZ;

        GuidSet m_sVaultNpcGuids;                           // sVaultNpcGuids GUID 集合
        GuidSet m_sArenaCrowdNpcGuids;                      // sArenaCrowdNpcGuids GUID 集合
        GuidSet m_sBarPatronNpcGuids;                       // sBarPatronNpcGuids GUID 集合
        GuidSet m_sBarPatrolGuids;                          // sBarPatrolGuids GUID 集合
};

#endif                                                      // 结束头文件防重复包含宏
