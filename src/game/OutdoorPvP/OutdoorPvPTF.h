/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef WORLD_PVP_TF
#define WORLD_PVP_TF

#include "Common.h"
#include "OutdoorPvP.h"
#include "Tools/Language.h"
#include "World/WorldStateDefines.h"

// 泰罗卡森林（Terokkar Forest）灵魂塔野外 PvP 常量定义
enum
{
    MAX_TF_TOWERS                               = 5,

    // gameobjects (五座灵魂塔占领点)
    GO_TOWER_BANNER_WEST                        = 183104,
    GO_TOWER_BANNER_NORTH                       = 183411,
    GO_TOWER_BANNER_EAST                        = 183412,
    GO_TOWER_BANNER_SOUTH_EAST                  = 183413,
    GO_TOWER_BANNER_SOUTH                       = 183414,

    // spells (奥金顿的祝福：全控阵营区域增益)
    SPELL_AUCHINDOUN_BLESSING                   = 33377,

    // timers (区域锁定持续时间：6 小时)
    TIMER_TF_LOCK_TIME                          = 6 * HOUR * IN_MILLISECONDS,
    //TIMER_TF_UPDATE_TIME                        = MINUTE * IN_MILLISECONDS,

    // quests (占领灵魂塔任务)
    QUEST_SPIRITS_OF_AUCHINDOUM_ALLIANCE        = 11505,
    QUEST_SPIRITS_OF_AUCHINDOUM_HORDE           = 11506,

    // events (五座灵魂塔占领/中立化事件 ID)
    EVENT_WEST_TOWER_PROGRESS_ALLIANCE          = 12226,
    EVENT_WEST_TOWER_PROGRESS_HORDE             = 12225,
    EVENT_WEST_TOWER_NEUTRAL_ALLIANCE           = 12228,
    EVENT_WEST_TOWER_NEUTRAL_HORDE              = 12227,

    EVENT_NORTH_TOWER_PROGRESS_ALLIANCE         = 12497,
    EVENT_NORTH_TOWER_PROGRESS_HORDE            = 12496,
    EVENT_NORTH_TOWER_NEUTRAL_ALLIANCE          = 12490,
    EVENT_NORTH_TOWER_NEUTRAL_HORDE             = 12491,

    EVENT_EAST_TOWER_PROGRESS_ALLIANCE          = 12486,
    EVENT_EAST_TOWER_PROGRESS_HORDE             = 12487,
    EVENT_EAST_TOWER_NEUTRAL_ALLIANCE           = 12488,
    EVENT_EAST_TOWER_NEUTRAL_HORDE              = 12489,

    EVENT_SOUTH_EAST_TOWER_PROGRESS_ALLIANCE    = 12499,
    EVENT_SOUTH_EAST_TOWER_PROGRESS_HORDE       = 12498,
    EVENT_SOUTH_EAST_TOWER_NEUTRAL_ALLIANCE     = 12492,
    EVENT_SOUTH_EAST_TOWER_NEUTRAL_HORDE        = 12493,

    EVENT_SOUTH_TOWER_PROGRESS_ALLIANCE         = 12501,
    EVENT_SOUTH_TOWER_PROGRESS_HORDE            = 12500,
    EVENT_SOUTH_TOWER_NEUTRAL_ALLIANCE          = 12494,
    EVENT_SOUTH_TOWER_NEUTRAL_HORDE             = 12495,
};

// 灵魂塔占领事件描述结构
struct TerokkarTowerEvent
{
    uint32  eventEntry;
    Team    team;
    uint32  defenseMessage;
    uint32  worldState;
};

static const TerokkarTowerEvent terokkarTowerEvents[MAX_TF_TOWERS][4] =
{
    {
        {EVENT_WEST_TOWER_PROGRESS_ALLIANCE,        ALLIANCE,   LANG_OPVP_TF_CAPTURE_TOWER_A,   WORLD_STATE_TF_WEST_TOWER_ALLIANCE},
        {EVENT_WEST_TOWER_PROGRESS_HORDE,           HORDE,      LANG_OPVP_TF_CAPTURE_TOWER_H,   WORLD_STATE_TF_WEST_TOWER_HORDE},
        {EVENT_WEST_TOWER_NEUTRAL_HORDE,            TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_A,      WORLD_STATE_TF_WEST_TOWER_NEUTRAL},
        {EVENT_WEST_TOWER_NEUTRAL_ALLIANCE,         TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_H,      WORLD_STATE_TF_WEST_TOWER_NEUTRAL},
    },
    {
        {EVENT_NORTH_TOWER_PROGRESS_ALLIANCE,       ALLIANCE,   LANG_OPVP_TF_CAPTURE_TOWER_A,   WORLD_STATE_TF_NORTH_TOWER_ALLIANCE},
        {EVENT_NORTH_TOWER_PROGRESS_HORDE,          HORDE,      LANG_OPVP_TF_CAPTURE_TOWER_H,   WORLD_STATE_TF_NORTH_TOWER_HORDE},
        {EVENT_NORTH_TOWER_NEUTRAL_HORDE,           TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_A,      WORLD_STATE_TF_NORTH_TOWER_NEUTRAL},
        {EVENT_NORTH_TOWER_NEUTRAL_ALLIANCE,        TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_H,      WORLD_STATE_TF_NORTH_TOWER_NEUTRAL},
    },
    {
        {EVENT_EAST_TOWER_PROGRESS_ALLIANCE,        ALLIANCE,   LANG_OPVP_TF_CAPTURE_TOWER_A,   WORLD_STATE_TF_EAST_TOWER_ALLIANCE},
        {EVENT_EAST_TOWER_PROGRESS_HORDE,           HORDE,      LANG_OPVP_TF_CAPTURE_TOWER_H,   WORLD_STATE_TF_EAST_TOWER_HORDE},
        {EVENT_EAST_TOWER_NEUTRAL_HORDE,            TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_A,      WORLD_STATE_TF_EAST_TOWER_NEUTRAL},
        {EVENT_EAST_TOWER_NEUTRAL_ALLIANCE,         TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_H,      WORLD_STATE_TF_EAST_TOWER_NEUTRAL},
    },
    {
        {EVENT_SOUTH_EAST_TOWER_PROGRESS_ALLIANCE,  ALLIANCE,   LANG_OPVP_TF_CAPTURE_TOWER_A,   WORLD_STATE_TF_SOUTH_EAST_TOWER_ALLIANCE},
        {EVENT_SOUTH_EAST_TOWER_PROGRESS_HORDE,     HORDE,      LANG_OPVP_TF_CAPTURE_TOWER_H,   WORLD_STATE_TF_SOUTH_EAST_TOWER_HORDE},
        {EVENT_SOUTH_EAST_TOWER_NEUTRAL_HORDE,      TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_A,      WORLD_STATE_TF_SOUTH_EAST_TOWER_NEUTRAL},
        {EVENT_SOUTH_EAST_TOWER_NEUTRAL_ALLIANCE,   TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_H,      WORLD_STATE_TF_SOUTH_EAST_TOWER_NEUTRAL},
    },
    {
        {EVENT_SOUTH_TOWER_PROGRESS_ALLIANCE,       ALLIANCE,   LANG_OPVP_TF_CAPTURE_TOWER_A,   WORLD_STATE_TF_SOUTH_TOWER_ALLIANCE},
        {EVENT_SOUTH_TOWER_PROGRESS_HORDE,          HORDE,      LANG_OPVP_TF_CAPTURE_TOWER_H,   WORLD_STATE_TF_SOUTH_TOWER_HORDE},
        {EVENT_SOUTH_TOWER_NEUTRAL_HORDE,           TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_A,      WORLD_STATE_TF_SOUTH_TOWER_NEUTRAL},
        {EVENT_SOUTH_TOWER_NEUTRAL_ALLIANCE,        TEAM_NONE,  LANG_OPVP_TF_LOSE_TOWER_H,      WORLD_STATE_TF_SOUTH_TOWER_NEUTRAL},
    },
};

static const uint32 terokkarTowers[MAX_TF_TOWERS] = {GO_TOWER_BANNER_WEST, GO_TOWER_BANNER_NORTH, GO_TOWER_BANNER_EAST, GO_TOWER_BANNER_SOUTH_EAST, GO_TOWER_BANNER_SOUTH};

// 泰罗卡森林灵魂塔野外 PvP 控制器：管理五座灵魂塔争夺、6 小时区域锁定倒计时与奥金顿全控增益
class OutdoorPvPTF : public OutdoorPvP
{
        friend class OutdoorPvPMgr;

    public:
        OutdoorPvPTF();

        void HandlePlayerEnterZone(Player* player, bool isMainZone) override;
        void HandlePlayerLeaveZone(Player* player, bool isMainZone) override;
        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;
        void SendRemoveWorldStates(Player* player) override;

        bool HandleEvent(uint32 eventId, Object* source, Object* target) override;
        void HandleObjectiveComplete(uint32 eventId, const std::list<Player*>& players, Team team) override;

        void HandleGameObjectCreate(GameObject* go) override;
        void Update(uint32 diff) override;

    private:
        void UpdateTimerWorldState(); // 刷新界面倒计时世界状态数字
        void CalculateTimerWorldStateValues(uint32& firstDigit, uint32& secondDigit, uint32& hoursLeft); // 拆解剩余锁定时间为两位分钟数与小时数

        // process capture events (处理单座灵魂塔占领与中立化)
        bool ProcessCaptureEvent(GameObject* go, uint32 towerId, Team team, uint32 newWorldState);

        void LockZone(GameObject* go, uint32 towerId, Team team, uint32 newWorldState); // 某阵营控制全部 5 座塔时锁定区域
        void UnlockZone(); // 锁定期满后重置全部灵魂塔并解锁争夺

        void LockTowers(const WorldObject* objRef); // 锁定所有灵魂塔占领点滑块
        void ResetTowers(const WorldObject* objRef); // 解锁并重置所有灵魂塔为中立

        uint32 m_towerWorldState[MAX_TF_TOWERS];
        uint32 m_zoneWorldState;

        Team m_towerOwner[MAX_TF_TOWERS];
        Team m_zoneOwner;

        uint32 m_zoneLockTimer;
        //uint32 m_zoneUpdateTimer;

        uint8 m_towersAlliance;
        uint8 m_towersHorde;

        ObjectGuid m_towerBanners[MAX_TF_TOWERS];
};

#endif
