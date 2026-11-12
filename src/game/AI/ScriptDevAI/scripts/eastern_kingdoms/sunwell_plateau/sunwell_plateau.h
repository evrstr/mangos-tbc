/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 太阳之井高地（Sunwell Plateau）全局定义头文件

#ifndef DEF_SUNWELLPLATEAU_H                                // 防止头文件重复包含
#define DEF_SUNWELLPLATEAU_H

#include "Chat/Chat.h"
#include "AI/ScriptDevAI/base/TimerAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER               = 6,                        // encounter 标识 ID

    TYPE_KALECGOS               = 0,                        // 卡雷苟斯 遭遇战进度数据标识 ID
    TYPE_BRUTALLUS              = 1,                        // 布鲁塔卢斯 遭遇战进度数据标识 ID
    TYPE_FELMYST                = 2,                        // 菲米丝 遭遇战进度数据标识 ID
    TYPE_EREDAR_TWINS           = 3,                        // eredartwins 遭遇战进度数据标识 ID
    TYPE_MURU                   = 4,                        // 穆鲁 遭遇战进度数据标识 ID
    TYPE_KILJAEDEN              = 5,                        // 基尔加丹 遭遇战进度数据标识 ID

    NPC_KALECGOS_DRAGON         = 24850,            // kalecgos blue dragon hostile (卡雷苟斯blue巨龙hostile)
    NPC_KALECGOS_HUMAN          = 24891,            // kalecgos human form in spectral realm
    NPC_SATHROVARR              = 24892,                    // 萨索瓦尔 Entry ID
    NPC_MADRIGOSA               = 24895,                    // madrigosa Entry ID
    NPC_FLIGHT_TRIGGER_LEFT     = 25357,            // Related to Felmyst flight path. Also the anchor to summon Madrigosa
    NPC_FLIGHT_TRIGGER_RIGHT    = 25358,            // related to Felmyst flight path
    NPC_WORLD_TRIGGER           = 22515,                    // world触发 Entry ID
    NPC_WORLD_TRIGGER_LARGE     = 23472,            // ground triggers spawned in Brutallus / Felmyst arena
    NPC_BRUTALLUS               = 24882,                    // 布鲁塔卢斯 Entry ID
    NPC_FELMYST                 = 25038,                    // 菲米丝 Entry ID
    NPC_KALECGOS_MADRIGOSA      = 24844,            // kalecgos blue dragon; spawns after Felmyst
    NPC_ALYTHESS                = 25166,                    // alythess Entry ID
    NPC_SACROLASH               = 25165,                    // 萨洛拉丝 Entry ID
    NPC_SUNBLADE_PROTECTOR      = 25507,            // npc_sunblade_scout
    NPC_MURU                    = 25741,                    // 穆鲁 Entry ID
    NPC_ENTROPIUS               = 25840,                    // 熵魔 Entry ID
    NPC_BERSERKER               = 25798,            // muru trash mobs - scripted in Acid
    NPC_FURY_MAGE               = 25799,                    // fury法师 Entry ID
    NPC_DARK_FIEND              = 25744,                    // darkfiend Entry ID
    NPC_VOID_SENTINEL           = 25772,                    // voidsentinel Entry ID
    NPC_HAND_OF_THE_DECEIVER    = 25588,                    // handofthedeceiver Entry ID
    NPC_KILJAEDEN               = 25315,                    // 基尔加丹 Entry ID
    NPC_KILJAEDEN_CONTROLLER    = 25608,            // kiljaeden event controller (基尔加丹事件controller)
    NPC_ANVEENA                 = 26046,            // related to kiljaeden event (relatedto基尔加丹事件)
    NPC_KALECGOS                = 25319,            // related to kiljaeden event (relatedto基尔加丹事件)
    NPC_VELEN                   = 26246,                    // velen Entry ID
    NPC_LIADRIN                 = 26247,                    // liadrin Entry ID

    GO_FORCEFIELD               = 188421,           // kalecgos door + collisions (卡雷苟斯门+collisions)
    GO_BOSS_COLLISION_1         = 188523,                   // bosscollision1 游戏对象 Entry ID
    GO_BOSS_COLLISION_2         = 188524,                   // bosscollision2 游戏对象 Entry ID
    GO_ICE_BARRIER              = 188119,           // used to block the players path during the Brutallus intro event
    GO_FIRE_BARRIER             = 188075,           // door after felmyst (门after菲米丝)
    // GO_FIRST_GATE            = 187766,           // door between felmyst and eredar twins
    // GO_SECOND_GATE           = 187764,           // door after eredar twins
    GO_MURU_ENTER_GATE          = 187990,           // muru gates (穆鲁gates)
    GO_MURU_EXIT_GATE           = 188118,                   // 穆鲁exit门禁 游戏对象 Entry ID
    // GO_THIRD_GATE            = 187765,           // door after muru; why another?

    GO_ORB_BLUE_FLIGHT_1        = 188115,           // orbs used in the Kil'jaeden fight
    GO_ORB_BLUE_FLIGHT_2        = 188116,                   // orbblueflight2 游戏对象 Entry ID
    GO_ORB_BLUE_FLIGHT_3        = 187869,                   // orbblueflight3 游戏对象 Entry ID
    GO_ORB_BLUE_FLIGHT_4        = 188114,                   // orbblueflight4 游戏对象 Entry ID

    NPC_BLUE_ORB_TARGET         = 25640,                    // blueorb目标 Entry ID
    SPELL_RING_BLUE_FLAME       = 45825,                    // 竞技场blue烈焰 法术 ID

    SAY_KALECGOS_OUTRO          = -1580043,                 // 卡雷苟斯尾声 台词广播 ID
    SAY_TWINS_INTRO             = -1580044,                 // twins开场 台词广播 ID

    // Kil'jaeden yells (基尔加丹yells)
    SAY_ORDER_1                 = -1580064,                 // order1 台词广播 ID
    SAY_ORDER_2                 = -1580065,                 // order2 台词广播 ID
    SAY_ORDER_3                 = -1580066,                 // order3 台词广播 ID
    SAY_ORDER_4                 = -1580067,                 // order4 台词广播 ID
    SAY_ORDER_5                 = -1580068,                 // order5 台词广播 ID

    AREATRIGGER_TWINS           = 4937,                     // twins 标识 ID

    // Kalec spectral realm spells
    SPELL_TELEPORT_NORMAL_REALM = 46020,                    // 传送normalrealm 法术 ID
    SPELL_SPECTRAL_REALM_AURA   = 46021,                    // 幽灵realm光环 法术 ID
    SPELL_SPECTRAL_EXHAUSTION   = 44867,                    // 幽灵exhaustion 法术 ID
    // Felmyst ouro spell (菲米丝ouro法术)
    SPELL_OPEN_BACK_DOOR        = 46650,            // Opens the fire barrier - script effect for 46652
    // used by both muru and entropius
    SPELL_MURU_BERSERK          = 26662,                    // 穆鲁狂暴 法术 ID
    // visuals for Kiljaeden encounter
    SPELL_ANVEENA_DRAIN         = 46410,                    // anveena吸取 法术 ID
    NPC_ANVEENA_MARKER          = 26057,                    // anveenamarker Entry ID

    // felmyst (菲米丝)
    NPC_UNYELDING_DEAD          = 25268,                    // unyeldingdead Entry ID
    NPC_DEMONIC_VAPOR           = 25265,                    // demonicvapor Entry ID
    NPC_DEMONIC_VAPOR_TRAIL     = 25267,                    // demonicvaportrail Entry ID
    SPELL_FOG_CORRUPTION        = 45582,                    // fog腐蚀 法术 ID

    // gauntlet
    NPC_GAUNTLET_IMP_TRIGGER    = 25848,                    // gauntletimp触发 Entry ID
    NPC_SHADOWSWORD_COMMANDER   = 25837,                    // shadowsword指挥官 Entry ID

    SAY_GAUNTLET_IMPS           = -1580116,                 // gauntletimps 台词广播 ID

    NPC_SHADOWSWORD_DEATHBRINGER= 25485,                    // shadowsworddeathbringer Entry ID
    NPC_VOLATILE_FIEND          = 25851,                    // volatilefiend Entry ID

    SPELL_CRYSTAL_CHANNEL       = 46174, // Shadowsword Soulbinder

    // twins
    GO_BLAZE                    = 187366,                   // 烈焰 游戏对象 Entry ID
    NPC_SHADOW_IMAGE            = 25214,                    // 暗影影像 Entry ID
    DB_ENCOUNTER_TWINS          = 727,                      // encountertwins 标识 ID

    // muru (穆鲁)
    NPC_MURU_PORTAL_TARGET      = 25770,                    // 穆鲁传送门目标 Entry ID
    SPELL_TRANSFORM_VISUAL_PERIODIC = 46205,                // 变形视觉特效periodic 法术 ID
    NPC_VOID_SPAWN              = 25824,                    // void生成 Entry ID
    NPC_DARKNESS                = 25879,                    // 黑暗 Entry ID
    NPC_SINGULARITY             = 25855,                    // singularity Entry ID

    // KJ
    MAX_DECEIVERS               = 3,                        // deceivers 标识 ID
    NPC_SINISTER_REFLECTION     = 25708,                    // sinister倒影镜像 Entry ID

    // outro (尾声剧情)
    SPELL_SIMPLE_TELEPORT       = 12980,                    // simple传送 法术 ID

    GUID_PREFIX                 = 5800000,                  // prefix 标识 ID
};

static const Position aMadrigosaLoc[] =
{
    {1463.82f, 661.212f, 19.7971f, 4.712551f},      // reload spawn loc - the place where to spawn Felmyst
    {1463.82f, 661.212f, 39.234f,  0.f},            // fly loc during the cinematig
    {1464.6943f, 652.1426f, 20.81918f, 0.f},        // landing loc after fel fireball
};

static const Position aKalecLoc[] =
{
    {1573.1461f, 755.20245f, 99.524956f, 3.59537816047668457f}, // spawn loc (生成loc)
    {1474.2347f, 624.0703f,  29.32589f,  0.f}, // first move
    {1511.6547f, 550.70245f, 25.510078f, 0.f}, // open door (open门)
    {1648.255f,  519.3769f,  165.8482f,  0.f}, // fly away
};

static const float afMuruSpawnLoc[4] = { 1816.401f, 625.8939f, 69.68857f, 5.585053f };

static const float aKalegSpawnLoc[4] = { 1734.465f, 592.5678f, 142.3971f, 4.533074f };

// 副本进度与状态机管理类：plateau（instance_sunwell_plateau）
class instance_sunwell_plateau : public ScriptedInstance, private DialogueHelper, public TimerManager
{
    public:
        instance_sunwell_plateau(Map* pMap);
        ~instance_sunwell_plateau() {}

        // 初始化副本数据结构
        void Initialize() override;
        bool IsEncounterInProgress() const override;

        // 玩家进入副本区域回调
        void OnPlayerEnter(Player* pPlayer) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        void OnCreatureRespawn(Creature* creature) override;
        void OnCreatureEnterCombat(Creature* creature) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;
        void OnCreatureEvade(Creature* pCreature) override;
        void OnPlayerDeath(Player* player) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        ObjectGuid SelectFelmystFlightTrigger(bool bLeftSide, uint8 uiIndex);

        void AddToSpectralRealm(ObjectGuid playerGuid) { m_spectralRealmPlayers.insert(playerGuid); }
        void RemoveFromSpectralRealm(ObjectGuid playerGuid) { m_spectralRealmPlayers.erase(playerGuid); }
        void DoEjectSpectralPlayers();

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        void ShowChatCommands(ChatHandler* handler) override;
        void ExecuteChatCommand(ChatHandler* handler, char* args) override;

        bool IsBrutallusIntroStarted() const { return m_brutallusIntroStarted; }
        void StartBrutallusIntro() { m_brutallusIntroStarted = true; }

        void DropFog();

    protected:
        void JustDidDialogueStep(int32 iEntry) override;
        void DoSortFlightTriggers();
        void SpawnFelmyst();
        void SpawnGauntlet(bool respawn = false);
        void SpawnTwins();
        void SpawnMuru();
        void SpawnKiljaeden();
        void FailKiljaeden();

        void StartImps();
        void StopImps();
        void ResetGauntlet(bool stop = false);
        void RespawnGauntlet();
        void ImpYell();
        void StartMiniEvent();

        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        bool m_brutallusIntroStarted;                       // rutallusIntroStarted 状态标记

        bool m_firstEnter;                                  // 是否首次进入副本标记
        bool m_spawnedGauntlet;
        bool m_impsStarted;
        bool m_miniAttackEvent;
        TimePoint m_gauntletRespawnTime;

        // Misc
        uint8 m_uiDeceiversKilled;
        uint32 m_uiSpectralRealmTimer;                      // SpectralRealm 技能计时器
        uint32 m_uiMuruBerserkTimer;                        // MuruBerserk 技能计时器
        uint32 m_uiKiljaedenYellTimer;                      // KiljaedenYell 技能计时器

        GuidSet m_spectralRealmPlayers;                     // spectralRealmPlayers GUID 集合
        GuidVector m_vRightFlightTriggersVect;
        GuidVector m_vLeftFlightTriggersVect;
        GuidList m_lAllFlightTriggersList;
        GuidList m_lBackdoorTriggersList;
        std::vector<uint32> m_kiljaedenRespawnDbGuids;
        GuidVector m_muruTrashGuids;
        GuidVector m_felmystSpawns;
        GuidVector m_gauntletSpawns;
        GuidVector m_twinsSpawns;
};
#endif                                                      // 结束头文件防重复包含宏
