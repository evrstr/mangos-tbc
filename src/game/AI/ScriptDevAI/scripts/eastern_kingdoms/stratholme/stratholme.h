/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
* This program is free software licensed under GPL version 2
* Please see the included DOCS/LICENSE.TXT for more information */

// 斯坦索姆（Stratholme）全局定义头文件

#ifndef DEF_STRATHOLME_H                                    // 防止头文件重复包含
#define DEF_STRATHOLME_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER               = 11,                       // encounter 标识 ID
    MAX_SILVERHAND              = 5,                        // silverhand 标识 ID
    MAX_ZIGGURATS               = 3,                        // ziggurats 标识 ID

    TYPE_BARON_RUN              = 0,                        // 男爵run 遭遇战进度数据标识 ID
    TYPE_BARONESS               = 1,                        // 男爵夫人 遭遇战进度数据标识 ID
    TYPE_NERUB                  = TYPE_BARONESS + 1,        // Assert that these three TYPEs are in correct order.
    TYPE_PALLID                 = TYPE_BARONESS + 2,
    TYPE_RAMSTEIN               = 4,                        // ramstein 遭遇战进度数据标识 ID
    TYPE_BARON                  = 5,                        // 男爵 遭遇战进度数据标识 ID
    TYPE_BARTHILAS_RUN          = 6,                        // barthilasrun 遭遇战进度数据标识 ID
    TYPE_AURIUS                 = 7,                        // aurius 遭遇战进度数据标识 ID
    TYPE_BLACK_GUARDS           = 8,                        // blackguards 遭遇战进度数据标识 ID
    TYPE_POSTMASTER             = 9,                        // postmaster 遭遇战进度数据标识 ID
    TYPE_TRUE_MASTERS           = 10,                       // truemasters 遭遇战进度数据标识 ID

    NPC_TIMMY_THE_CRUEL         = 10808,                    // timmythecruel Entry ID
    NPC_BARTHILAS               = 10435,                    // barthilas Entry ID
    NPC_BARONESS_ANASTARI       = 10436,                    // 男爵夫人安娜丝塔丽 Entry ID
    NPC_NERUBENKAN              = 10437,                    // nerubenkan Entry ID
    NPC_MALEKI_THE_PALLID       = 10438,                    // 玛勒基thepallid Entry ID
    NPC_RAMSTEIN                = 10439,                    // ramstein Entry ID
    NPC_BARON                   = 10440,                    // 男爵 Entry ID
    NPC_BALNAZZAR               = 10813,                    // 巴纳扎尔 Entry ID
    NPC_CRYSTAL                 = 10415,                    // Three ziggurat crystals
    NPC_THUZADIN_ACOLYTE        = 10399,                    // Acolytes in ziggurats
    NPC_ABOM_BILE               = 10416,                    // abombile Entry ID
    NPC_ABOM_VENOM              = 10417,                    // abomvenom Entry ID
    NPC_MINDLESS_UNDEAD         = 11030,                    // Zombies summoned after Ramstein
    NPC_BLACK_GUARD             = 10394,                    // Zombies summoned after Ramstein
    NPC_YSIDA                   = 16031,                    // ysida Entry ID
    // NPC_YSIDA_TRIGGER        = 16100,
    NPC_CRIMSON_INITIATE        = 10420,                    // A couple of them related to spawn Timmy
    NPC_CRIMSON_GALLANT         = 10424,                    // crimsongallant Entry ID
    NPC_CRIMSON_GUARDSMAN       = 10418,                    // crimsonguardsman Entry ID
    NPC_CRIMSON_CONJURER        = 10419,                    // crimsonconjurer Entry ID
    NPC_CRIMSON_MONK            = 11043,                    // crimsonmonk Entry ID
    NPC_CRIMSON_SORCERER        = 10422,                    // crimson巫师 Entry ID
    NPC_CRIMSON_BATTLE_MAGE     = 10425,                    // crimsonbattle法师 Entry ID
    NPC_AURIUS                  = 10917,                    // aurius Entry ID
    NPC_UNDEAD_POSTMAN          = 11142,                    // undeadpostman Entry ID
    NPC_SKELETAL_GUARDIAN       = 10390,                    // skeletalguardian Entry ID
    NPC_SKELETAL_BERSERKER      = 10391,                    // skeletalberserker Entry ID
    NPC_GREGOR_THE_JUSTICIAR    = 17910,                    // related to quest "True Masters of the Light"
    NPC_CATHELA_THE_SEEKER      = 17911,                    // cathelatheseeker Entry ID
    NPC_NEMAS_THE_ARBITER       = 17912,                    // nemasthearbiter Entry ID
    NPC_AELMAR_THE_VANQUISHER   = 17913,                    // aelmarthevanquisher Entry ID
    NPC_VICAR_HYERONIMUS        = 17914,                    // vicarhyeronimus Entry ID
    NPC_PALADIN_QUEST_CREDIT    = 17915,                    // 圣骑士任务credit Entry ID
    NPC_PLAGUED_RAT             = 10441,                    // 疫病rat Entry ID
    NPC_PLAGUED_INSECT          = 10461,                    // 疫病insect Entry ID
    NPC_PLAGUED_MAGGOT          = 10536,                    // 疫病蛆虫 Entry ID

    GO_SERVICE_ENTRANCE         = 175368,                   // serviceentrance 游戏对象 Entry ID
    GO_GAUNTLET_GATE1           = 175357,                   // gauntletgate1 游戏对象 Entry ID
    GO_PORT_SLAUGHTER_GATE      = 175358,                   // Port used at the undead event
    GO_ZIGGURAT_DOOR_1          = 175380,                   // Baroness (男爵夫人)
    GO_ZIGGURAT_DOOR_2          = 175379,                   // Nerub'enkan
    GO_ZIGGURAT_DOOR_3          = 175381,                   // Maleki (玛勒基)
    GO_ZIGGURAT_DOOR_4          = 175405,                   // Ramstein
    GO_ZIGGURAT_DOOR_5          = 175796,                   // Baron (男爵)
    GO_PORT_GAUNTLET            = 175374,                   // Port from gauntlet to slaughter
    GO_PORT_SLAUGHTER           = 175373,                   // Port at slaughter
    GO_PORT_ELDERS              = 175377,                   // Port at elders square
    GO_YSIDA_CAGE               = 181071,                   // Cage to open after baron event is done
    GO_PORT_TRAP_GATE_1         = 175351,                   // Portcullis used in the gate traps (rats trap)
    GO_PORT_TRAP_GATE_2         = 175350,                   // Scarlet side
    GO_PORT_TRAP_GATE_3         = 175355,                   // Undead side
    GO_PORT_TRAP_GATE_4         = 175354,                   // porttrap门禁4 游戏对象 Entry ID

    FACTION_VICTIM              = 42,                       // victim 阵营 ID

    QUEST_DEAD_MAN_PLEA         = 8945,                     // deadmanplea 任务 ID
    QUEST_MEDALLION_FAITH       = 5122,                     // medallionfaith 任务 ID

    SPELL_BARON_ULTIMATUM       = 27861,                    // Aura for player during the run (visual icon)
    SPELL_BARON_SOUL_DRAIN      = 27640,                    // Used by the Baron to kill Ysida
    SPELL_YSIDA_FREED           = 27773,                    // Argent Dawn extra-reputation reward on successful Baron run
    SPELL_SUMMON_POSTMASTER     = 24627,                    // 召唤postmaster 法术 ID
    SPELL_SUICIDE               = 7,                        // Used by crystals

    SAY_ANNOUNCE_ZIGGURAT_1     = -1329004,                 // announceziggurat1 台词广播 ID
    SAY_ANNOUNCE_ZIGGURAT_2     = -1329005,                 // announceziggurat2 台词广播 ID
    SAY_ANNOUNCE_ZIGGURAT_3     = -1329006,                 // announceziggurat3 台词广播 ID
    SAY_ANNOUNCE_RIVENDARE      = -1329007,                 // announcerivendare 台词广播 ID

    SAY_WARN_BARON              = -1329008,                 // warn男爵 台词广播 ID
    SAY_ANNOUNCE_RUN_START      = -1329009,                 // announcerun开始 台词广播 ID
    SAY_ANNOUNCE_RUN_10_MIN     = -1329010,                 // announcerun10min 台词广播 ID
    SAY_ANNOUNCE_RUN_5_MIN      = -1329011,                 // announcerun5min 台词广播 ID
    SAY_YSIDA_RUN_5_MIN         = -1329019,                 // ysidarun5min 台词广播 ID
    SAY_ANNOUNCE_RUN_FAIL       = -1329012,                 // announcerunfail 台词广播 ID
    SAY_YSIDA_RUN_FAIL          = -1329020,                 // ysidarunfail 台词广播 ID
    SAY_ANNOUNCE_RAMSTEIN       = -1329013,                 // announceramstein 台词广播 ID
    SAY_UNDEAD_DEFEAT           = -1329014,                 // undeaddefeat 台词广播 ID
    SAY_EPILOGUE                = -1329015,                 // epilogue 台词广播 ID

    SAY_AURIUS_AGGRO            = -1329021,                 // aurius开战 台词广播 ID
    SAY_AURIUS_DEATH            = -1329022,                 // aurius死亡 台词广播 ID

    // Used in the Scarlet Bastion defense events
    YELL_BASTION_BARRICADE      = -1329023,                 // bastionbarricade 大喊台词广播 ID
    YELL_BASTION_STAIRS         = -1329024,                 // bastionstairs 大喊台词广播 ID
    YELL_BASTION_ENTRANCE       = -1329025,                 // bastionentrance 大喊台词广播 ID
    YELL_BASTION_HALL_LIGHTS    = -1329026,                 // bastion大厅lights 大喊台词广播 ID
    YELL_BASTION_INNER_1        = -1329027,                 // bastioninner1 大喊台词广播 ID
    YELL_BASTION_INNER_2        = -1329028,                 // bastioninner2 大喊台词广播 ID

    // Jarien and Sothos
    SPELL_SUMMON_WINNER_BOX     = 27645,                    // 召唤winnerbox 法术 ID
    NPC_JARIEN                  = 16101,                    // jarien Entry ID
    NPC_SOTHOS                  = 16102,                    // sothos Entry ID
};

static const uint32 gates[] =
{
    GO_PORT_TRAP_GATE_1, GO_PORT_TRAP_GATE_2, GO_PORT_TRAP_GATE_3, GO_PORT_TRAP_GATE_4
};

static const uint32 plaguedCritters[] =
{
    NPC_PLAGUED_RAT, NPC_PLAGUED_MAGGOT, NPC_PLAGUED_INSECT
};

static const Position stratholmeLocation[] =
{
    {3725.577f, -3599.484f, 142.367f, 0.0},                 // Barthilas door run
    {4068.284f, -3535.678f, 122.771f, 2.50f},               // Barthilas tele
    {4032.643f, -3378.546f, 119.752f, 4.74f},               // Ramstein summon loc
    {4032.843f, -3390.246f, 119.732f, 0.0},                 // Ramstein move loc
    {3969.357f, -3391.871f, 119.116f, 5.91f},               // Skeletons summon loc
    {4033.044f, -3431.031f, 119.055f, 0.0},                 // Skeletons move loc
    {4032.602f, -3378.506f, 119.752f, 4.74f},               // Guards summon loc
    {4044.78f,  -3333.68f,  115.53f,  4.15f},               // Ysida summon loc
    {4041.9f,   -3337.6f,   115.06f,  3.82f}                // Ysida move/death loc
};

// Scarlet Bastion defense events
enum
{
    BARRICADE           = 0,                                // 标识 ID
    STAIRS              = 1,                                // 标识 ID
    TIMMY               = 2,                                // 标识 ID
    ENTRANCE            = 3,                                // 标识 ID
    HALL_OF_LIGHTS      = 4,                                // oflights 标识 ID
    INNER_BASTION_1     = 5,                                // bastion1 标识 ID
    INNER_BASTION_2     = 6,                                // bastion2 标识 ID
    CRIMSON_THRONE      = 7,                                // throne 标识 ID
    FIRST_BARRICADES    = 7,                                // barricades 标识 ID

    MAX_DEFENSE_POINTS  = 8                                 // defensepoints 标识 ID
};

static const int ScarletEventYells[] =
{
    YELL_BASTION_BARRICADE, YELL_BASTION_STAIRS, 0, YELL_BASTION_ENTRANCE,
    YELL_BASTION_HALL_LIGHTS, YELL_BASTION_INNER_1, YELL_BASTION_INNER_2
};

static const Position defensePoints[] =
{
    {3662.84f, -3175.15f, 126.559f, 0.0},                        // Last barricade before the Scarlet Bastion (Courtyard)
    {3661.57f, -3157.80f, 128.945f, 0.0},                        // Scarlet Bastion stairs (Courtyard)
    {3668.60f, -3183.31f, 126.215f, 0.0},                        // Courtyard mobs sort point (Timmy)
    {3646.49f, -3072.84f, 134.207f, 0.0},                        // Scarlet Bastion entrance corridor
    {3599.28f, -3107.91f, 134.204f, 0.0},                        // Hall of Lights
    {3485.98f, -3087.02f, 135.080f, 0.0},                        // Inner Bastion: first corridor
    {3436.74f, -3090.19f, 135.085f, 0.0},                        // Inner Bastion: Second corridor
    {3661.89f, -3192.89f, 126.691f, 0.0}                         // Barricades before the last one (Courtyard): in last position for conveniently iterate over the table
};

static const Position scarletGuards[] =               // Spawned when players cleared some parts of the Bastion
{
    {3598.213f, -3094.812f, 135.657f, 5.3425f},             // Hall of Lights
    {3602.198f, -3096.243f, 134.120f, 0.0},
    {3598.152f, -3098.927f, 134.120f, 0.0},
    {3432.967f, -3069.643f, 136.529f, 5.3425f},             // Inner Bastion
    {3441.795f, -3077.431f, 135.000f, 0.0},
    {3437.445f, -3080.316f, 135.000f, 0.0}
};

static const Position scourgeInvaders[] =             // Attack when players cleared some parts of the Bastion
{
    {3614.702f, -3187.642f, 131.406f, 4.024f},              // Timmy. TIMMYYYYYYY !!!
    {3647.36f, -3139.70f, 134.78f, 2.1962f},                // Entrance
    {3642.50f, -3106.24f, 314.18f, 0.5210f},
    {3547.85f, -3076.46f, 135.00f, 2.2269f},                // Inner Bastion
    {3512.21f, -3066.85f, 135.00f, 3.7136f},
    {3492.44f, -3077.72f, 135.00f, 2.1680f},                // Crimson Throne
    {3443.18f, -3083.90f, 135.01f, 2.1563f}
};

static const Position scarletLastStand[] =            // Positions remaining Scarlet defenders will move to if the position is lost
{
    {3658.43f, -3178.07f, 126.696f, 5.23599f},              // Last barricade
    {3665.62f, -3173.88f, 126.551f, 5.02655f},
    {3662.84f, -3175.15f, 126.559f, 5.11381f},
    {3656.91f, -3161.94f, 128.359f, 5.39307f},              // Bastion stairs
    {3664.29f, -3157.06f, 128.357f, 5.18363f},
    {3661.57f, -3157.80f, 128.945f, 5.23599f}
};

static const Position gateTrap[] =                    // Positions of the two Gate Traps
{
    {3612.29f, -3335.39f, 124.077f, 3.14159f},              // Scarlet side
    {3919.88f, -3547.34f, 134.269f, 2.94961f}               // Undead side
};

struct ZigguratStore
{
    ObjectGuid m_doorGuid;                                  // door 实体 GUID
    ObjectGuid m_crystalGuid;                               // crystal 实体 GUID
    GuidList m_zigguratAcolyteGuid;
};

// 副本进度与状态机管理类：标识 ID（instance_stratholme）
class instance_stratholme : public ScriptedInstance
{
    public:
        instance_stratholme(Map* map);
        ~instance_stratholme() {}

        // 初始化副本数据结构
        void Initialize() override;

        // 玩家进入副本区域回调
        void OnPlayerEnter(Player* player) override;

        // 副本内生物创建加载回调
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

        bool CheckConditionCriteriaMeet(Player const* player, uint32 instanceConditionId, WorldObject const* conditionSource, uint32 conditionSourceType) const override;

        void OnCreatureEnterCombat(Creature* creature) override;
        void OnCreatureEvade(Creature* creature) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* creature) override;
        void OnCreatureRespawn(Creature* creature) override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

    protected:
        bool StartSlaughterSquare();
        void DoSortZiggurats();
        void DoOpenSlaughterhouseDoor(bool open);
        void ThazudinAcolyteJustDied(Creature* creature);

        void DoSpawnScarletGuards(uint8 step, Player* summoner);
        void DoSpawnScourgeInvaders(uint8 step, Player* summoner);
        void DoMoveBackDefenders(uint8 step, Creature* creature);
        void DoScarletBastionDefense(uint8 step, Creature* creature);

        void DoGateTrap(uint8 gate);
        void DoSpawnPlaguedCritters(uint8 gate, Player* player);
        uint32 m_gateTrapTimers[2][3];

        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        uint32 m_baronRunTimer;
        uint32 m_barthilasRunTimer;
        uint32 m_mindlessSummonTimer;
        uint32 m_slaughterSquareTimer;
        uint32 m_slaughterDoorTimer;
        uint32 m_blackGuardsTimer;
        uint32 m_auriusSummonTimer;
        bool m_isSlaughterDoorOpen;

        uint32 m_yellCounter;
        uint32 m_mindlessCount;
        uint8 m_postboxesUsed;
        uint8 m_silverHandKilled;

        bool m_jarienKilled;
        bool m_sothosKilled;

        ZigguratStore m_zigguratStorage[MAX_ZIGGURATS];

        GuidList m_crimsonDefendersLowGuids[MAX_DEFENSE_POINTS];
        GuidList m_crystalGUIDs;
        GuidSet m_abominationGUIDs;                         // abominationGUIDs GUID 集合
        GuidList m_acolyteGUIDs;
        GuidList m_undeadGUIDs;
        GuidList m_guardGUIDs;
};

#endif                                                      // 结束头文件防重复包含宏
