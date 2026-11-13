/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

// 奥达曼（Uldaman）全局定义头文件

#ifndef DEF_ULDAMAN_H                                       // 防止头文件重复包含
#define DEF_ULDAMAN_H

#include "World/WorldStateDefines.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    MAX_ENCOUNTER               = 2,                        // encounter 标识 ID

    TYPE_ALTAR_EVENT            = 1,                        // altar事件 遭遇战进度数据标识 ID
    TYPE_ARCHAEDAS              = 2,                        // 阿扎达斯 遭遇战进度数据标识 ID
    DATA_EVENT_STARTER          = 3,                        // 事件starter 遭遇战进度数据标识 ID

    GO_TEMPLE_DOOR_UPPER        = 124367,                   // temple门upper 游戏对象 Entry ID
    GO_TEMPLE_DOOR_LOWER        = 141869,                   // temple门lower 游戏对象 Entry ID
    GO_ANCIENT_VAULT            = 124369,                   // ancient宝库 游戏对象 Entry ID
    GO_ANCIENT_TREASURE         = 141979,                   // ancienttreasure 游戏对象 Entry ID

    NPC_ARCHAEDAS               = 2748,                     // 阿扎达斯 Entry ID
    NPC_CUSTODIAN               = 7309,                     // custodian Entry ID
    NPC_HALLSHAPER              = 7077,                     // hallshaper Entry ID
    NPC_GUARDIAN                = 7076,                     // guardian Entry ID
    NPC_VAULT_WARDER            = 10120,                    // 宝库守卫者 Entry ID
    NPC_STONE_KEEPER            = 4857,                     // stonekeeper Entry ID

    SPELL_STONED                = 10255,                    // stoned 法术 ID
    SPELL_FREEZE_ANIM           = 16245,                    // freezeanim 法术 ID

    EVENT_ID_ALTAR_KEEPER       = 2228,                     // spell 11568 (法术11568)
    EVENT_ID_ALTAR_ARCHAEDAS    = 2268                      // spell 10340 (法术10340)
};

// 副本进度与状态机管理类：标识 ID（instance_uldaman）
class instance_uldaman : public ScriptedInstance
{
    public:
        instance_uldaman(Map* pMap);
        ~instance_uldaman() {}

        // 初始化副本数据结构
        void Initialize() override;

        // 副本内游戏对象创建加载回调
        void OnObjectCreate(GameObject* pGo) override;
        // 副本内生物创建加载回调
        void OnCreatureCreate(Creature* pCreature) override;

        // 副本内生物死亡回调
        void OnCreatureDeath(Creature* pCreature) override;
        void OnCreatureEvade(Creature* pCreature) override;

        void OnCreatureGroupDespawn(CreatureGroup* pGroup, Creature* pCreature) override;

        // 副本全局状态周期性轮询更新
        void Update(const uint32 diff) override;

        // 更新遭遇战或自定义数据状态
        void SetData(uint32 uiType, uint32 uiData) override;
        void SetData64(uint32 uiData, uint64 uiGuid) override;
        // 获取遭遇战或自定义数据状态
        uint32 GetData(uint32 uiType) const override;
        uint64 GetData64(uint32 uiData) const override;

        void StartEvent(uint32 uiEventId, Player* pPlayer);

        Creature* GetClosestDwarfNotInCombat(Creature* pSearcher);

        // 序列化副本进度数据
        const char* Save() const override { return m_strInstData.c_str(); }
        // 反序列化加载副本进度数据
        void Load(const char* chrIn) override;

    protected:
        void DoResetKeeperEvent();

        uint32 m_auiEncounter[MAX_ENCOUNTER];               // 各遭遇战状态记录数组
        std::string m_strInstData;                          // 序列化持久化保存的进度字符串

        ObjectGuid m_playerGuid;                            // player 实体 GUID

        uint32 m_uiKeeperCooldown;

        GuidSet m_sWardens;
        GuidSet m_sKeepers;
        GuidSet m_sDeadKeepers;
};

#endif                                                      // 结束头文件防重复包含宏
