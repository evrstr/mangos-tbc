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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MANGOS_CREATURE_EAI_MGR_H
#define MANGOS_CREATURE_EAI_MGR_H

#include "Common.h"
#include "CreatureEventAI.h"

// 生物 EventAI 全局脚本与召唤点定义管理器：负责从数据库加载并校验 creature_ai_scripts 与 creature_ai_summons
class CreatureEventAIMgr
{
    public:
        CreatureEventAIMgr() {};
        ~CreatureEventAIMgr() {};

        void LoadCreatureEventAI_Summons(bool check_entry_use);       // 加载 EventAI 预设召唤点坐标表 (creature_ai_summons)
        void LoadCreatureEventAI_Scripts();                             // 加载并校验所有 EventAI 事件与动作脚本 (creature_ai_scripts)

        std::shared_ptr<CreatureEventAI_Event_Map> const& GetCreatureEventEntryAIMap() const { return m_CreatureEventAIEventEntryMap; }
        std::shared_ptr<CreatureEventAI_Event_Map> const& GetCreatureEventGuidAIMap()  const { return m_CreatureEventAIEventGuidMap; }
        CreatureEventAI_Summon_Map const& GetCreatureEventAISummonMap() const { return m_CreatureEventAI_Summon_Map; }
        std::shared_ptr<CreatureEventAI_EventComputedData_Map> const& GetEAIComputedDataMap() const { return m_creatureEventAIComputedDataMap; }

    private:
        void CheckUnusedAISummons();                                    // 检查未被任何 EventAI 脚本引用的召唤点记录

        std::shared_ptr<CreatureEventAI_Event_Map>  m_CreatureEventAIEventEntryMap; // 按生物 Entry 索引的 EventAI 事件列表
        std::shared_ptr<CreatureEventAI_Event_Map>  m_CreatureEventAIEventGuidMap;  // 按生物 DBGUID 索引的 EventAI 事件列表
        CreatureEventAI_Summon_Map m_CreatureEventAI_Summon_Map; // deprecated (已废弃的召唤坐标表)
        std::shared_ptr<CreatureEventAI_EventComputedData_Map> m_creatureEventAIComputedDataMap; // 预计算的事件附加数据缓存
};

#define sEventAIMgr MaNGOS::Singleton<CreatureEventAIMgr>::Instance()
#endif
