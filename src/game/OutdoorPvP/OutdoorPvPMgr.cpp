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

#include "OutdoorPvPMgr.h"
#include "Policies/Singleton.h"
#include "OutdoorPvP.h"
#include "World/World.h"
#include "Log/Log.h"
#include "OutdoorPvPEP.h"
#include "OutdoorPvPHP.h"
#include "OutdoorPvPNA.h"
#include "OutdoorPvPSI.h"
#include "OutdoorPvPTF.h"
#include "OutdoorPvPZM.h"

INSTANTIATE_SINGLETON_1(OutdoorPvPMgr);

OutdoorPvPMgr::OutdoorPvPMgr()
{
    m_updateTimer.SetInterval(TIMER_OPVP_MGR_UPDATE);
    memset(&m_scripts, 0, sizeof(m_scripts));
}

OutdoorPvPMgr::~OutdoorPvPMgr()
{
    for (auto& m_script : m_scripts)
        delete m_script;
}

#define LOAD_OPVP_ZONE(a)                                           \
    if (sWorld.getConfig(CONFIG_BOOL_OUTDOORPVP_##a##_ENABLED))     \
    {                                                               \
        m_scripts[OPVP_ID_##a] = new OutdoorPvP##a();               \
        ++counter;                                                  \
    }
/**
   Function which loads all outdoor pvp scripts (根据世界配置加载并初始化所有启用的野外 PvP 战区脚本)
 */
void OutdoorPvPMgr::InitOutdoorPvP()
{
    uint8 counter = 0;

    LOAD_OPVP_ZONE(SI);
    LOAD_OPVP_ZONE(EP);
    LOAD_OPVP_ZONE(HP);
    LOAD_OPVP_ZONE(ZM);
    LOAD_OPVP_ZONE(TF);
    LOAD_OPVP_ZONE(NA);

    sLog.outString(">> Loaded %u Outdoor PvP zones", counter);
    sLog.outString();
}

// 根据主区域 Zone ID 获取对应的野外 PvP 脚本实例
OutdoorPvP* OutdoorPvPMgr::GetScript(uint32 zoneId)
{
    switch (zoneId)
    {
        case ZONE_ID_SILITHUS:
            return m_scripts[OPVP_ID_SI];
        case ZONE_ID_EASTERN_PLAGUELANDS:
            return m_scripts[OPVP_ID_EP];
        case ZONE_ID_HELLFIRE_PENINSULA:
            return m_scripts[OPVP_ID_HP];
        case ZONE_ID_ZANGARMARSH:
            return m_scripts[OPVP_ID_ZM];
        case ZONE_ID_TEROKKAR_FOREST:
            return m_scripts[OPVP_ID_TF];
        case ZONE_ID_NAGRAND:
            return m_scripts[OPVP_ID_NA];
        default:
            return nullptr;
    }
}

// 根据受区域增益影响的副本 Zone ID 获取对应的野外 PvP 脚本实例
OutdoorPvP* OutdoorPvPMgr::GetScriptOfAffectedZone(uint32 zoneId)
{
    switch (zoneId)
    {
        case ZONE_ID_TEMPLE_OF_AQ:
        case ZONE_ID_RUINS_OF_AQ:
        case ZONE_ID_GATES_OF_AQ:
            return m_scripts[OPVP_ID_SI];
        case ZONE_ID_STRATHOLME:
        case ZONE_ID_SCHOLOMANCE:
            return m_scripts[OPVP_ID_EP];
        case ZONE_ID_HELLFIRE_RAMPARTS:
        case ZONE_ID_HELLFIRE_CITADEL:
        case ZONE_ID_BLOOD_FURNACE:
        case ZONE_ID_SHATTERED_HALLS:
            return m_scripts[OPVP_ID_HP];
        case ZONE_ID_STEAMVAULT:
        case ZONE_ID_UNDERBOG:
        case ZONE_ID_SLAVE_PENS:
            return m_scripts[OPVP_ID_ZM];
        case ZONE_ID_SHADOW_LABYRINTH:
        case ZONE_ID_AUCHENAI_CRYPTS:
        case ZONE_ID_SETHEKK_HALLS:
        case ZONE_ID_MANA_TOMBS:
            return m_scripts[OPVP_ID_TF];
        default:
            return nullptr;
    }
}

/**
   Function that handles the players which enters a specific zone
   (处理玩家进入主战区或关联副本区域的事件分发)

   @param   player to be handled in the event
   @param   zone id used for the current outdoor pvp script
 */
void OutdoorPvPMgr::HandlePlayerEnterZone(Player* player, uint32 zoneId)
{
    if (OutdoorPvP* script = GetScript(zoneId))
        script->HandlePlayerEnterZone(player, true);
    else if (OutdoorPvP* affectedScript = GetScriptOfAffectedZone(zoneId))
        affectedScript->HandlePlayerEnterZone(player, false);
}

/**
   Function that handles the player who leaves a specific zone
   (处理玩家离开主战区或关联副本区域的事件分发)

   @param   player to be handled in the event
   @param   zone id used for the current outdoor pvp script
 */
void OutdoorPvPMgr::HandlePlayerLeaveZone(Player* player, uint32 zoneId)
{
    // teleport: called once from Player::CleanupsBeforeDelete, once from Player::UpdateZone
    // (传送时可能先后由 CleanupsBeforeDelete 与 UpdateZone 触发，内部通过 map 擦除防重)
    if (OutdoorPvP* script = GetScript(zoneId))
        script->HandlePlayerLeaveZone(player, true);
    else if (OutdoorPvP* affectedScript = GetScriptOfAffectedZone(zoneId))
        affectedScript->HandlePlayerLeaveZone(player, false);
}

// 周期性驱动各野外 PvP 战区脚本的定时更新逻辑
void OutdoorPvPMgr::Update(uint32 diff)
{
    m_updateTimer.Update(diff);
    if (!m_updateTimer.Passed())
        return;

    for (auto& m_script : m_scripts)
        if (m_script)
            m_script->Update(m_updateTimer.GetCurrent());

    m_updateTimer.Reset();
}
