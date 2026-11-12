/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_RUINS_OF_AHNQIRAJ_H
#define DEF_RUINS_OF_AHNQIRAJ_H

/**
 * 安其拉废墟 (Ruins of Ahn'Qiraj) 副本常量与进度枚举定义
 */

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER               = 6,
    MAX_HELPERS                 = 5,

    TYPE_KURINNAXX              = 0,                        // 首领遭遇/事件状态标识
    TYPE_RAJAXX                 = 1,                        // 首领遭遇/事件状态标识
    TYPE_MOAM                   = 2,                        // 首领遭遇/事件状态标识
    TYPE_BURU                   = 3,                        // 首领遭遇/事件状态标识
    TYPE_AYAMISS                = 4,                        // 首领遭遇/事件状态标识
    TYPE_OSSIRIAN               = 5,                        // 首领遭遇/事件状态标识

    NPC_KURINNAXX               = 15348,                    // NPC 条目 ID
    NPC_MOAM                    = 15340,                    // NPC 条目 ID
    NPC_BURU                    = 15370,                    // NPC 条目 ID
    NPC_AYAMISS                 = 15369,                    // NPC 条目 ID
    NPC_OSSIRIAN                = 15339,                    // NPC 条目 ID
    NPC_GENERAL_ANDOROV         = 15471,                    // The general and the kaldorei are escorted for the rajaxx encounter (说明)
    NPC_KALDOREI_ELITE          = 15473,                    // NPC 条目 ID
    NPC_RAJAXX                  = 15341,                    // All of the following are used in the rajaxx encounter (说明)
    NPC_COLONEL_ZERRAN          = 15385,                    // NPC 条目 ID
    NPC_MAJOR_PAKKON            = 15388,                    // NPC 条目 ID
    NPC_MAJOR_YEGGETH           = 15386,                    // NPC 条目 ID
    NPC_CAPTAIN_XURREM          = 15390,                    // NPC 条目 ID
    NPC_CAPTAIN_DRENN           = 15389,                    // NPC 条目 ID
    NPC_CAPTAIN_TUUBID          = 15392,                    // NPC 条目 ID
    NPC_CAPTAIN_QEEZ            = 15391,                    // NPC 条目 ID
    NPC_QIRAJI_WARRIOR          = 15387,                    // NPC 条目 ID
    NPC_SWARMGUARD_NEEDLER      = 15344,                    // NPC 条目 ID

    MAX_ARMY_WAVES              = 7,

    GO_OSSIRIAN_CRYSTAL         = 180619,                   // Used in the ossirian encounter (说明)
    NPC_OSSIRIAN_TRIGGER        = 15590,                    // Triggers ossirian weakness (说明)

    SAY_OSSIRIAN_INTRO          = -1509022,                 // Yelled after Kurinnax dies (说明)

    // Rajaxx yells (说明)
    SAY_WAVE3                   = -1509005,                 // 剧情台词/喊话文本 ID
    SAY_WAVE4                   = -1509006,                 // 剧情台词/喊话文本 ID
    SAY_WAVE5                   = -1509007,                 // 剧情台词/喊话文本 ID
    SAY_WAVE6                   = -1509008,                 // 剧情台词/喊话文本 ID
    SAY_WAVE7                   = -1509009,                 // 剧情台词/喊话文本 ID
    SAY_INTRO                   = -1509010,                 // 剧情台词/喊话文本 ID
    SAY_DEAGGRO                 = -1509015,                 // on Rajaxx evade (说明)
    SAY_COMPLETE_QUEST          = -1509017,                 // Yell when realm complete quest 8743 for world event (说明)
};

// Spawn coords for Andorov and his team (说明)
static const SpawnLocation aAndorovSpawnLocs[MAX_HELPERS] =
{
    {NPC_GENERAL_ANDOROV, -8660.4f,  1510.29f, 32.449f,  2.2184f,  0, 0},
    {NPC_KALDOREI_ELITE,  -8655.84f, 1509.78f, 32.462f,  2.33341f, 0, 0},
    {NPC_KALDOREI_ELITE,  -8657.39f, 1506.28f, 32.418f,  2.33346f, 0, 0},
    {NPC_KALDOREI_ELITE,  -8660.96f, 1504.9f,  32.1567f, 2.33306f, 0, 0},
    {NPC_KALDOREI_ELITE,  -8664.45f, 1506.44f, 32.0944f, 2.33302f, 0, 0}
};

// Movement locations for Andorov (说明)
static const SpawnLocation aAndorovMoveLocs[] =
{
    {0, -8701.51f, 1561.80f, 32.092f, 0.0,  0, 0},
    {0, -8718.66f, 1577.69f, 21.612f, 0.0,  0, 0},
    {0, -8876.97f, 1651.96f, 21.57f, 5.52f, 0, 0},
    {0, -8882.15f, 1602.77f, 21.386f, 0.0,  0, 0},
    {0, -8940.45f, 1550.69f, 21.616f, 0.0,  0, 0},
};

struct SortingParameters
{
    uint32 m_uiEntry;
    int32 m_uiYellEntry;
    std::string m_stringId;
};

static const SortingParameters aArmySortingParameters[MAX_ARMY_WAVES] =
{
    {NPC_CAPTAIN_QEEZ,   0,         "AQ20_QEEZ"},
    {NPC_CAPTAIN_TUUBID, 0,         "AQ20_TUUBID"},
    {NPC_CAPTAIN_DRENN,  SAY_WAVE3, "AQ20_DRENN"},
    {NPC_CAPTAIN_XURREM, SAY_WAVE4, "AQ20_XURREM"},
    {NPC_MAJOR_YEGGETH,  SAY_WAVE5, "AQ20_YEGGETH"},
    {NPC_MAJOR_PAKKON,   SAY_WAVE6, "AQ20_PAKKON"},
    {NPC_COLONEL_ZERRAN, SAY_WAVE7, "AQ20_ZERRAN"},
};

// instance_ruins_of_ahnqiraj 副本数据管理脚本
class instance_ruins_of_ahnqiraj : public ScriptedInstance
{
    public:
        instance_ruins_of_ahnqiraj(Map* pMap);
        ~instance_ruins_of_ahnqiraj() {}

        void Initialize() override;                         // 初始化副本数据与状态

        bool IsEncounterInProgress() const override;              // not active in AQ20 (说明)

        void OnCreatureCreate(Creature* creature) override;    // 生物创建/加载至副本地图时的回调
        void OnObjectCreate(GameObject* go) override;       // 游戏对象创建/加载至副本地图时的回调
        void OnPlayerEnter(Player* player) override;

        void OnCreatureEnterCombat(Creature* creature) override;
        void OnCreatureEvade(Creature* creature) override;
        void OnCreatureDeath(Creature* creature) override;    // 生物在副本地图中死亡回调
        void OnCreatureRespawn(Creature* creature) override;

        void OnCreatureGroupDespawn(CreatureGroup* group, Creature* creature) override;

        void SetData(uint32 type, uint32 data) override;    // 设置自定义状态数据
        uint32 GetData(uint32 type) const override;         // 获取自定义状态数据

        void GetKaldoreiGuidList(GuidList& lList) const { lList = m_kaldoreiGuidList; }

        void Update(const uint32 diff) override;

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

    private:
        void DoSpawnAndorovIfCan();
        void DoSortArmyWaves();
        void DoSendNextArmyWave();

        uint32 m_encounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        GuidList m_kaldoreiGuidList;

        uint32 m_armyDelayTimer;
        uint8 m_currentArmyWave;
};
#endif
