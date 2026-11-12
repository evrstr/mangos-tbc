/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef DEF_THE_EYE_H

// 风暴要塞团队副本数据头文件
/* 遭遇战概述：
 * 定义风暴要塞四大首领与门禁枚举。
 */
#define DEF_THE_EYE_H

// 相关常量、法术与文本条目枚举
enum
{
    MAX_ENCOUNTER               = 4,
    MAX_ADVISORS                = 4,

    TYPE_ALAR                   = 0,
    TYPE_SOLARIAN               = 1,
    TYPE_VOIDREAVER             = 2,
    TYPE_KAELTHAS               = 3,

    NPC_ALAR                    = 19514,
    // NPC_ASTROMANCER           = 18805,
    NPC_KAELTHAS                = 19622,

    NPC_CAPERNIAN               = 20062,
    NPC_SANGUINAR               = 20060,
    NPC_TELONICUS               = 20063,
    NPC_THALADRED               = 20064,

    NPC_WORLD_TRIGGER_LARGE     = 22517,

    NPC_EMBER_OF_ALAR           = 19551,

    NPC_NETHERSTRAND_LONGBOW    = 21268,
    NPC_DEVASTATION             = 21269,
    NPC_COSMIC_INFUSER          = 21270,
    NPC_INFINITY_BLADES         = 21271,
    NPC_WARP_SLICER             = 21272,
    NPC_PHASESHIFT_BULWARK      = 21273,
    NPC_STAFF_OF_DISINTEGRATION = 21274,

    GO_ARCANE_DOOR_HORIZ_3      = 184325,               // combat doors for Kael
    GO_ARCANE_DOOR_HORIZ_4      = 184324,
    GO_RAID_DOOR_3              = 184327,
    GO_RAID_DOOR_4              = 184329,               // encounter doors - no longer used since 2.4.0
    GO_ARCANE_DOOR_VERT_3       = 184326,
    GO_ARCANE_DOOR_VERT_4       = 184328,
    GO_KAEL_STATUE_LEFT         = 184597,               // cosmetic objects for Kael encounter
    GO_KAEL_STATUE_RIGHT        = 184596,
    GO_BRIDGE_WINDOW            = 184069,
};

static const uint32 aAdvisors[MAX_ADVISORS] = {NPC_CAPERNIAN, NPC_SANGUINAR, NPC_TELONICUS, NPC_THALADRED};

// instance_the_eye 副本数据管理类
class instance_the_eye : public ScriptedInstance
{
    public:
        instance_the_eye(Map* pMap);

        // 初始化副本数据与状态
        void Initialize() override;
        bool IsEncounterInProgress() const override;

        // 副本生物创建登记
        void OnCreatureCreate(Creature* pCreature) override;
        // 副本生物重生处理
        void OnCreatureRespawn(Creature* creature) override;
        // 副本游戏对象创建登记
        void OnObjectCreate(GameObject* pGo) override;

        // 副本生物脱战脱逃处理
        void OnCreatureEvade(Creature* creature) override;

        // 设置遭遇战状态数据
        void SetData(uint32 uiType, uint32 uiData) override;
        // 获取遭遇战状态数据
        uint32 GetData(uint32 uiType) const override;

        // checks whether to open the doors preventing Kael access
        bool CheckDoorOpening() const;
        void OpenDoors();

        // Save needed for doors
        const char* Save() const override { return m_strInstData.c_str(); }
        void Load(const char* chrIn) override;

    private:
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        uint32 m_uiKaelthasEventPhase;
};

#endif
