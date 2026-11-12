/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_OLD_HILLSBRAD_H
#define DEF_OLD_HILLSBRAD_H

/**
 * 旧希尔斯布莱德丘陵：副本数据常量与事件枚举定义
 */

#include "World/WorldStateDefines.h"

// 常量与事件枚举
enum
{
    MAX_ENCOUNTER                   = 9,
    MAX_BARRELS                     = 5,
    MAX_WIPE_COUNTER                = 20,

    TYPE_BARREL_DIVERSION           = 0,                    // barrel event (事件)
    TYPE_DRAKE                      = 1,                    // first boss (说明)
    TYPE_SKARLOC                    = 2,                    // prison to keep - boss (说明)
    TYPE_ESCORT_SKARLOC             = 3,                    // Thrall respawning at Skarloc (说明)
    TYPE_ESCORT_BARN                = 4,                    // Thrall respawning at Barn (说明)
    TYPE_ESCORT_INN                 = 5,                    // Thrall respawning at Inn (说明)
    TYPE_EPOCH_WAVE                 = 6,                    // Epoch Hunter Waves (说明)
    TYPE_EPOCH                      = 7,                    // Boss Epoch Hunter (说明)
    TYPE_THRALL_EVENT               = 8,                    // global Escort Event (护送事件)

    // event npcs (事件)
    NPC_THRALL                      = 17876,                // NPC 条目 ID
    NPC_TARETHA                     = 18887,                // NPC 条目 ID
    NPC_EROZION                     = 18723,                // NPC 条目 ID
    NPC_LODGE_QUEST_TRIGGER         = 20155,                // NPC 条目 ID
    NPC_ESCORT_QUEST_TRIGGER        = 20156,                // NPC 条目 ID
    NPC_ARMORER                     = 18764,                // NPC 条目 ID
    NPC_ORC_PRISONER                = 18598,                // NPC 条目 ID

    NPC_SKARLOCS_MOUNT              = 18798,                // NPC 条目 ID
    NPC_IMAGE_EROZION               = 19438,                // NPC 条目 ID

    // bosses (说明)
    NPC_DRAKE                       = 17848,                // NPC 条目 ID
    NPC_SKARLOC                     = 17862,                // NPC 条目 ID
    NPC_EPOCH                       = 18096,                // NPC 条目 ID

    GO_ROARING_FLAME                = 182592,               // 游戏对象条目 ID
    GO_PRISON_DOOR                  = 184393,               // 游戏对象条目 ID

    // RP scripts (说明)
    NPC_BARTOLO                     = 20365,                // NPC 条目 ID
    NPC_BEGGAR                      = 20432,                // NPC 条目 ID
    NPC_COMMANDER_MOGRAINE          = 20345,                // NPC 条目 ID
    NPC_COT_DESPAWN_TRIGGER         = 19433,                // NPC 条目 ID

    THRALL_PATH_ID                  = 5600000,

    // Spell used to despawn all EscortNPCs (法术消失)
    SPELL_STUN_PHASE_2              = 33937,                // 技能法术 ID
    SPELL_STUN_PHASE_3              = 34016,                // 技能法术 ID
    SPELL_STUN_PHASE_4              = 34023,                // 技能法术 ID
    SPELL_STUN_PHASE_5              = 34024,                // 技能法术 ID

    // Taretha aura when Epoch Hunter spawns (刷新)
    SPELL_SHADOW_SPIKE              = 33125,                // 技能法术 ID

    // reset texts (重置)
    SAY_ERONZION_RESET_THRALL       = 16729,                // 剧情台词/喊话文本 ID
    SAY_ERONZION_RESET_LAST         = 16731,                // 剧情台词/喊话文本 ID
};

// instance_old_hillsbrad 副本数据管理脚本
class instance_old_hillsbrad : public ScriptedInstance, public TimerManager
{
    public:
        instance_old_hillsbrad(Map* pMap);
        ~instance_old_hillsbrad() {}

        void Initialize() override;                         // 初始化副本数据与状态

        void OnPlayerEnter(Player* pPlayer) override;
        void OnCreatureCreate(Creature* pCreature) override;    // 生物创建/加载至副本地图时的回调
        void OnCreatureRespawn(Creature* creature) override;
        void OnObjectCreate(GameObject* pGo) override;      // 游戏对象创建/加载至副本地图时的回调

        void OnCreatureEnterCombat(Creature* pCreature) override;
        void OnCreatureEvade(Creature* pCreature) override;
        void OnCreatureDeath(Creature* pCreature) override;    // 生物在副本地图中死亡回调
        void OnCreatureDespawn(Creature* pCreature) override;
        void OnCreatureGroupDespawn(CreatureGroup* group, Creature* creature) override;

        void SetData(uint32 uiType, uint32 uiData) override;    // 设置自定义状态数据
        uint32 GetData(uint32 uiType) const override;       // 获取自定义状态数据

        uint32 GetThrallEventCount() const { return m_uiThrallEventCount; }

        const char* Save() const override { return m_strInstData.c_str(); }    // 序列化副本进度数据供存盘
        void Load(const char* chrIn) override;              // 反序列化并恢复副本进度数据

        bool CheckConditionCriteriaMeet(Player const* pPlayer, uint32 uiInstanceConditionId, WorldObject const* pConditionSource, uint32 conditionSourceType) const override;

        void Update(const uint32 diff) override;

        void ShowChatCommands(ChatHandler* handler) override;
        void ExecuteChatCommand(ChatHandler* handler, char* args) override;

        TimePoint m_beggarScriptTime;

    protected:
        void HandleThrallRelocation();
        void UpdateLodgeQuestCredit();
        void UpdateEscortQuestCredit();
        void DoBarrelRP();
        void SpawnWave();

        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        uint32 m_uiBarrelCount;
        uint32 m_uiThrallEventCount;
        uint8 m_uiDrakeStep;
        uint8 m_uiWave;
};

#endif
