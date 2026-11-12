/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 纳克萨玛斯（Naxxramas）副本全局定义头文件

#ifndef DEF_NAXXRAMAS_H                                     // 防止头文件重复包含
#define DEF_NAXXRAMAS_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER               = 16,                       // encounter 标识 ID

    // A few instance-script related texts
    SAY_THADDIUS_GREET          = -1533029,                 // 塔迪乌斯问候 台词广播 ID

    // Background screams in Instance if Thaddius still alive, four of them from 8873 to 8876
    SOUND_SCREAM1               = 8873,                     // scream1 音效 ID

    // Kel'Thuzad (克尔苏加德)
    SAY_KELTHUZAD_CAT_DIED      = 13150,                    // 克尔苏加德catdied 台词广播 ID
    SAY_KELTHUZAD_SHACKLES_1    = 13492,                    // 克尔苏加德shackles1 台词广播 ID
    // SOUND_KELTHUZAD_SHACKLES_2    = 9089, // not used officially
    // Kel'Thuzad's taunts after killing Wing Bosses
    SAY_KELTHUZAD_TAUNT1        = 12984,                    // 克尔苏加德taunt1 台词广播 ID
    SAY_KELTHUZAD_TAUNT2        = 12985,                    // 克尔苏加德taunt2 台词广播 ID
    SAY_KELTHUZAD_TAUNT3        = 12986,                    // 克尔苏加德taunt3 台词广播 ID
    SAY_KELTHUZAD_TAUNT4        = 12987,                    // 克尔苏加德taunt4 台词广播 ID
    EMOTE_FLEE                  = 12391,                    // flee 表情动作 ID
    // Dialogues with Lich King
    SAY_SAPP_DIALOG1            = 12990,                    // sappdialog1 台词广播 ID
    SAY_SAPP_DIALOG2_LICH       = 12988,                    // sappdialog2lich 台词广播 ID
    SAY_SAPP_DIALOG3            = 12991,                    // sappdialog3 台词广播 ID
    SAY_SAPP_DIALOG4_LICH       = 12989,                    // sappdialog4lich 台词广播 ID
    SAY_SAPP_DIALOG5            = 12992,                    // sappdialog5 台词广播 ID
    // Horsemen dialogue texts
    SAY_BLAU_TAUNT1             = 13014,                    // blautaunt1 台词广播 ID
    SAY_BLAU_TAUNT2             = 13015,                    // blautaunt2 台词广播 ID
    SAY_BLAU_TAUNT3             = 13016,                    // blautaunt3 台词广播 ID
    SAY_MORG_TAUNT1             = 13058,                    // morgtaunt1 台词广播 ID
    SAY_MORG_TAUNT2             = 13059,                    // morgtaunt2 台词广播 ID
    SAY_MORG_TAUNT3             = 13060,                    // morgtaunt3 台词广播 ID
    SAY_KORT_TAUNT1             = 13038,                    // korttaunt1 台词广播 ID
    SAY_KORT_TAUNT2             = 13039,                    // korttaunt2 台词广播 ID
    SAY_KORT_TAUNT3             = 13040,                    // korttaunt3 台词广播 ID
    SAY_ZELI_TAUNT1             = 13101,                    // zelitaunt1 台词广播 ID
    SAY_ZELI_TAUNT2             = 13102,                    // zelitaunt2 台词广播 ID
    SAY_ZELI_TAUNT3             = 13103,                    // zelitaunt3 台词广播 ID
    // Grand Widow Faerlina intro (Grand黑女巫法琳娜开场)
    SAY_FAERLINA_INTRO          = 12852,                    // 法琳娜开场 台词广播 ID
    FOLLOWERS_STAND             = 1,                        // stand 标识 ID
    FOLLOWERS_AURA              = 2,                        // 光环 标识 ID
    FOLLOWERS_KNEEL             = 3,                        // kneel 标识 ID

    TYPE_ANUB_REKHAN            = 0,                        // 阿努布雷坎 遭遇战进度数据标识 ID
    TYPE_FAERLINA               = 1,                        // 法琳娜 遭遇战进度数据标识 ID
    TYPE_MAEXXNA                = 2,                        // 迈克斯纳 遭遇战进度数据标识 ID

    TYPE_NOTH                   = 3,                        // 诺斯 遭遇战进度数据标识 ID
    TYPE_HEIGAN                 = 4,                        // 希尔盖 遭遇战进度数据标识 ID
    TYPE_LOATHEB                = 5,                        // 洛欧塞布 遭遇战进度数据标识 ID

    TYPE_RAZUVIOUS              = 6,                        // 拉苏维奥斯 遭遇战进度数据标识 ID
    TYPE_GOTHIK                 = 7,                        // 戈提克 遭遇战进度数据标识 ID
    TYPE_FOUR_HORSEMEN          = 8,                        // four四骑士 遭遇战进度数据标识 ID

    TYPE_PATCHWERK              = 9,                        // 帕奇维克 遭遇战进度数据标识 ID
    TYPE_GROBBULUS              = 10,                       // 格罗布鲁斯 遭遇战进度数据标识 ID
    TYPE_GLUTH                  = 11,                       // 格拉斯 遭遇战进度数据标识 ID
    TYPE_THADDIUS               = 12,                       // 塔迪乌斯 遭遇战进度数据标识 ID

    TYPE_SAPPHIRON              = 13,                       // 萨菲隆 遭遇战进度数据标识 ID
    TYPE_KELTHUZAD              = 14,                       // 克尔苏加德 遭遇战进度数据标识 ID

    NPC_ANUB_REKHAN             = 15956,                    // 阿努布雷坎 Entry ID
    NPC_FAERLINA                = 15953,                    // 法琳娜 Entry ID
    NPC_NAXXRAMAS_CULTIST       = 15980,                    // naxxramas教徒 Entry ID
    NPC_NAXXRAMAS_ACOLYTE       = 15981,                    // naxxramas侍僧 Entry ID
    NPC_CORPSE_SCARAB           = 16698,                    // 尸体甲虫 Entry ID

    NPC_ZOMBIE_CHOW             = 16360,                    // 僵尸食物 Entry ID
    NPC_GLUTH                   = 15932,                    // 格拉斯 Entry ID
    NPC_THADDIUS                = 15928,                    // 塔迪乌斯 Entry ID
    NPC_STALAGG                 = 15929,                    // 斯塔拉格 Entry ID
    NPC_FEUGEN                  = 15930,                    // feugen Entry ID
    NPC_TESLA_COIL              = 16218,                    // 特斯拉电极coil Entry ID
    NPC_NAXXRAMAS_TRIGGER       = 16082,                    // naxxramas触发 Entry ID
    NPC_TOXIC_TUNNEL            = 16400,                    // toxictunnel Entry ID

    NPC_MOGRAINE                = 16062,                    // 莫格莱尼 Entry ID
    NPC_ZELIEK                  = 16063,                    // 瑟里耶克 Entry ID
    NPC_THANE                   = 16064,                    // thane Entry ID
    NPC_BLAUMEUX                = 16065,                    // 布劳缪克丝 Entry ID

    NPC_SPIRIT_OF_MOGRAINE      = 16775,                    // 之魂of莫格莱尼 Entry ID
    NPC_SPIRIT_OF_BLAUMEUX      = 16776,                    // 之魂of布劳缪克丝 Entry ID
    NPC_SPIRIT_OF_ZELIREK       = 16777,                    // 之魂ofzelirek Entry ID
    NPC_SPIRIT_OF_KORTHAZZ      = 16778,                    // 之魂of库尔塔兹 Entry ID

    NPC_SAPPHIRON               = 15989,                    // 萨菲隆 Entry ID
    NPC_KELTHUZAD               = 15990,                    // 克尔苏加德 Entry ID
    NPC_THE_LICHKING            = 16980,                    // thelichking Entry ID
    NPC_MR_BIGGLESWORTH         = 16998,                    // mrbigglesworth Entry ID
    NPC_LIVING_POISON           = 16027,                    // living毒药 Entry ID

    // Gothik (戈提克)
    NPC_GOTHIK                  = 16060,                    // 戈提克 Entry ID
    NPC_SUB_BOSS_TRIGGER        = 16137,                    // summon locations (召唤 locations)
    NPC_UNREL_TRAINEE           = 16124,                    // unreltrainee Entry ID
    NPC_UNREL_DEATH_KNIGHT      = 16125,                    // unrel死亡骑士 Entry ID
    NPC_UNREL_RIDER             = 16126,                    // unrelrider Entry ID
    NPC_SPECT_TRAINEE           = 16127,                    // specttrainee Entry ID
    NPC_SPECT_DEATH_KNIGHT      = 16148,                    // spect死亡骑士 Entry ID
    NPC_SPECT_RIDER             = 16150,                    // spectrider Entry ID
    NPC_SPECT_HORSE             = 16149,                    // specthorse Entry ID

    // Kel'Thuzad (克尔苏加德)
    NPC_SOLDIER_FROZEN          = 16427,                    // soldierfrozen Entry ID
    NPC_UNSTOPPABLE_ABOM        = 16428,                    // unstoppableabom Entry ID
    NPC_SOUL_WEAVER             = 16429,                    // 灵魂weaver Entry ID
    NPC_GUARDIAN                = 16441,                    // guardian Entry ID

    // Arachnid Quarter
    GO_ARAC_ANUB_DOOR           = 181126,                   // encounter door (遭遇战大门)
    GO_ARAC_ANUB_GATE           = 181195,                   // open after boss is dead
    GO_ARAC_FAER_WEB            = 181235,                   // encounter door (遭遇战大门)
    GO_ARAC_FAER_DOOR           = 181167,                   // after faerlina, to outer ring (after法琳娜to外竞技场)
    GO_ARAC_MAEX_INNER_DOOR     = 181197,                   // encounter door (遭遇战大门)
    GO_ARAC_MAEX_OUTER_DOOR     = 181209,                   // right before maex

    // Plague Quarter
    GO_PLAG_SLIME01_DOOR        = 181198,                   // not used
    GO_PLAG_SLIME02_DOOR        = 181199,                   // not used
    GO_PLAG_NOTH_ENTRY_DOOR     = 181200,                   // encounter door (遭遇战大门)
    GO_PLAG_NOTH_EXIT_DOOR      = 181201,                   // exit, open when boss dead
    GO_PLAG_HEIG_ENTRY_DOOR     = 181202,                   // plagheigentry门 游戏对象 Entry ID
    GO_PLAG_HEIG_EXIT_DOOR      = 181203,                   // exit door - not used here
    GO_PLAG_HEIG_EXIT_HALLWAY   = 181496,                   // exit door, at the end of the hallway
    GO_PLAG_LOAT_DOOR           = 181241,                   // encounter door (遭遇战大门)

    // Military Quarter
    GO_MILI_GOTH_ENTRY_GATE     = 181124,                   // used while encounter is in progress
    GO_MILI_GOTH_EXIT_GATE      = 181125,                   // exit, open at boss dead
    GO_MILI_GOTH_COMBAT_GATE    = 181170,                   // used while encounter is in progress
    GO_MILI_HORSEMEN_DOOR       = 181119,                   // encounter door (遭遇战大门)

    GO_CHEST_HORSEMEN_NORM      = 181366,                   // four horsemen event, DoRespawnGameObject() when event == DONE

    // Construct Quarter
    GO_CONS_PATH_EXIT_DOOR      = 181123,                   // cons路径exit门 游戏对象 Entry ID
    GO_CONS_GLUT_EXIT_DOOR      = 181120,                   // consglutexit门 游戏对象 Entry ID
    GO_CONS_THAD_DOOR           = 181121,                   // Thaddius enc door (塔迪乌斯enc门)
    GO_CONS_NOX_TESLA_FEUGEN    = 181477,                   // consnox特斯拉电极feugen 游戏对象 Entry ID
    GO_CONS_NOX_TESLA_STALAGG   = 181478,                   // consnox特斯拉电极斯塔拉格 游戏对象 Entry ID

    // Frostwyrm Lair
    GO_KELTHUZAD_WATERFALL_DOOR = 181225,                   // exit, open after sapphiron is dead
    GO_KELTHUZAD_EXIT_DOOR      = 181228,                   // 克尔苏加德exit门 游戏对象 Entry ID
    GO_KELTHUZAD_WINDOW_1       = 181402,                   // 克尔苏加德window1 游戏对象 Entry ID
    GO_KELTHUZAD_WINDOW_2       = 181403,                   // 克尔苏加德window2 游戏对象 Entry ID
    GO_KELTHUZAD_WINDOW_3       = 181404,                   // 克尔苏加德window3 游戏对象 Entry ID
    GO_KELTHUZAD_WINDOW_4       = 181405,                   // 克尔苏加德window4 游戏对象 Entry ID
    GO_KELTHUZAD_TRIGGER        = 181444,                   // 克尔苏加德触发 游戏对象 Entry ID

    // Eyes
    GO_ARAC_EYE_RAMP            = 181212,                   // araceyeramp 游戏对象 Entry ID
    GO_PLAG_EYE_RAMP            = 181211,                   // plageyeramp 游戏对象 Entry ID
    GO_MILI_EYE_RAMP            = 181210,                   // milieyeramp 游戏对象 Entry ID
    GO_CONS_EYE_RAMP            = 181213,                   // conseyeramp 游戏对象 Entry ID

    GO_ARAC_EYE_BOSS            = 181233,                   // araceyeboss 游戏对象 Entry ID
    GO_PLAG_EYE_BOSS            = 181231,                   // plageyeboss 游戏对象 Entry ID
    GO_MILI_EYE_BOSS            = 181230,                   // milieyeboss 游戏对象 Entry ID
    GO_CONS_EYE_BOSS            = 181232,                   // conseyeboss 游戏对象 Entry ID

    // Portals
    GO_ARAC_PORTAL              = 181575,                   // arac传送门 游戏对象 Entry ID
    GO_PLAG_PORTAL              = 181577,                   // plag传送门 游戏对象 Entry ID
    GO_MILI_PORTAL              = 181578,                   // mili传送门 游戏对象 Entry ID
    GO_CONS_PORTAL              = 181576,                   // cons传送门 游戏对象 Entry ID

    AREATRIGGER_FROSTWYRM       = 4120,                     // not needed here, but AT to be scripted
    AREATRIGGER_KELTHUZAD       = 4112,                     // 克尔苏加德 标识 ID
    AREATRIGGER_GOTHIK          = 4116,                     // 戈提克 标识 ID
    AREATRIGGER_THADDIUS_DOOR   = 4113,                     // 塔迪乌斯门 标识 ID
    AREATRIGGER_FROSTWYRM_TELE  = 4156,                     // frostwyrmtele 标识 ID
    AREATRIGGER_FAERLINA_INTRO  = 4115,                     // 法琳娜开场 标识 ID

    // Gothik summon steps (戈提克召唤steps)
    STEP_TRAINEE                = 0,                        // trainee 标识 ID
    STEP_KNIGHT                 = 1,                        // 骑士 标识 ID
    STEP_RIDER                  = 2,                        // rider 标识 ID
    SPELL_SUMMON_TRAINEE_PERIODIC        = 28007,                    // Triggers 27884 every 20s
    SPELL_SUMMON_KNIGHT_PERIODIC         = 28009,                    // Triggers 28008 every 25s
    SPELL_SUMMON_MOUNTED_KNIGHT_PERIODIC = 28011,                    // Triggers 28010 every 30s
    
    SPELL_SPECTRAL_ASSAULT      = 28781,                    // 幽灵突袭 法术 ID
    SPELL_UNRELENTING_ASSAULT   = 29874,                    // unrelenting突袭 法术 ID

    EVENT_ID_DECIMATE           = 10495,                    // ID残杀 事件标识 ID
    EVENT_CLEAR_SHACKLES        = 10536,                    // clearshackles 事件标识 ID
    EVENT_GUARDIAN_SHACKLE      = 10537,                    // guardianshackle 事件标识 ID

    SPELL_DARK_CHANNELING       = 21157,                    // darkchanneling 法术 ID
    SPELL_CHANNEL_VISUAL        = 29423,                    // Periodically trigger 29422
    SPELL_CLEAR_ALL_SHACKLES    = 29910,                    // Cast by Kel'Thuzad if more than three Guardians of Icecrown are controlled (由 Kel'Thuzad if more than three Guardians of Icecrown are controlled 施放)

    MAX_SHACKLES                = 3,                        // How many Guardians of Icecrown can be crowed control without Kel'Thuzad dispelling the shackles

    SPELL_EXPLODE               = 28433,                    // Used by Living Poison blobs when players come in range
};

// 枚举定义：法术技能、生物与环境事件标识
enum GothikSpellDummy
{
    SPELL_A_TO_ANCHOR_1         = 27892,                    // atoanchor1 法术 ID
    SPELL_B_TO_ANCHOR_1         = 27928,                    // btoanchor1 法术 ID
    SPELL_C_TO_ANCHOR_1         = 27935,                    // ctoanchor1 法术 ID

    SPELL_A_TO_ANCHOR_2         = 27893,                    // atoanchor2 法术 ID
    SPELL_B_TO_ANCHOR_2         = 27929,                    // btoanchor2 法术 ID
    SPELL_C_TO_ANCHOR_2         = 27936,                    // ctoanchor2 法术 ID

    SPELL_A_TO_SKULL            = 27915,                    // atoskull 法术 ID
    SPELL_B_TO_SKULL            = 27931,                    // btoskull 法术 ID
    SPELL_C_TO_SKULL            = 27937,                    // ctoskull 法术 ID

    SPELL_CHOOSE_RANDOM_SKULL_PILE_A = 27896,               // chooserandomskullpilea 法术 ID
    SPELL_CHOOSE_RANDOM_SKULL_PILE_B = 27930,               // chooserandomskullpileb 法术 ID
    SPELL_CHOOSE_RANDOM_SKULL_PILE_C = 27938,               // chooserandomskullpilec 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum GothikSummonFlag {
    SUMMON_FLAG_TRAINEE         = 0x01,                     // flagtrainee 标识 ID
    SUMMON_FLAG_KNIGHT          = 0x02,                     // flag骑士 标识 ID
    SUMMON_FLAG_RIDER           = 0x04                      // flagrider 标识 ID
};

struct GothTrigger
{
    bool isRightSide;
    bool isAnchorHigh;
    int summonTypeFlag;
};

static const float sapphironPositions[4] = {3521.48f, -5234.87f, 137.626f, 4.53329f};

// Used in Construct Quarter for the deadly blobs continuously spawning in Patchwerk corridor
static const Position livingPoisonPositions[6] =
{
    {3128.692f, -3119.211f, 293.346f, 4.725505f},
    {3154.432f, -3125.669f, 293.408f, 4.456693f},
    {3175.614f, -3134.716f, 293.282f, 4.244928f},
    {3128.709f, -3157.404f, 293.3238f, 4.725505f},
    {3145.881f, -3158.563f, 293.3216f, 4.456693f},
    {3157.736f, -3164.859f, 293.2874f, 4.244928f},
};

// 副本进度与状态机管理类：标识 ID（instance_naxxramas）
class instance_naxxramas : public ScriptedInstance, private DialogueHelper
{
    public:
        instance_naxxramas(Map* map);
        ~instance_naxxramas() {}

        // 初始化副本数据结构
        void Initialize() override;

        bool IsEncounterInProgress() const override;

        // 玩家进入副本区域回调
        void OnPlayerEnter(Player* player) override;
        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* creature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* gameObject) override;

        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* creature) override;

        void OnCreatureRespawn(Creature* creature) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 type, uint32 data) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 type) const override;

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        // Gothik (戈提克)
        bool IsInRightSideGothikArea(Unit* unit);

        // Kel'Thuzad (克尔苏加德)
        void DoTaunt();

        bool DoHandleEvent(uint32 eventId);

        bool DoHandleAreaTrigger(AreaTriggerEntry const* areaTrigger);

    protected:
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        GuidList m_thaddiusTeslaCoilList;
        GuidList m_gothikTriggerList;
        GuidList m_zombieChowList;
        GuidList m_faerlinaFollowersList;
        GuidList m_unrelentingSideList;
        GuidList m_spectralSideList;
        GuidList m_icrecrownGuardianList;

        uint32 m_sapphironSpawnTimer;
        uint32 m_tauntTimer;
        uint8 m_horsemenKilled;
        uint32 m_livingPoisonTimer;
        uint32 m_screamsTimer;
        uint32 m_horsemenTauntTimer;
        uint32 m_despawnKTTriggerTimer;
        uint32 m_checkGuardiansTimer;
        uint32 m_shackledGuardians;

        bool isFaerlinaIntroDone;

        void JustDidDialogueStep(int32 entry) override;
};

#endif                                                      // 结束头文件防重复包含宏
