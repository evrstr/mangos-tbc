/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
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

/* ScriptData
SDName: Instance_Onyxias_Lair
SD%Complete: 80%
SDComment: Quel'Serrar forging event is missing. Whelp summoning would be improved by GO casting.
SDCategory: Onyxia's Lair
EndScriptData */

/**
 * 奥妮克希亚的巢穴 (Onyxia's Lair) 副本数据管理脚本
 * 负责追踪奥妮克希亚战斗阶段（地面/起飞/狂暴）、幼龙刷新及奥妮克希亚守卫的重置与清理
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "onyxias_lair.h"

instance_onyxias_lair::instance_onyxias_lair(Map* pMap) : ScriptedInstance(pMap) // 构造函数：初始化副本状态
{
    Initialize();
}

void instance_onyxias_lair::Initialize()                     // 初始化遭遇战状态为 NOT_STARTED
{
    m_uiEncounter = NOT_STARTED;
    m_tPhaseTwoStart = time(nullptr);
}

bool instance_onyxias_lair::IsEncounterInProgress() const    // 检查战斗是否进行中或处于起飞升空阶段
{
    return m_uiEncounter == IN_PROGRESS || m_uiEncounter >= DATA_LIFTOFF;
}

void instance_onyxias_lair::OnCreatureCreate(Creature* pCreature) // 缓存触发器 NPC GUID
{
    switch (pCreature->GetEntry())
    {
        case NPC_ONYXIA_TRIGGER:
            m_npcEntryGuidStore[NPC_ONYXIA_TRIGGER] = pCreature->GetObjectGuid();
            break;
    }
}

void instance_onyxias_lair::OnCreatureDeath(Creature* pCreature) // 记录守卫阵亡 GUID
{
    switch (pCreature->GetEntry())
    {
        // Keep track of all killed Warders to respawn them while Onyxia is in combat (记录阵亡守卫，用于在开战时复活它们)
        case NPC_ONYXIAN_WARDER:
            m_lWarderGUIDList.push_back(pCreature->GetObjectGuid());
            break;
    }
}

void instance_onyxias_lair::OnObjectCreate(GameObject* pGo)  // 游戏对象生成回调：通过触发器召唤小龙
{
    switch (pGo->GetEntry())
    {
        case GO_WHELP_SPAWNER:
            if (Creature* pTrigger = GetSingleCreatureFromStorage(NPC_ONYXIA_TRIGGER))
            {
                pTrigger->SummonCreature(NPC_ONYXIA_WHELP, pGo->GetPositionX(), pGo->GetPositionY(), pGo->GetPositionZ(), 0.0f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, MINUTE * IN_MILLISECONDS);
                // TODO when GO spell casting is added in core, replace above code by (or something similar): (待核心支持游戏对象施法后优化此处实现)
                // pGO->CastSpell(pGO, SPELL_SUMMONWHELP, TRIGGERED_NONE);
            }
            break;
    }
}

void instance_onyxias_lair::SetData(uint32 uiType, uint32 uiData) // 设置副本状态数据
{
    if (uiType != TYPE_ONYXIA)
        return;

    m_uiEncounter = uiData;

    switch (uiData)
    {
        case DATA_LIFTOFF:
            m_tPhaseTwoStart = time(nullptr);
            break;
        case IN_PROGRESS:
            // Respawn dead Onyxian Warders (开战时重新刷新阵亡的守卫)
            for (GuidList::const_iterator itr = m_lWarderGUIDList.begin(); itr != m_lWarderGUIDList.end(); ++itr)
            {
                if (Creature* pWarder = instance->GetCreature(*itr))
                {
                    if (!pWarder->IsAlive())
                        pWarder->Respawn();
                }
            }
            break;
        case FAIL:
        case DONE:
            // Despawn Onyxian Warders that were respawned during fight (灭团或击杀后清理战斗中刷新的守卫)
            for (GuidList::const_iterator itr = m_lWarderGUIDList.begin(); itr != m_lWarderGUIDList.end(); ++itr)
            {
                if (Creature* pWarder = instance->GetCreature(*itr))
                    pWarder->ForcedDespawn();
            }
            break;
    }

    // Currently no reason to save anything (奥妮克希亚为单首领副本，暂无存盘需求)
}

InstanceData* GetInstanceData_instance_onyxias_lair(Map* pMap) // 获取奥妮克希亚巢穴副本数据实例
{
    return new instance_onyxias_lair(pMap);
}

void AddSC_instance_onyxias_lair()                           // 注册副本数据脚本
{
    Script* pNewScript = new Script;
    pNewScript->Name = "instance_onyxias_lair";
    pNewScript->GetInstanceData = &GetInstanceData_instance_onyxias_lair;
    pNewScript->RegisterSelf();
}
