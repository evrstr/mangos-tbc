/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 通灵学院（Scholomance）全局定义头文件

#ifndef DEF_SCHOLOMANCE_H                                   // 防止头文件重复包含
#define DEF_SCHOLOMANCE_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER           = 10,                           // encounter 标识 ID
    MAX_EVENTS              = 6,                            // events 标识 ID
    MAX_GROUPS              = 4,                            // groups 标识 ID
    MAX_NPC_PER_GROUP       = 4,                            // NPCpergroup 标识 ID

    TYPE_KIRTONOS           = 0,                            // kirtonos 遭遇战进度数据标识 ID
    TYPE_RATTLEGORE         = 1,                            // rattlegore 遭遇战进度数据标识 ID
    TYPE_RAS_FROSTWHISPER   = 2,                            // rasfrostwhisper 遭遇战进度数据标识 ID
    TYPE_MALICIA            = 3,                            // malicia 遭遇战进度数据标识 ID
    TYPE_THEOLEN            = 4,                            // theolen 遭遇战进度数据标识 ID
    TYPE_POLKELT            = 5,                            // polkelt 遭遇战进度数据标识 ID
    TYPE_RAVENIAN           = 6,                            // ravenian 遭遇战进度数据标识 ID
    TYPE_ALEXEI_BAROV       = 7,                            // alexei巴罗夫 遭遇战进度数据标识 ID
    TYPE_ILLUCIA_BAROV      = 8,                            // illucia巴罗夫 遭遇战进度数据标识 ID
    TYPE_GANDLING           = 9,                            // 加丁 遭遇战进度数据标识 ID

    NPC_NECROFIEND          = 11551,                        // necrofiend Entry ID
    NPC_RISEN_ABERRATION    = 10485,                        // 复活畸变体 Entry ID
    NPC_DISEASED_GHOUL      = 10495,                        // diseased食尸鬼 Entry ID
    NPC_REANIMATED_CORPSE   = 10481,                        // reanimated尸体 Entry ID
    NPC_SCHOLOMANCE_STUDENT = 10475,                        // scholomancestudent Entry ID

    NPC_KIRTONOS            = 10506,                        // kirtonos Entry ID
    NPC_RATTLEGORE          = 11622,                        // rattlegore Entry ID
    NPC_RAS_FROSTWHISPER    = 10508,                        // rasfrostwhisper Entry ID
    NPC_THEOLEN_KRASTINOV   = 11261,                        // theolenkrastinov Entry ID
    NPC_LOREKEEPER_POLKELT  = 10901,                        // lorekeeperpolkelt Entry ID
    NPC_RAVENIAN            = 10507,                        // ravenian Entry ID
    NPC_ILLUCIA_BAROV       = 10502,                        // illucia巴罗夫 Entry ID
    NPC_ALEXEI_BAROV        = 10504,                        // alexei巴罗夫 Entry ID
    NPC_INSTRUCTOR_MALICIA  = 10505,                        // instructormalicia Entry ID
    NPC_VECTUS              = 10432,                        // vectus Entry ID
    NPC_MARDUK_BLACKPOOL    = 10433,                        // mardukblackpool Entry ID
    NPC_DARKMASTER_GANDLING = 1853,                         // 院长加丁 Entry ID
    NPC_RISEN_GUARDIAN      = 11598,                        // summoned in random rooms by gandling

    GO_GATE_KIRTONOS        = 175570,                       // 门禁kirtonos 游戏对象 Entry ID
    GO_VIEWING_ROOM_DOOR    = 175167,                       // Must be opened in reload case
    GO_GATE_RAS             = 177370,                       // 门禁ras 游戏对象 Entry ID
    GO_GATE_MALICIA         = 177375,                       // 门禁malicia 游戏对象 Entry ID
    GO_GATE_THEOLEN         = 177377,                       // 门禁theolen 游戏对象 Entry ID
    GO_GATE_POLKELT         = 177376,                       // 门禁polkelt 游戏对象 Entry ID
    GO_GATE_RAVENIAN        = 177372,                       // 门禁ravenian 游戏对象 Entry ID
    GO_GATE_BAROV           = 177373,                       // 门禁巴罗夫 游戏对象 Entry ID
    GO_GATE_ILLUCIA         = 177371,                       // 门禁illucia 游戏对象 Entry ID
    GO_GATE_GANDLING        = 177374,                       // 门禁加丁 游戏对象 Entry ID

    // Because the shadow portal teleport coordinates are guesswork (taken from old script) these IDs might be randomized
    // TODO Syncronise with correct DB coordinates when they will be known
    EVENT_ID_POLKELT        = 5618,                         // IDpolkelt 事件标识 ID
    EVENT_ID_THEOLEN        = 5619,                         // IDtheolen 事件标识 ID
    EVENT_ID_MALICIA        = 5620,                         // IDmalicia 事件标识 ID
    EVENT_ID_ILLUCIA        = 5621,                         // IDillucia 事件标识 ID
    EVENT_ID_BAROV          = 5622,                         // ID巴罗夫 事件标识 ID
    EVENT_ID_RAVENIAN       = 5623,                         // IDravenian 事件标识 ID

    EVENT_ID_DAWN_GAMBIT    = 5140,                         // IDdawngambit 事件标识 ID

    FACTION_SCOURGE         = 233,                          // scourge 阵营 ID

    SAY_GANDLING_SPAWN      = -1289000,                     // 加丁生成 台词广播 ID
    YELL_VECTUS_GAMBIT      = -1289001,                     // vectusgambit 大喊台词广播 ID

    SPELL_STUDENT_TRANSFORM = 18115,                        // student变形 法术 ID
    SPELL_XMAS_GANDLING     = 26199,                        // xmas加丁 法术 ID
};

static const Position aGandlingSpawnLocs[1] =
{
    {180.771f, -5.4286f, 75.5702f, 1.29154f}
};

// Coordinates used to respawn the NPCs in the entrance room
// (right before the Viewing Room) on Rattlegore's death
static const Position aEntranceRoomSpawnLocs[17] =
{
    {186.036f, 94.5f, 104.72f, 1.29154f},      // First corner
    {179.117f, 95.5166f, 104.81f, 1.29154f},
    {180.612f, 100.176f, 104.80f, 1.29154f},
    {185.926f, 100.079f, 104.80f, 1.29154f},
    {178.999f, 75.2952f, 104.72f, 1.29154f},   // Second corner
    {185.558f, 77.276f, 104.72f, 1.29154f},
    {187.556f, 70.4334f, 104.72f, 1.29154f},
    {180.51f, 82.3917f, 104.72f, 1.29154f},
    {212.915f, 70.6005f, 104.80f, 1.29154f},   // Third corner
    {221.199f, 77.0037f, 104.72f, 1.29154f},
    {214.381f, 76.233f, 104.80f, 1.29154f},
    {218.64f, 71.5957f, 104.72f, 1.29154f},
    {221.249f, 94.9361f, 104.72f, 1.29154f},   // Fourth corner
    {214.406f, 101.903f, 104.72f, 1.29154f},
    {217.521f, 95.4237f, 104.72f, 1.29154f},
    {223.296f, 105.101f, 104.72f, 1.29154f},
    {209.233f, 73.2819f, 104.80f, 1.29154f}    // patrolling necrofiend
};

struct GandlingEventData
{
    GandlingEventData() : m_bIsActive(false) {}
    bool m_bIsActive;                                       // IsActive 状态标记
    ObjectGuid m_doorGuid;                                  // door 实体 GUID
    GuidSet m_sAddGuids;                                    // sAddGuids GUID 集合
};

struct BoxVolume
{
    float m_fCornerX;
    float m_fCornerY;
    float m_fCenterZ;
    uint32 m_uiLength;
    uint32 m_uiWidth;
};

static const BoxVolume aEntranceRoom = { 174.13f, 63.84f, 104.0f, 54, 44 };

static const uint32 aGandlingEvents[MAX_EVENTS] = {EVENT_ID_POLKELT, EVENT_ID_THEOLEN, EVENT_ID_MALICIA, EVENT_ID_ILLUCIA, EVENT_ID_BAROV, EVENT_ID_RAVENIAN};

typedef std::map<uint32, GandlingEventData> GandlingEventMap;

// 副本进度与状态机管理类：标识 ID（instance_scholomance）
class instance_scholomance : public ScriptedInstance
{
    public:
        instance_scholomance(Map* pMap);
        ~instance_scholomance() {}

        // 初始化副本数据结构
        void Initialize() override;

        void OnCreatureEnterCombat(Creature* pCreature) override;
        void OnCreatureEvade(Creature* pCreature) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;

        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        // 玩家进入副本区域回调
        void OnPlayerEnter(Player* pPlayer) override;

        void HandlePortalEvent(uint32 uiEventId, uint32 uiData);
        void HandleDawnGambitEvent();

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

    private:
        void DoSpawnGandlingIfCan(bool bByPlayerEnter);
        void DoRespawnEntranceRoom(Player* pSummoner);

        uint32 m_uiGambitTransformTimer;                    // GambitTransform 技能计时器
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        uint32 m_uiGandlingEvent;
        GandlingEventMap m_mGandlingData;

        bool m_bIsRoomReset;                                // IsRoomReset 状态标记
        GuidSet m_sEntranceRoomGuids;                       // sEntranceRoomGuids GUID 集合
        GuidSet m_sViewingRoomGuids;                        // sViewingRoomGuids GUID 集合
};

#endif                                                      // 结束头文件防重复包含宏
