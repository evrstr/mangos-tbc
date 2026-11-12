/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 祖阿曼（Zul'Aman）全局定义头文件

#ifndef DEF_ZULAMAN_H                                       // 防止头文件重复包含
#define DEF_ZULAMAN_H

#include "World/WorldStateDefines.h"

// 枚举定义：法术技能、生物与环境事件标识
enum InstanceZA
{
    MAX_ENCOUNTER           = 13,                           // encounter 标识 ID
    MAX_VENDOR              = 2,                            // vendor 标识 ID
    MAX_CHESTS              = 4,                            // chests 标识 ID
    MAX_BEAR_WAVES          = 4,                            // bearwaves 标识 ID

    SAY_INST_RELEASE        = -1568067,                     // TODO Event NYI
    SAY_INST_BEGIN          = -1568068,                     // instbegin 台词广播 ID
    SAY_INST_PROGRESS_1     = -1568069,                     // instprogress1 台词广播 ID
    SAY_INST_PROGRESS_2     = -1568070,                     // instprogress2 台词广播 ID
    SAY_INST_PROGRESS_3     = -1568071,                     // instprogress3 台词广播 ID
    SAY_INST_WARN_1         = -1568072,                     // instwarn1 台词广播 ID
    SAY_INST_WARN_2         = -1568073,                     // instwarn2 台词广播 ID
    SAY_INST_WARN_3         = -1568074,                     // instwarn3 台词广播 ID
    SAY_INST_WARN_4         = -1568075,                     // instwarn4 台词广播 ID
    SAY_INST_SACRIF1        = -1568076,                     // instsacrif1 台词广播 ID
    SAY_INST_SACRIF2        = -1568077,                     // instsacrif2 台词广播 ID
    SAY_INST_COMPLETE       = -1568078,                     // instcomplete 台词广播 ID

    // Bear event yells
    SAY_WAVE1_AGGRO         = -1568010,                     // wave1开战 台词广播 ID
    SAY_WAVE2_STAIR1        = -1568011,                     // wave2stair1 台词广播 ID
    SAY_WAVE3_STAIR2        = -1568012,                     // wave3stair2 台词广播 ID
    SAY_WAVE4_PLATFORM      = -1568013,                     // wave4platform 台词广播 ID
    SAY_RUN_BACK            = -1568171,                     // runback 台词广播 ID

    // Zul'jin intro yell (祖尔金开场大喊)
    SAY_INTRO               = -1568056,                     // 开场 台词广播 ID

    TYPE_EVENT_RUN          = 0,                            // 事件run 遭遇战进度数据标识 ID
    TYPE_AKILZON            = 1,                            // 埃基尔松 遭遇战进度数据标识 ID
    TYPE_NALORAKK           = 2,                            // 纳洛拉克 遭遇战进度数据标识 ID
    TYPE_JANALAI            = 3,                            // 埃加拉什 遭遇战进度数据标识 ID
    TYPE_HALAZZI            = 4,                            // 哈尔拉兹 遭遇战进度数据标识 ID
    TYPE_MALACRASS          = 5,                            // 玛拉卡斯 遭遇战进度数据标识 ID
    TYPE_ZULJIN             = 6,                            // 祖尔金 遭遇战进度数据标识 ID
    TYPE_RUN_EVENT_TIME     = 7,                            // run事件time 遭遇战进度数据标识 ID

    TYPE_TANZAR             = 8,                            // tanzar 遭遇战进度数据标识 ID
    TYPE_KRAZ               = 9,                            // kraz 遭遇战进度数据标识 ID
    TYPE_ASHLI              = 10,                           // ashli 遭遇战进度数据标识 ID
    TYPE_HARKOR             = 11, // Must be MAX_ENCOUNTER -1

    TYPE_AKILZON_GAUNTLET   = 12,                           // 埃基尔松gauntlet 遭遇战进度数据标识 ID

    TYPE_RAND_VENDOR_1      = 13,                           // randvendor1 遭遇战进度数据标识 ID
    TYPE_RAND_VENDOR_2      = 14,                           // randvendor2 遭遇战进度数据标识 ID

    NPC_AKILZON             = 23574,                        // 埃基尔松 Entry ID
    NPC_NALORAKK            = 23576,                        // 纳洛拉克 Entry ID
    NPC_JANALAI             = 23578,                        // 埃加拉什 Entry ID
    NPC_HALAZZI             = 23577,                        // 哈尔拉兹 Entry ID
    NPC_MALACRASS           = 24239,                        // 玛拉卡斯 Entry ID
    NPC_ZULJIN              = 23863,                        // 祖尔金 Entry ID

    // Narolakk event npcs
    NPC_MEDICINE_MAN        = 23581,                        // medicineman Entry ID
    NPC_TRIBES_MAN          = 23582,                        // tribesman Entry ID
    NPC_AXETHROWER          = 23542,                        // axethrower Entry ID
    NPC_WARBRINGER          = 23580,                        // 战争使者 Entry ID

    // Akil'zon gauntlet npcs
    NPC_TEMPEST             = 24549,                        // tempest Entry ID
    NPC_LOOKOUT             = 24175,                        // lookout Entry ID
    NPC_PROTECTOR           = 24180,                        // protector Entry ID
    NPC_WIND_WALKER         = 24179,                        // windwalker Entry ID

	// Janalai npcs (埃加拉什npcs)
	NPC_HATCHLING           = 23598,                           // hatchling Entry ID
    NPC_DRAGONHAWK_EGG      = 23817,                        // dragonhawk龙蛋 Entry ID
    NPC_WORLD_TRIGGER_NOT_IMMUNE_PC = 21252,                // world触发notimmunepc Entry ID

    // Malacrass companions (玛拉卡斯companions)
    NPC_ALYSON              = 24240,                        // alyson Entry ID
    NPC_THURG               = 24241,                        // thurg Entry ID
    NPC_SLITHER             = 24242,                        // slither Entry ID
    NPC_RADAAN              = 24243,                        // radaan Entry ID
    NPC_GAZAKROTH           = 24244,                        // gazakroth Entry ID
    NPC_FENSTALKER          = 24245,                        // fenstalker Entry ID
    NPC_DARKHEART           = 24246,                        // darkheart Entry ID
    NPC_KORAGG              = 24247,                        // koragg Entry ID

    NPC_HARRISON            = 24358,                        // harrison Entry ID
    NPC_AMANISHI_GUARDIAN   = 23597,                        // amanishiguardian Entry ID
    // Time Run Event NPCs
    NPC_TANZAR              = 23790,                        // at bear
    NPC_KRAZ                = 24024,                        // at phoenix
    NPC_ASHLI               = 24001,                        // at lynx
    NPC_HARKOR              = 23999,                        // at eagle
    // unused (TODO or TODO with DB-tools)
    NPC_TANZAR_CORPSE       = 24442,                        // tanzar尸体 Entry ID
    NPC_KRAZ_CORPSE         = 24444,                        // kraz尸体 Entry ID
    NPC_ASHIL_CORPSE        = 24441,                        // ashil尸体 Entry ID
    NPC_HARKOR_CORPSE       = 24443,                        // harkor尸体 Entry ID

    NPC_EXTERIOR_INVISMAN   = 23746,                        // exteriorinvisman Entry ID

    // Zul'jin event spirits (祖尔金事件spirits)
    NPC_BEAR_SPIRIT         = 23878,                        // They should all have aura 42466
    NPC_EAGLE_SPIRIT        = 23880,                        // eagle之魂 Entry ID
    NPC_LYNX_SPIRIT         = 23877,                        // lynx之魂 Entry ID
    NPC_DRAGONHAWK_SPIRIT   = 23879,                        // dragonhawk之魂 Entry ID

    GO_STRANGE_GONG         = 187359,                       // strangegong 游戏对象 Entry ID
    GO_MASSIVE_GATE         = 186728,                       // massive门禁 游戏对象 Entry ID
    GO_WIND_DOOR            = 186858,                       // wind门 游戏对象 Entry ID
    GO_LYNX_TEMPLE_ENTRANCE = 186304,                       // lynxtempleentrance 游戏对象 Entry ID
    GO_LYNX_TEMPLE_EXIT     = 186303,                       // lynxtempleexit 游戏对象 Entry ID
    GO_HEXLORD_ENTRANCE     = 186305,                       // hexlordentrance 游戏对象 Entry ID
    GO_WOODEN_DOOR          = 186306,                       // wooden门 游戏对象 Entry ID
    GO_FIRE_DOOR            = 186859,                       // 火焰门 游戏对象 Entry ID

    GO_TANZARS_TRUNK        = 186648,                       // tanzarstrunk 游戏对象 Entry ID
    GO_KRAZS_PACKAGE        = 186667,                       // krazspackage 游戏对象 Entry ID
    GO_ASHLIS_BAG           = 186672,                       // ashlisbag 游戏对象 Entry ID
    GO_HARKORS_SATCHEL      = 187021,                       // harkorssatchel 游戏对象 Entry ID

    GO_GOLD_COINS_1         = 186633,                       // goldcoins1 游戏对象 Entry ID
    GO_GOLD_COINS_2         = 186634,                       // goldcoins2 游戏对象 Entry ID

    GO_HARKORS_CAGE         = 187378,                       // harkorscage 游戏对象 Entry ID
    GO_DWARF_HAMMER         = 186623,                       // 矮人hammer 游戏对象 Entry ID
    GO_DWARF_LOOT_BOX       = 186622,                       // 矮人lootbox 游戏对象 Entry ID
    GO_HARKORS_BREW_KEG     = 186748,                       // harkors美酒酒桶 游戏对象 Entry ID

    GO_TANZARS_CAGE         = 187377,                       // tanzarscage 游戏对象 Entry ID
    GO_AMANI_DRUM           = 186865,                       // amanidrum 游戏对象 Entry ID

    GO_KRAZS_CHEST          = 186658,                       // krazs宝箱 游戏对象 Entry ID
    GO_KRAZS_CAGE           = 187379,                       // krazscage 游戏对象 Entry ID

    GO_ASHLIS_CAGE          = 187380,                       // ashliscage 游戏对象 Entry ID
    GO_ASHLIS_VASE          = 186671,                       // ashlisvase 游戏对象 Entry ID

    GO_ALTAR_TORCH_EAGLE_GOD = 187035,                      // altartorcheaglegod 游戏对象 Entry ID
    GO_ALTAR_TORCH_DRAGONHAWK_GOD = 187036,                 // altartorchdragonhawkgod 游戏对象 Entry ID
    GO_ALTAR_TORCH_LYNX_GOD = 187037,                       // altartorchlynxgod 游戏对象 Entry ID
    GO_ALTAR_TORCH_BEAR_GOD = 186860,                       // altartorchbeargod 游戏对象 Entry ID

    NPC_WORLD_TRIGGER       = 22515,                        // used to mark location of drums for Amani'shi Scouts and Amani'shi Reinforcement spawns
    NPC_REINFORCEMENT       = 23587,                        // reinforcement Entry ID
    NPC_SAVAGE              = 23889,                        // 野蛮 Entry ID

    SPELL_ZULAMAN_OBJECT_VISUAL     = 45222,                // zulamanobject视觉特效 法术 ID
    SPELL_RETURN_TO_SPIRIT_REALM    = 44035,                // returnto之魂realm 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum BossToChestIndex
{
    INDEX_NALORAKK          = 0,                            // 纳洛拉克 标识 ID
    INDEX_JANALAI           = 1,                            // 埃加拉什 标识 ID
    INDEX_HALAZZI           = 2,                            // 哈尔拉兹 标识 ID
    INDEX_AKILZON           = 3                             // 埃基尔松 标识 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum RunEventSteps
{
    RUN_START               = 1,                            // 开始 标识 ID
    RUN_FAIL                = 2,                            // fail 标识 ID
    RUN_DONE                = 3,                            // done 标识 ID
    RUN_PROGRESS            = 4,                            // progress 标识 ID
    RUN_FAIL_SOON           = 5                             // failsoon 标识 ID
};

struct TimeEventNpcInfo
{
    TimeEventNpcInfo() : uiSavePosition(0) {}

    uint8 uiSavePosition;                                   // stores in what order this npc was saved (0 means unsaved)
    ObjectGuid npGuid;
};

struct NalorakkBearEventInfo
{
    int yellId;
    float x, y, z, ori, aggroDist;
};

static const NalorakkBearEventInfo aBearEventInfo[MAX_BEAR_WAVES] =
{
    {SAY_WAVE1_AGGRO,    0, 0, 0, 0, 45.0f},
    {SAY_WAVE2_STAIR1,   -52.46344f, 1419.698f, 27.29911f, 0.f, 37.0f},
    {SAY_WAVE3_STAIR2,   -80.22266f, 1375.835f, 40.75975f, 1.570796f, 35.0f},
    {SAY_WAVE4_PLATFORM, -79.59927f, 1300.431f, 48.5325f, 1.570796f, 60.0f}
};

struct NalorakkTrashInfo
{
    GuidSet nalorakkTrashSet;
    uint8 trashKilledCount;
};

// 副本进度与状态机管理类：标识 ID（instance_zulaman）
class instance_zulaman : public ScriptedInstance
{
    public:
        instance_zulaman(Map* map);

        // 初始化副本数据结构
        void Initialize() override;
        bool IsEncounterInProgress() const override;

        // 玩家进入副本区域回调
        void OnPlayerEnter(Player* player) override;
        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* creature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* creature) override;
        void OnCreatureEvade(Creature* creature) override;
		void OnCreatureRespawn(Creature* creature) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        bool IsBearPhaseInProgress() const { return m_isBearPhaseInProgress; }
        void SetBearEventProgress(bool bIsInProgress) { m_isBearPhaseInProgress = bIsInProgress; }
        void SendNextBearWave(Unit* pTarget);

        bool IsAkilzonGauntletSpawning() const { return m_auiEncounter[TYPE_AKILZON_GAUNTLET] == IN_PROGRESS; }
        bool IsAkilzonGauntletInProgress() const { return m_auiEncounter[TYPE_AKILZON_GAUNTLET] != NOT_STARTED; }

        bool CheckConditionCriteriaMeet(Player const* player, uint32 instanceConditionId, WorldObject const* conditionSource, uint32 conditionSourceType) const override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        GuidSet sDrumTriggerGuidSet;
        GuidSet sHutTriggerGuidSet;

        GuidSet sGongGuardianGuidSet;
        ObjectGuid m_GongGuardianAttackerGuid;              // GongGuardianAttacker 实体 GUID

        void ChangeWeather(bool rain);

        void ShowChatCommands(ChatHandler* handler) override;
        void ExecuteChatCommand(ChatHandler* handler, char* args) override;

        void StartSpiritTimer();

        std::vector<uint32>& GetMalacrassAddsEntryList() { return m_malacrassAddsEntryList; }
    private:
        uint8 GetKilledPreBosses();
        void DoTimeRunSay(RunEventSteps uiData);
        void DoChestEvent(BossToChestIndex uiIndex);
        void SpawnMalacrass();

        std::string m_strInstData;                          // 序列化持久化保存的进度字符串
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        uint32 m_auiRandVendor[MAX_VENDOR];
        TimeEventNpcInfo m_aEventNpcInfo[MAX_CHESTS];

        bool m_startCheck;
        uint32 m_uiEventTimer;                              // Event 技能计时器

        NalorakkTrashInfo m_nalorakkEvent[MAX_BEAR_WAVES];
        uint8 m_uiBearEventPhase;
        bool m_isBearPhaseInProgress;

        GuidSet sAkilzonTrashGuidSet;

        uint32 m_spiritFadeTimer;

        std::vector<uint32> m_malacrassAddsEntryList;
};

#endif                                                      // 结束头文件防重复包含宏
