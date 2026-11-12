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

#include "TotemAI.h"
#include "Entities/Totem.h"
#include "Entities/Creature.h"
#include "Server/DBCStores.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"
#include "Grids/CellImpl.h"

// 许可度检查：仅图腾类型生物适用 TotemAI
int TotemAI::Permissible(const Creature* creature)
{
    if (creature->IsTotem())
        return PERMIT_BASE_PROACTIVE;

    return PERMIT_BASE_NO;
}

TotemAI::TotemAI(Creature* creature) : CreatureEventAI(creature)
{
}

void TotemAI::MoveInLineOfSight(Unit* /*who*/)
{
}

// 脱战处理：停止自身战斗并派发 EventAI 脱战事件
void TotemAI::EnterEvadeMode()
{
    m_creature->CombatStopWithPets(true);

    // Handle Evade events (触发 EAI 脱战事件)
    IncreaseDepthIfNecessary();
    for (auto& i : m_CreatureEventAIList)
    {
        if (i.event.event_type == EVENT_T_EVADE)
            CheckAndReadyEventForExecution(i);
    }
    ProcessEvents();
}

// 图腾主更新循环：校验法术冷却，锁定射程内最近可攻击敌对单位并施放图腾法术
void TotemAI::UpdateAI(const uint32 diff)
{
    UpdateEventTimers(diff);

    if (getTotem().GetTotemType() != TOTEM_ACTIVE)
        return;

    if (!m_creature->IsAlive() || m_creature->IsNonMeleeSpellCasted(false))
        return;

    // Search spell (读取图腾绑定法术模板)
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(getTotem().GetSpell());
    if (!spellInfo)
        return;

    // shorthand CD check (检查法术冷却是否就绪)
    if (!m_creature->IsSpellReady(*spellInfo))
        return;

    // Get spell rangy (查询法术最大射程)
    SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(spellInfo->rangeIndex);
    float maxRange = GetSpellMaxRange(srange);

    // SPELLMOD_RANGE not applied in this place just because nonexistent range mods for attacking totems
    // 攻击型图腾不享受射程天赋修正，直接使用基础法术射程

    // pointer to appropriate target if found any (先尝试复用上一次记住的目标)
    Unit* victim = m_creature->GetMap()->GetUnit(i_victimGuid);

    // Search victim if no, not attackable, or out of range, or friendly (possible in case duel end)
    // 若无缓存目标或原目标失效/超距/变为友方（如决斗结束），重新搜索射程内最近的可攻击目标
    if (!victim ||
            !m_creature->CanAttackOnSight(victim) || !m_creature->IsWithinDistInMap(victim, maxRange) ||
            m_creature->CanAssist(victim) || !victim->IsVisibleForOrDetect(m_creature, m_creature, false))
    {
        victim = nullptr;

        if (maxRange != 0.0f)
        {
            MaNGOS::NearestAttackableUnitInObjectRangeCheck u_check(m_creature, m_creature->GetOwner(), maxRange);
            MaNGOS::UnitLastSearcher<MaNGOS::NearestAttackableUnitInObjectRangeCheck> checker(victim, u_check);
            Cell::VisitAllObjects(m_creature, checker, maxRange);
        }
        else
            victim = m_creature;
    }

    // If have target (找到合法目标则缓存其 GUID、调整朝向并施放图腾法术)
    if (victim)
    {
        // remember (缓存目标 GUID)
        i_victimGuid = victim->GetObjectGuid();

        // attack (面向目标施放法术)
        m_creature->SetInFront(victim);                     // client change orientation by self
        m_creature->CastSpell(victim, getTotem().GetSpell(), TRIGGERED_NONE);
    }
    else
        i_victimGuid.Clear();
}

bool TotemAI::IsVisible(Unit* /*who*/) const
{
    return false;
}

void TotemAI::AttackStart(Unit* /*who*/)
{
}

// 将底层 m_creature 安全强转为 Totem 引用
Totem& TotemAI::getTotem() const
{
    return static_cast<Totem&>(*m_creature);
}

// 法术命中处理：根基图腾 (entry 5925) 在吸收法术失去重定向光环后立即自毁
void TotemAI::SpellHit(Unit* /*unit*/, const SpellEntry* /*spellInfo*/)
{
    // TODO: Give grounding totem SD2
    if (m_creature->GetEntry() == 5925 && !m_creature->HasAura(8178) && m_creature->IsAlive()) // Grounding Totem redirection aura (根基图腾吸收重定向光环)
        m_creature->CastSpell(nullptr, 45317, TRIGGERED_NONE); // Grounding Totem - Suicide spell - verified vs sniff (触发自毁法术)
}
