/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_TEMPLE_OF_AHNQIRAJ_H
#define DEF_TEMPLE_OF_AHNQIRAJ_H

/**
 * 安其拉神殿 (Temple of Ahn'Qiraj) 副本常量与首领遭遇枚举定义
 */

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER               = 10,

    TYPE_SKERAM                 = 0,                        // 首领遭遇/事件状态标识
    TYPE_BUG_TRIO               = 1,                        // 首领遭遇/事件状态标识
    TYPE_SARTURA                = 2,                        // 首领遭遇/事件状态标识
    TYPE_FANKRISS               = 3,                        // 首领遭遇/事件状态标识
    TYPE_VISCIDUS               = 4,                        // 首领遭遇/事件状态标识
    TYPE_HUHURAN                = 5,                        // 首领遭遇/事件状态标识
    TYPE_TWINS                  = 6,                        // 首领遭遇/事件状态标识
    TYPE_OURO                   = 7,                        // 首领遭遇/事件状态标识
    TYPE_CTHUN                  = 8,                        // 首领遭遇/事件状态标识
    TYPE_TWINS_INTRO            = 9,                        // 首领遭遇/事件状态标识

    NPC_SKERAM                  = 15263,                    // NPC 条目 ID
    NPC_KRI                     = 15511,                    // NPC 条目 ID
    NPC_VEM                     = 15544,                    // NPC 条目 ID
    NPC_YAUJ                    = 15543,                    // NPC 条目 ID
    NPC_SARTURA                 = 15516,                    // NPC 条目 ID
    NPC_VEKLOR                  = 15276,                    // NPC 条目 ID
    NPC_VEKNILASH               = 15275,                    // NPC 条目 ID
    NPC_MASTERS_EYE             = 15963,                    // NPC 条目 ID
    NPC_OURO_SPAWNER            = 15957,                    // NPC 条目 ID
    NPC_VISCIDUS                = 15299,                    // NPC 条目 ID

    // C'Thun encounter
    NPC_EYE_OF_CTHUN            = 15589,                    // NPC 条目 ID
    NPC_CTHUN                   = 15727,                    // NPC 条目 ID
    NPC_CLAW_TENTACLE           = 15725,                    // NPC 条目 ID
    NPC_EYE_TENTACLE            = 15726,                    // NPC 条目 ID
    NPC_TENTACLE_PORTAL         = 15904,                    // NPC 条目 ID
    NPC_GIANT_CLAW_TENTACLE     = 15728,                    // NPC 条目 ID
    NPC_GIANT_EYE_TENTACLE      = 15334,                    // NPC 条目 ID
    NPC_EXIT_TRIGGER            = 15800,                    // NPC 条目 ID
    NPC_FLESH_TENTACLE          = 15802,                    // NPC 条目 ID
    NPC_GIANT_TENTACLE_PORTAL   = 15910,                    // NPC 条目 ID

    GO_SKERAM_GATE              = 180636,                   // 游戏对象条目 ID
    GO_TWINS_ENTER_DOOR         = 180634,                   // 游戏对象条目 ID
    GO_TWINS_EXIT_DOOR          = 180635,                   // 游戏对象条目 ID
    GO_SANDWORM_BASE            = 180795,                   // 游戏对象条目 ID

    EMOTE_EYE_INTRO             = 11700,                    // 剧情台词/喊话文本 ID
    STAND_EMPERORS_INTRO        = 1,
    SAY_EMPERORS_INTRO_1        = -1531013,                 // 剧情台词/喊话文本 ID
    SAY_EMPERORS_INTRO_2        = -1531014,                 // 剧情台词/喊话文本 ID
    SAY_EMPERORS_INTRO_3        = -1531015,                 // 剧情台词/喊话文本 ID
    SAY_EMPERORS_INTRO_4        = -1531016,                 // 剧情台词/喊话文本 ID
    SAY_EMPERORS_INTRO_5        = -1531017,                 // 剧情台词/喊话文本 ID
    SAY_EMPERORS_INTRO_6        = -1531018,                 // 剧情台词/喊话文本 ID

    // Whispered on players around the map (说明)
    SAY_CTHUN_WHISPER_1         = -1531033,                 // 剧情台词/喊话文本 ID
    SAY_CTHUN_WHISPER_2         = -1531034,                 // 剧情台词/喊话文本 ID
    SAY_CTHUN_WHISPER_3         = -1531035,                 // 剧情台词/喊话文本 ID
    SAY_CTHUN_WHISPER_4         = -1531036,                 // 剧情台词/喊话文本 ID
    SAY_CTHUN_WHISPER_5         = -1531037,                 // 剧情台词/喊话文本 ID
    SAY_CTHUN_WHISPER_6         = -1531038,                 // 剧情台词/喊话文本 ID
    SAY_CTHUN_WHISPER_7         = -1531039,                 // 剧情台词/喊话文本 ID
    SAY_CTHUN_WHISPER_8         = -1531040,                 // 剧情台词/喊话文本 ID

    AREATRIGGER_TWIN_EMPERORS   = 4047,
    AREATRIGGER_SARTURA         = 4052,

    SPELL_SUMMON_PLAYER         = 20477,                    // 技能法术 ID

    // Cast periodically on players around the instance (说明)
    SPELL_WHISPERINGS_CTHUN_1   = 26195,                    // 技能法术 ID
    SPELL_WHISPERINGS_CTHUN_2   = 26197,                    // 技能法术 ID
    SPELL_WHISPERINGS_CTHUN_3   = 26198,                    // 技能法术 ID
    SPELL_WHISPERINGS_CTHUN_4   = 26258,                    // 技能法术 ID
    SPELL_WHISPERINGS_CTHUN_5   = 26259,                    // 技能法术 ID

    SPELL_SUMMON_PORTAL         = 26396,                    // 技能法术 ID
    SPELL_SUMMON_GIANT_PORTAL   = 26477,                    // 技能法术 ID
    SPELL_BIRTH_TENTACLE        = 26586,                    // 技能法术 ID

    NPC_POISON_CLOUD            = 15933,                    // NPC 条目 ID

    NPC_QIRAJI_SCARAB           = 15316,                    // NPC 条目 ID
    NPC_QIRAJI_SCORPION         = 15317,                    // NPC 条目 ID
};

// Prophecies yelled by Propher Skeram before he is engaged (说明)
static const uint32 sound_skeram_prophecy[] = { 8616, 8621, 8619, 8620, 8618 };

// instance_temple_of_ahnqiraj 副本数据管理脚本
class instance_temple_of_ahnqiraj : public ScriptedInstance, private DialogueHelper
{
    public:
        instance_temple_of_ahnqiraj(Map* map);
        ~instance_temple_of_ahnqiraj() {}

        void Initialize() override;                         // 初始化副本数据与状态

        bool IsEncounterInProgress() const override;        // 检查是否有首领战正在进行中

        void OnCreatureCreate(Creature* creature) override;    // 生物创建/加载至副本地图时的回调
        void OnCreatureDeath(Creature* creature) override;    // 生物在副本地图中死亡回调
        void OnObjectCreate(GameObject* go) override;       // 游戏对象创建/加载至副本地图时的回调
        void OnCreatureRespawn(Creature* creature) override;

        void SetData(uint32 type, uint32 data) override;    // 设置自定义状态数据
        uint32 GetData(uint32 type) const override;         // 获取自定义状态数据

        void DoHandleTempleAreaTrigger(uint32 triggerId, Player* player);

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

        void Update(uint32 diff) override;

    private:
        uint32 m_encounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        uint8 m_uiBugTrioDeathCount;
        uint32 m_uiCthunWhisperTimer;
        uint32 m_uiSkeramProphecyTimer;

        GuidVector m_bugTrioSpawns;

        void JustDidDialogueStep(int32 entry) override;
};

#endif
