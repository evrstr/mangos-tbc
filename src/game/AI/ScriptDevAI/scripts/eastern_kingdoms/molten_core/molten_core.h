/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 熔火之心（Molten Core）副本全局定义头文件

#ifndef DEF_MOLTEN_CORE_H                                   // 防止头文件重复包含
#define DEF_MOLTEN_CORE_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER               = 10,                       // encounter 标识 ID

    TYPE_LUCIFRON               = 0,                        // 鲁西弗隆 遭遇战进度数据标识 ID
    TYPE_MAGMADAR               = 1,                        // 玛格曼达 遭遇战进度数据标识 ID
    TYPE_GEHENNAS               = 2,                        // 基赫纳斯 遭遇战进度数据标识 ID
    TYPE_GARR                   = 3,                        // 加尔 遭遇战进度数据标识 ID
    TYPE_SHAZZRAH               = 4,                        // 沙斯拉尔 遭遇战进度数据标识 ID
    TYPE_GEDDON                 = 5,                        // 迦顿男爵 遭遇战进度数据标识 ID
    TYPE_GOLEMAGG               = 6,                        // 古雷曼格 遭遇战进度数据标识 ID
    TYPE_SULFURON               = 7,                        // 萨弗隆 遭遇战进度数据标识 ID
    TYPE_MAJORDOMO              = 8,                        // majordomo 遭遇战进度数据标识 ID
    TYPE_RAGNAROS               = 9,                        // 拉格纳罗斯 遭遇战进度数据标识 ID

    NPC_LUCIFRON                = 12118,                    // 鲁西弗隆 Entry ID
    NPC_MAGMADAR                = 11982,                    // 玛格曼达 Entry ID
    NPC_GEHENNAS                = 12259,                    // 基赫纳斯 Entry ID
    NPC_GARR                    = 12057,                    // 加尔 Entry ID
    NPC_SHAZZRAH                = 12264,                    // 沙斯拉尔 Entry ID
    NPC_GEDDON                  = 12056,                    // 迦顿男爵 Entry ID
    NPC_GOLEMAGG                = 11988,                    // 古雷曼格 Entry ID
    NPC_SULFURON                = 12098,                    // 萨弗隆 Entry ID
    NPC_MAJORDOMO               = 12018,                    // majordomo Entry ID
    NPC_RAGNAROS                = 11502,                    // 拉格纳罗斯 Entry ID

    // Adds
    // Used for respawn in case of wipe
    NPC_FLAMEWAKER_PROTECTOR    = 12119,                    // Lucifron (鲁西弗隆)
    NPC_FLAMEWAKER              = 11661,                    // Gehennas (基赫纳斯)
    NPC_FIRESWORN               = 12099,                    // Garr (加尔)
    NPC_CORE_RAGER              = 11672,                    // Golemagg (古雷曼格)
    NPC_FLAMEWAKER_PRIEST       = 11662,                    // Sulfuron (萨弗隆)
    NPC_FLAMEWAKER_HEALER       = 11663,                    // Majordomo
    NPC_FLAMEWAKER_ELITE        = 11664,                    // Majordomo

    GO_LAVA_STEAM               = 178107,                   // lavasteam 游戏对象 Entry ID
    GO_LAVA_SPLASH              = 178108,                   // lavasplash 游戏对象 Entry ID
    GO_CACHE_OF_THE_FIRE_LORD   = 179703,                   // 储物箱ofthe火焰lord 游戏对象 Entry ID
    GO_RUNE_KRESS               = 176956,                   // Magmadar (玛格曼达)
    GO_RUNE_MOHN                = 176957,                   // Gehennas (基赫纳斯)
    GO_RUNE_BLAZ                = 176955,                   // Garr (加尔)
    GO_RUNE_MAZJ                = 176953,                   // Shazzrah (沙斯拉尔)
    GO_RUNE_ZETH                = 176952,                   // Geddon (迦顿男爵)
    GO_RUNE_THERI               = 176954,                   // Golemagg (古雷曼格)
    GO_RUNE_KORO                = 176951,                   // Sulfuron (萨弗隆)
    GO_CIRCLE_MAGMADAR          = 178192,                   // Flames circle around each rune, removed when each boss is killed
    GO_CIRCLE_GEHENNAS          = 178193,                   // circle基赫纳斯 游戏对象 Entry ID
    GO_CIRCLE_GARR              = 178191,                   // circle加尔 游戏对象 Entry ID
    GO_CIRCLE_SHAZZRAH          = 178189,                   // circle沙斯拉尔 游戏对象 Entry ID
    GO_CIRCLE_BARON_GEDDON      = 178188,                   // circle男爵迦顿男爵 游戏对象 Entry ID
    GO_CIRCLE_GOLEMAGG          = 178190,                   // circle古雷曼格 游戏对象 Entry ID
    GO_CIRCLE_SULFURON          = 178187,                   // circle萨弗隆 游戏对象 Entry ID

    MAX_MOLTEN_RUNES            = 7,                        // molten符文 标识 ID
    MAX_MAJORDOMO_ADDS          = 8,                        // majordomoadds 标识 ID
    FACTION_MAJORDOMO_FRIENDLY  = 1080,                     // majordomofriendly 阵营 ID
    SAY_MAJORDOMO_SPAWN         = -1409004,                 // majordomo生成 台词广播 ID
};

struct sRuneEncounters
{
    uint32 m_uiRuneEntry, m_uiFlamesCircleEntry, m_uiType;
};

static const sRuneEncounters m_aMoltenCoreRunes[MAX_MOLTEN_RUNES] =
{
    {GO_RUNE_KRESS, GO_CIRCLE_MAGMADAR,     TYPE_MAGMADAR},
    {GO_RUNE_MOHN,  GO_CIRCLE_GEHENNAS,     TYPE_GEHENNAS},
    {GO_RUNE_BLAZ,  GO_CIRCLE_GARR,         TYPE_GARR},
    {GO_RUNE_MAZJ,  GO_CIRCLE_SHAZZRAH,     TYPE_SHAZZRAH},
    {GO_RUNE_ZETH,  GO_CIRCLE_BARON_GEDDON, TYPE_GEDDON},
    {GO_RUNE_THERI, GO_CIRCLE_GOLEMAGG,     TYPE_GOLEMAGG},
    {GO_RUNE_KORO,  GO_CIRCLE_SULFURON,     TYPE_SULFURON}
};

static SpawnLocation m_aMajordomoLocations[2] =
{
    {NPC_MAJORDOMO, 758.0892f, -1176.712f, -118.6403f, 3.124139f, 0, 0},  // Summon fight position (召唤 fight position)
    {NPC_MAJORDOMO, 848.933f, -812.875f, -229.601f, 4.046f,       0, 0}     // Summon and teleport location (near Ragnaros) (召唤 and teleport location (near Ragnaros))
};

// 副本进度与状态机管理类：core（instance_molten_core）
class instance_molten_core : public ScriptedInstance
{
    public:
        instance_molten_core(Map* pMap);
        ~instance_molten_core() {}

        // 初始化副本数据结构
        void Initialize() override;
        bool IsEncounterInProgress() const override;

        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        // 玩家进入副本区域回调
        void OnPlayerEnter(Player* pPlayer) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

        void ShowChatCommands(ChatHandler* handler) override;
        void ExecuteChatCommand(ChatHandler* handler, char* args) override;

    protected:
        void DoSpawnMajordomoIfCan(bool bByPlayerEnter);
        void SpawnMajordomo(Unit* summoner, bool initialSummon, uint8 summonPos);

        std::string m_strInstData;                          // 序列化持久化保存的进度字符串
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
};

#endif                                                      // 结束头文件防重复包含宏
