/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 卡拉赞（Karazhan）副本全局定义头文件

#ifndef DEF_KARAZHAN_H                                      // 防止头文件重复包含
#define DEF_KARAZHAN_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER                   = 11,                   // encounter 标识 ID
    MAX_OZ_OPERA_MOBS               = 4,                    // ozoperamobs 标识 ID
    MAX_BASEMENT_MINIBOSSES         = 3,                    // basementminibosses 标识 ID
    MIN_BASEMENT_MOBS               = 45,               // note: some DB provides have more spawns, up to 55. However we need to keep this compatible with all

    TYPE_ATTUMEN                    = 0,                    // 阿图门 遭遇战进度数据标识 ID
    TYPE_MOROES                     = 1,                    // 莫罗斯 遭遇战进度数据标识 ID
    TYPE_MAIDEN                     = 2,                    // maiden 遭遇战进度数据标识 ID
    TYPE_OPERA                      = 3,                    // opera 遭遇战进度数据标识 ID
    TYPE_CURATOR                    = 4,                    // 馆长 遭遇战进度数据标识 ID
    TYPE_TERESTIAN                  = 5,                    // 特雷斯坦 遭遇战进度数据标识 ID
    TYPE_ARAN                       = 6,                    // 埃兰 遭遇战进度数据标识 ID
    TYPE_NETHERSPITE                = 7,                    // 虚空幽龙 遭遇战进度数据标识 ID
    TYPE_CHESS                      = 8,                    // chess 遭遇战进度数据标识 ID
    TYPE_MALCHEZZAR                 = 9,                    // malchezzar 遭遇战进度数据标识 ID
    TYPE_NIGHTBANE                  = 10,                   // 夜之魇 遭遇战进度数据标识 ID
    TYPE_OPERA_PERFORMANCE          = 11,               // no regular encounter - just store one random opera event

    DATA_BASEMENT_EVENT             = 1,                // DO NOT CHANGE! Used by Acid. - used to check the mobs for the the basement event.

    NPC_ATTUMEN                     = 15550,                // 阿图门 Entry ID
    NPC_MIDNIGHT                    = 16151,                // 午夜 Entry ID
    NPC_MOROES                      = 15687,                // 莫罗斯 Entry ID
    NPC_BARNES                      = 16812,                // barnes Entry ID
    // NPC_TERESTIAN                = 15688,
    NPC_NIGHTBANE                   = 17225,                // 夜之魇 Entry ID
    NPC_NIGHTBANE_HELPER            = 17260,                // 夜之魇helper Entry ID
    NPC_NETHERSPITE                 = 15689,                // 虚空幽龙 Entry ID
    NPC_ECHO_MEDIVH                 = 16816,                // echomedivh Entry ID
    NPC_INVISIBLE_STALKER           = 22519,                    // placeholder for dead chess npcs
    NPC_CHESS_STATUS_BAR            = 22520,                    // npc that controlls the transformation of dead pieces
    NPC_CHESS_VICTORY_CONTROLLER    = 22524,                // chessvictorycontroller Entry ID
    // NPC_CHESS_SOUND_BUNNY        = 21921,                    // npc that handles the encounter sounds
    // NPC_WAITING_ROOM_STALKER     = 17459,                    // trigger which marks the teleport location of the players; also used to cast some control spells during the game
    NPC_SQUARE_WHITE                = 17208,                    // chess white square
    NPC_SQUARE_BLACK                = 17305,                    // chess black square
    // NPC_SQUARE_OUTSIDE_BLACK     = 17316,                    // outside chess black square
    // NPC_SQUARE_OUTSIDE_WHITE     = 17317,                    // outside chess white square
    NPC_VICTORY_DUMMY_TOOL          = 22523,                // victorydummytool Entry ID

    // Moroes event related (莫罗斯事件related)
    NPC_LADY_KEIRA_BERRYBUCK        = 17007,                // ladykeiraberrybuck Entry ID
    NPC_LADY_CATRIONA_VON_INDI      = 19872,                // ladycatrionavonindi Entry ID
    NPC_LORD_CRISPIN_FERENCE        = 19873,                // lordcrispinference Entry ID
    NPC_BARON_RAFE_DREUGER          = 19874,                // 男爵rafedreuger Entry ID
    NPC_BARONESS_DOROTHEA_MILLSTIPE = 19875,                // 男爵夫人dorotheamillstipe Entry ID
    NPC_LORD_ROBIN_DARIS            = 19876,                // lordrobindaris Entry ID

    // Opera event (Opera事件)
    NPC_DOROTHEE                    = 17535,                // dorothee Entry ID
    NPC_ROAR                        = 17546,                // roar Entry ID
    NPC_TINHEAD                     = 17547,                // tinhead Entry ID
    NPC_STRAWMAN                    = 17543,                // strawman Entry ID
    NPC_CRONE                       = 18168,                // crone Entry ID
    NPC_GRANDMOTHER                 = 17603,                // grandmother Entry ID
    NPC_JULIANNE                    = 17534,                // julianne Entry ID
    NPC_ROMULO                      = 17533,                // romulo Entry ID

    // The Master's Terrace quest related
    NPC_IMAGE_OF_MEDIVH             = 17651,                // 影像ofmedivh Entry ID
    NPC_IMAGE_OF_ARCANAGOS          = 17652,                // 影像ofarcanagos Entry ID

    // Aran Teleport npcs (埃兰传送npcs)
    NPC_TELEPORT_N                  = 17168,                // 传送n Entry ID
    NPC_TELEPORT_S                  = 17169,                // 传送s Entry ID
    NPC_TELEPORT_E                  = 17170,                // 传送e Entry ID
    NPC_TELEPORT_W                  = 17171,                // 传送w Entry ID
    NPC_TELEPORT_NE                 = 17172,                // 传送ne Entry ID
    NPC_TELEPORT_SE                 = 17173,                // 传送se Entry ID
    NPC_TELEPORT_SW                 = 17174,                // 传送sw Entry ID
    NPC_TELEPORT_NW                 = 17175,                // 传送nw Entry ID

    NPC_BLIZZARD                    = 17161,                // 暴风雪 Entry ID

    // Chess event (Chess事件)
    NPC_ORC_GRUNT                   = 17469,                    // pawn
    NPC_ORC_WOLF                    = 21748,                    // knight (骑士)
    NPC_ORC_WARLOCK                 = 21750,                    // queen
    NPC_ORC_NECROLYTE               = 21747,                    // bishop
    NPC_SUMMONED_DAEMON             = 21726,                    // rook
    NPC_WARCHIEF_BLACKHAND          = 21752,                    // king
    NPC_HUMAN_FOOTMAN               = 17211,                    // pawn
    NPC_HUMAN_CHARGER               = 21664,                    // knight (骑士)
    NPC_HUMAN_CONJURER              = 21683,                    // queen
    NPC_HUMAN_CLERIC                = 21682,                    // bishop
    NPC_CONJURED_WATER_ELEMENTAL    = 21160,                    // rook
    NPC_KING_LLANE                  = 21684,                    // king

    // Servants' Quarters event
    NPC_BAT_MINIBOSS                = 16180,                    // Shadikith the Glider
    NPC_DOG_MINIBOSS                = 16181,                    // Rokad the Ravager
    NPC_SPIDER_MINIBOSS             = 16179,                    // Hyakiss the Lurker

    NPC_SHADOWBAT                   = 16173,                // shadowbat Entry ID
    NPC_GREATER_SHADOWBAT           = 16174,                // greatershadowbat Entry ID
    NPC_VAMPIRIC_SHADOWBAT          = 16175,                // vampiricshadowbat Entry ID
    NPC_PHASE_HOUND                 = 16178,                // 阶段hound Entry ID
    NPC_DREADBEAST                  = 16177,                // dreadbeast Entry ID
    NPC_SHADOWBEAST                 = 16176,                // shadowbeast Entry ID
    NPC_COLDMIST_STALKER            = 16170,                // coldmiststalker Entry ID
    NPC_COLDMIST_WIDOW              = 16171,                // coldmist黑女巫 Entry ID

    // prince malchezaar (prince玛克扎尔)
    NPC_INFERNAL_RELAY              = 17645,                // infernalrelay Entry ID
    NPC_INFERNAL_TARGET             = 17644,                // infernal目标 Entry ID
    NPC_INFERNAL                    = 17646,                // infernal Entry ID

    SPELL_HELLFIRE                  = 30859, // Infernal damage aura (Infernal伤害光环)

    GO_STAGE_CURTAIN                = 183932,               // stagecurtain 游戏对象 Entry ID
    GO_STAGE_DOOR_LEFT              = 184278,               // stage门left 游戏对象 Entry ID
    GO_STAGE_DOOR_RIGHT             = 184279,               // stage门right 游戏对象 Entry ID
    GO_PRIVATE_LIBRARY_DOOR         = 184517,               // privatelibrary门 游戏对象 Entry ID
    GO_MASSIVE_DOOR                 = 185521,               // massive门 游戏对象 Entry ID
    GO_GAMESMANS_HALL_DOOR          = 184276,               // gamesmans大厅门 游戏对象 Entry ID
    GO_GAMESMANS_HALL_EXIT_DOOR     = 184277,               // gamesmans大厅exit门 游戏对象 Entry ID
    GO_NETHERSPACE_DOOR             = 185134,               // netherspace门 游戏对象 Entry ID
    GO_SIDE_ENTRANCE_DOOR           = 184275,               // sideentrance门 游戏对象 Entry ID
    GO_SERVANTS_ACCESS_DOOR         = 184281,               // servantsaccess门 游戏对象 Entry ID
    GO_DUST_COVERED_CHEST           = 185119,               // dustcovered宝箱 游戏对象 Entry ID
    GO_MASTERS_TERRACE_DOOR_1       = 184274,               // mastersterrace门1 游戏对象 Entry ID
    GO_MASTERS_TERRACE_DOOR_2       = 184280,               // mastersterrace门2 游戏对象 Entry ID
    GO_BLACKENED_URN                = 194092,               // blackenedurn 游戏对象 Entry ID

    // Opera event stage decoration
    GO_OZ_BACKDROP                  = 183442,               // ozbackdrop 游戏对象 Entry ID
    GO_OZ_HAY                       = 183496,               // ozhay 游戏对象 Entry ID
    GO_HOOD_BACKDROP                = 183491,               // hoodbackdrop 游戏对象 Entry ID
    GO_HOOD_TREE                    = 183492,               // hoodtree 游戏对象 Entry ID
    GO_HOOD_HOUSE                   = 183493,               // hoodhouse 游戏对象 Entry ID
    GO_RAJ_BACKDROP                 = 183443,               // rajbackdrop 游戏对象 Entry ID
    GO_RAJ_MOON                     = 183494,               // rajmoon 游戏对象 Entry ID
    GO_RAJ_BALCONY                  = 183495,               // rajbalcony 游戏对象 Entry ID
    GO_CHESSBOARD                   = 185324,               // chessboard 游戏对象 Entry ID

    // Chess event spells
    SPELL_CLEAR_BOARD               = 37366,                    // spell cast to clear the board at the end of the event
    SPELL_GAME_IN_SESSION           = 39331,                    // debuff on players received while the game is in session
    SPELL_FORCE_KILL_BUNNY          = 45260,                    // triggers 45259
    SPELL_GAME_OVER                 = 39401,                    // cast by Medivh on game end (由 Medivh on game end 施放)
    SPELL_BOARD_VISUAL              = 39390,                // board视觉特效 法术 ID
    SPELL_VICTORY_VISUAL            = 39395,                    // cast by the Victory controller on game end (由 the Victory controller on game end 施放)

    FACTION_ID_CHESS_HORDE          = 1689,                 // IDchesshorde 阵营 ID
    FACTION_ID_CHESS_ALLIANCE       = 1690,                 // IDchessalliance 阵营 ID

    EMOTE_BAT_SPAWN                 = -1532133,             // bat生成 表情动作 ID
    EMOTE_DOG_SPAWN                 = -1532134,             // dog生成 表情动作 ID
    EMOTE_SPIDER_SPAWN              = -1532135,             // spider生成 表情动作 ID
    EMOTE_SPIDER_SPAWN2             = -1532138,             // spiderspawn2 表情动作 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum OperaEvents
{
    OPERA_EVENT_WIZARD_OZ           = 1,                    // 事件wizardoz 标识 ID
    OPERA_EVENT_RED_RIDING_HOOD     = 2,                    // 事件redridinghood 标识 ID
    OPERA_EVENT_ROMULO_AND_JUL      = 3                     // 事件romuloandjul 标识 ID
};

struct BasementSpawns
{
    uint32 uiEntry;
    int32 emote1;
    int32 emote2;
    float fX, fY, fZ, fO;
};

static const BasementSpawns aBasementEnum[MAX_BASEMENT_MINIBOSSES] =
{
    {NPC_BAT_MINIBOSS,    EMOTE_BAT_SPAWN, 0,   -10959.11f, -1940.86f, 46.19f, 3.769f},
    {NPC_DOG_MINIBOSS,    EMOTE_DOG_SPAWN, 0,   -10900.01f, -2085.47f, 49.55f, 1.343f},
    {NPC_SPIDER_MINIBOSS, EMOTE_SPIDER_SPAWN, EMOTE_SPIDER_SPAWN2, -10939.75f, -2041.11f, 49.55f, 1.361f},
};

struct OperaSpawns
{
    uint32 uiEntry;
    float fX, fY, fZ, fO;
};

static const OperaSpawns aOperaLocOz[MAX_OZ_OPERA_MOBS] =
{
    { NPC_DOROTHEE,  -10896.65f, -1757.62f, 90.55f, 4.86f },
    { NPC_ROAR,      -10889.53f, -1758.10f, 90.55f, 4.57f },
    { NPC_TINHEAD,   -10883.84f, -1758.85f, 90.55f, 4.53f },
    { NPC_STRAWMAN,  -10902.11f, -1756.45f, 90.55f, 4.66f },
};

static const OperaSpawns aOperaLocWolf = {NPC_GRANDMOTHER, -10892.01f, -1758.01f, 90.55f, 4.73f};
static const OperaSpawns aOperaLocJul  = {NPC_JULIANNE,    -10893.56f, -1760.43f, 90.55f, 4.55f};

static const float afChroneSpawnLoc[4] = { -10893.11f, -1757.85f, 90.55f, 4.60f};

// 副本进度与状态机管理类：标识 ID（instance_karazhan）
class instance_karazhan : public ScriptedInstance
{
    public:
        instance_karazhan(Map* pMap);
        ~instance_karazhan() {}

        // 初始化副本数据结构
        void Initialize() override;
        bool IsEncounterInProgress() const override;

        // 玩家进入副本区域回调
        void OnPlayerEnter(Player* pPlayer) override;
        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        void OnCreatureRespawn(Creature* creature) override;

        void OnCreatureEvade(Creature* creature) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        void SetData64(uint32 uiData, uint64 uiGuid) override;

        void DoPrepareOperaStage(Creature* pOrganizer);

        uint32 GetPlayerTeam() const { return m_uiTeam; }
        bool IsFriendlyGameReady() const { return m_bFriendlyGame; }
        void DoMoveChessPieceToSides(uint32 uiSpellId, uint32 uiFaction, bool bGameEnd = false);
        void DoFailChessEvent();
        void DoFinishChessEvent();
        void GetChessPiecesByFaction(GuidList& lList, uint32 uiFaction) { lList = uiFaction == FACTION_ID_CHESS_ALLIANCE ? m_lChessPiecesAlliance : m_lChessPiecesHorde; }

        GuidVector& GetAranTeleportNPCs() { return m_aranTeleportNPCs; }

        void GetNightbaneTriggers(GuidList& lList, bool bGround) { lList = bGround ? m_lNightbaneGroundTriggers : m_lNightbaneAirTriggers; }

        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;
        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        GuidVector m_vInfernalRelays;

    private:
        void DoPrepareChessEvent();

        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        uint32 m_uiOperaEvent;
        uint32 m_uiOzDeathCount;
        uint32 m_uiTeam;                                    // Team of first entered player, used for the Chess event
        uint32 m_uiChessResetTimer;                         // ChessReset 技能计时器
        uint32 m_uiChessEndingTimer;                        // ChessEnding 技能计时器
        uint32 m_uiVictoryControllerTimer;                  // VictoryController 技能计时器
        uint32 m_uiVictoryToolTimers[8];

        uint8 m_uiAllianceStalkerCount;
        uint8 m_uiHordeStalkerCount;

        bool m_bFriendlyGame;                               // FriendlyGame 状态标记
        bool m_bBasementBossReady;                          // BasementBossReady 状态标记
        bool m_uiVictoryTimersPhase[8];

        ObjectGuid m_HordeStatusGuid;                       // HordeStatus 实体 GUID
        ObjectGuid m_AllianceStatusGuid;                    // AllianceStatus 实体 GUID

        GuidList m_lOperaTreeGuidList;
        GuidList m_lOperaHayGuidList;
        GuidList m_lNightbaneGroundTriggers;
        GuidList m_lNightbaneAirTriggers;
        GuidSet m_sBasementMobsSet;                         // sBasementMobsSet GUID 集合

        GuidList m_lChessHordeStalkerList;
        GuidList m_lChessAllianceStalkerList;
        GuidList m_lChessPiecesAlliance;
        GuidList m_lChessPiecesHorde;
        GuidVector m_vHordeStalkers;
        GuidVector m_vAllianceStalkers;
        GuidVector m_vVictoryDummyTools;
        GuidVector m_vChessSquares;
        GuidVector m_aranTeleportNPCs;
};

#endif                                                      // 结束头文件防重复包含宏
