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

#include "AI/ScriptDevAI/include/sc_creature.h"
#include "AI/ScriptDevAI/include/sc_instance.h"
#include "Entities/Creature.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "Spells/Spell.h"
#include "Spells/SpellMgr.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"

// 内部自定义动作 ID
enum
{
    ACTION_CASTING_RESTORE = 1000,                          // 施法后目标恢复动作 ID
    ACTION_ON_KILL_COOLDOWN = 1001,                         // 击杀反应内置冷却恢复动作 ID
};

CombatAI::CombatAI(Creature* creature, uint32 combatActions) : ScriptedAI(creature, combatActions), m_onKillCooldown(false), m_stopTargeting(false)
{
    AddCustomAction(ACTION_CASTING_RESTORE, true, [&]() { HandleTargetRestoration(); });
    AddCustomAction(ACTION_ON_KILL_COOLDOWN, true, [&]() { m_onKillCooldown = false; });
}

// 重置 AI 状态
void CombatAI::Reset()
{
    ScriptedAI::Reset();
    m_onKillCooldown = false;
    m_storedTarget = ObjectGuid();
}

// 延迟瞬发法术动画处理：记录当前目标并在 2 秒后尝试恢复目标
void CombatAI::HandleDelayedInstantAnimation(SpellEntry const* /*spellInfo*/)
{
    m_storedTarget = m_creature->GetTarget() ? m_creature->GetTarget()->GetObjectGuid() : ObjectGuid();
    if (m_storedTarget)
        ResetTimer(ACTION_CASTING_RESTORE, 2000);
}

// 恢复战斗主目标（若当前仍在施法或目标已改变则退出）
void CombatAI::HandleTargetRestoration()
{
    ObjectGuid guid = m_unit->GetTarget() ? m_unit->GetTarget()->GetObjectGuid() : ObjectGuid();
    if (guid != m_storedTarget || m_unit->IsNonMeleeSpellCasted(false))
    {
        m_storedTarget = ObjectGuid();
        return;
    }

    if (m_unit->GetVictim() && !GetCombatScriptStatus())
        m_unit->SetTarget(m_unit->GetVictim());
    else
        m_unit->SetTarget(nullptr);

    m_storedTarget = ObjectGuid();
}

// 检查是否限制目标选择（主动限制、施法暂存锁定或基类限制）
bool CombatAI::IsTargetingRestricted()
{
    return m_stopTargeting || m_storedTarget || ScriptedAI::IsTargetingRestricted();
}

// 受到嘲讽时的响应：快速触发目标恢复
void CombatAI::OnTaunt()
{
    if (m_storedTarget)
        ResetTimer(ACTION_CASTING_RESTORE, 1); // clear on next update (在下次 Update 时清理暂存目标)
}

// 添加击杀玩家时的台词响应
void CombatAI::AddOnKillText(int32 text)
{
    m_onDeathReactions.emplace_back(text, false);
}

// 添加击杀玩家时的音效响应
void CombatAI::AddOnKillSound(int32 soundId)
{
    m_onDeathReactions.emplace_back(soundId, true);
}

// 清除击杀反应列表
void CombatAI::ClearOnKillReactions()
{
    m_onDeathReactions.clear();
}

// 击杀单位回调：击杀玩家时随机播放台词或声音并设置 10 秒冷却
void CombatAI::KilledUnit(Unit* victim)
{
    if (!m_creature->IsAlive() || !victim->IsPlayer())
        return;

    if (!m_onKillCooldown && m_onDeathReactions.size() > 0)
    {
        m_onKillCooldown = true;
        auto selected = m_onDeathReactions[urand(0, m_onDeathReactions.size() - 1)];
        if (selected.second)
            m_creature->PlayDirectSound(selected.first);
        else
            DoBroadcastText(selected.first, m_creature, victim);
        ResetTimer(ACTION_ON_KILL_COOLDOWN, 10000);
    }
}
