/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_SHADOW_LABYRINTH_H
#define DEF_SHADOW_LABYRINTH_H

// 暗影迷宫首领、随从与对象枚举
enum
{
    MAX_ENCOUNTER           = 4,                            // 首领遭遇战总数

    TYPE_HELLMAW            = 1,                            // 赫尔默大使
    // TYPE_OVERSEER           = 2,                            // obsolete id used by acid (已废弃 ID)
    TYPE_INCITER            = 3,                            // 煽动者布莱卡特
    TYPE_VORPIL             = 4,                            // 沃匹尔大师
    TYPE_MURMUR             = 5,                            // 摩摩尔

    DATA_CABAL_RITUALIST    = 1,                            // DO NOT CHANGE! Used by Acid. - used to check the Cabal Ritualists alive (苦修者存活检查)

    NPC_HELLMAW             = 18731,                        // 赫尔默大使
    NPC_VORPIL              = 18732,                        // 沃匹尔大师
    NPC_CABAL_RITUALIST     = 18794,                        // 暗影议会苦修者
    NPC_BLACKHEART_THE_INCITER = 18667,                     // 煽动者布莱卡特
    NPC_CONTAINMENT_BEAM    = 21159,                        // 抑制光束触发器

    NPC_CABAL_SPELLBINDER   = 18639,                        // 暗影议会缚法者
    NPC_CABAL_SUMMONER      = 18634,                        // 暗影议会召唤师
    NPC_MURMUR              = 18708,                        // 摩摩尔

    GO_REFECTORY_DOOR       = 183296,                       // door opened when blackheart the inciter dies (布莱卡特死后开启的大门)
    GO_SCREAMING_HALL_DOOR  = 183295,                       // door opened when grandmaster vorpil dies and player comes in range (沃匹尔死后开启的尖啸大厅大门)

    SPELL_BANISH            = 30231,                        // spell is handled in creature_template_addon; (放逐光环)
    SPELL_SHAPE_OF_BEAST    = 33949,                        // 猛兽形态法术

    // Blackheart the Inciter (布莱卡特房间怪物组)
    SL_BLACKHEAT_GROUP_01   = 5550024,
    SL_BLACKHEAT_GROUP_02   = 5550025,
    SL_BLACKHEAT_GROUP_03   = 5550026,
    SL_BLACKHEAT_GROUP_04   = 5550027,
    SL_BLACKHEAT_GROUP_05   = 5550028,
    SL_BLACKHEAT_GROUP_06   = 5550029,
    SL_BLACKHEAT_GROUP_07   = 5550030,
    SL_BLACKHEAT_GROUP_08   = 5550031,
    SL_BLACKHEAT_GROUP_09   = 5550032,
    SL_BLACKHEAT_GROUP_10   = 5550033,


    SL_SPAWN_GROUP_043      = 5550048,                      // SpawnGroup that stops respawning of first runner (停止第1名信使重生的怪物组)
    SL_SPAWN_GROUP_044      = 5550049,                      // SpawnGroup that stops respawning of 2nd runner (停止第2名信使重生的怪物组)
    SL_SPAWN_GROUP_045      = 5550050,                      // SpawnGroup that stops respawning of 3rd runner (停止第3名信使重生的怪物组)
    SL_SPAWN_GROUP_046      = 5550051,                      // SpawnGroup that stops respawning of 4th runner (停止第4名信使重生的怪物组)
};

// 摩摩尔之怒目标组标识常量
const std::string MURMURS_WRATH_TARGETS_01 = "SL_MURMUR_WRATH_TARGET_01";
const std::string MURMURS_WRATH_TARGETS_02 = "SL_MURMUR_WRATH_TARGET_02";
const std::string MURMURS_WRATH_TARGETS_03 = "SL_MURMUR_WRATH_TARGET_03";

// 暗影迷宫副本数据管理类
class instance_shadow_labyrinth : public ScriptedInstance
{
    public:
        instance_shadow_labyrinth(Map* pMap);

        void Initialize() override;                             // 初始化副本数据与进度

        void OnObjectCreate(GameObject* pGo) override;          // 游戏对象创建登记
        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建登记

        void OnCreatureDeath(Creature* pCreature) override;     // 生物死亡处理（苦修者全部死亡解除赫尔默放逐）
        void OnCreatureRespawn(Creature* creature) override;    // 生物重生处理

        void OnCreatureGroupDespawn(CreatureGroup* pGroup, Creature* pCreature) override; // 怪物组消失回调

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置遭遇战进度数据
        uint32 GetData(uint32 uiType) const override;           // 获取遭遇战进度数据

        void SetData64(uint32 uiType, uint64 uiGuid) override;  // 设置 64 位数据（记录存活苦修者）

        const char* Save() const override { return m_strInstData.c_str(); } // 存盘数据字符串
        void Load(const char* chrIn) override;                  // 读取副本存盘数据

        bool IsHellmawUnbanished() const { return m_sRitualistsAliveGUIDSet.empty(); } // 赫尔默大使是否已解除放逐

    private:
        uint32 m_auiEncounter[MAX_ENCOUNTER];                   // 遭遇战进度数组
        std::string m_strInstData;                              // 副本数据存储字符串

        GuidSet m_sRitualistsAliveGUIDSet;                      // 当前存活的苦修者 GUID 集合
};

#endif
