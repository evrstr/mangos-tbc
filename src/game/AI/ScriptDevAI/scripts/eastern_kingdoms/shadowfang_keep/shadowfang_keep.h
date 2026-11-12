/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 影牙城堡（Shadowfang Keep）全局定义头文件

#ifndef DEF_SHADOWFANG_H                                    // 防止头文件重复包含
#define DEF_SHADOWFANG_H

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER           = 6,                            // encounter 标识 ID

    TYPE_FREE_NPC           = 1,                            // 释放NPC 遭遇战进度数据标识 ID
    TYPE_RETHILGORE         = 2,                            // rethilgore 遭遇战进度数据标识 ID
    TYPE_FENRUS             = 3,                            // 芬鲁斯 遭遇战进度数据标识 ID
    TYPE_NANDOS             = 4,                            // nandos 遭遇战进度数据标识 ID
    TYPE_INTRO              = 5,                            // 开场 遭遇战进度数据标识 ID
    TYPE_VOIDWALKER         = 6,                            // voidwalker 遭遇战进度数据标识 ID

    SAY_BOSS_DIE_AD         = -1033007,                     // bossdiead 台词广播 ID
    SAY_BOSS_DIE_AS         = -1033008,                     // bossdieas 台词广播 ID
    YELL_PACK_DEAD          = -1033020,                     // packdead 大喊台词广播 ID
    SAY_ARUGAL_INTRO_1      = -1033009,                     // arugal开场1 台词广播 ID
    SAY_ARUGAL_INTRO_2      = -1033010,                     // arugal开场2 台词广播 ID
    SAY_ARUGAL_INTRO_3      = -1033011,                     // arugal开场3 台词广播 ID
    SAY_ARUGAL_INTRO_4      = -1033012,                     // arugal开场4 台词广播 ID

    YELL_FENRUS             = -1033013,                     // 芬鲁斯 大喊台词广播 ID
    YELL_AGGRO              = -1033017,                     // 开战 大喊台词广播 ID
    YELL_KILLED_PLAYER      = -1033018,                     // killedplayer 大喊台词广播 ID
    YELL_WORGEN_CURSE       = -1033019,                     // worgen诅咒 大喊台词广播 ID

    SPELL_SPAWN             = 7741,                         // 生成 法术 ID
    SPELL_ARUGAL_TELEPORT   = 7136,                         // arugal传送 法术 ID
    SPELL_FIRE              = 6422,                         // 火焰 法术 ID

    NPC_ASH                 = 3850,                         // ash Entry ID
    NPC_ADA                 = 3849,                         // ada Entry ID
    GOSSIP_ASH              = 798,                          // ash 对话菜单选项 ID
    GOSSIP_ADA              = 799,                          // ada 对话菜单选项 ID

    NPC_ARUGAL              = 10000,                        // "Arugal" says intro text
    NPC_ARCHMAGE_ARUGAL     = 4275,                         // "Archmage Arugal" does Fenrus event (大法师Arugal"does芬鲁斯事件)
    NPC_FENRUS              = 4274,                         // used to summon Arugal in Fenrus event
    NPC_VINCENT             = 4444,                         // Vincent should be "dead" is Arugal is done the intro already

    // Wolf Master Nandos intro event (Wolf大师Nandos开场事件)
    NPC_MASTER_NANDOS       = 3927,                         // 大师nandos Entry ID
    NPC_LUPINE_HORROR       = 3863,                         // lupinehorror Entry ID
    NPC_WOLFGUARD_WORG      = 5058,                         // wolfguardworg Entry ID
    NPC_BLEAK_WORG          = 3861,                         // bleakworg Entry ID
    NPC_SLAVERING_WORG      = 3862,                         // slaveringworg Entry ID

    GO_COURTYARD_DOOR       = 18895,                        // door to open when talking to NPC's
    GO_SORCERER_DOOR        = 18972,                        // door to open when Fenrus the Devourer
    GO_ARUGAL_DOOR          = 18971,                        // door to open when Wolf Master Nandos
    GO_ARUGAL_FOCUS         = 18973,                        // this generates the lightning visual in the Fenrus event
};

struct Waypoint
{
    float fX, fY, fZ;
};

// 枚举定义：法术技能、生物与环境事件标识
enum {
    VINCENT_DEATH = 1,                                      // 死亡 标识 ID
    ARUGAL_VISIBLE,
    ARUGAL_TELEPORT_IN,
    ARUGAL_TURN_TO_VINCENT,
    ARUGAL_EMOTE_POINT,
    ARUGAL_EMOTE_EXCLAMATION,
    ARUGAL_EMOTE_LAUGH,
    ARUGAL_TELEPORT_OUT,
    ARUGAL_INTRO_DONE,
    ARCHMAGE_FIRE,
    ARCHMAGE_LIGHTNING,
    ARCHMAGE_INVIS,
    ARCHMAGE_VOIDWALKERS,
};

static const Waypoint nandosMovement = {-170.6f, 2182.45f, 151.91f};

// 副本进度与状态机管理类：keep（instance_shadowfang_keep）
class instance_shadowfang_keep : public ScriptedInstance, public DialogueHelper
{
    public:
        instance_shadowfang_keep(Map* map);

        // 初始化副本数据结构
        void Initialize() override;

        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* creature) override;
        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* creature) override;
        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* go) override;
        void DoSpeech();
        void JustDidDialogueStep(int32 entry) override;

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

    private:
        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        GuidList m_lNandosWolvesGuids;
};

#endif                                                      // 结束头文件防重复包含宏
