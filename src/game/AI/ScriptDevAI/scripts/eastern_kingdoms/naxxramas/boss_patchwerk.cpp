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
SDName: Boss_Patchwerk
SD%Complete: 100
SDComment:
SDCategory: Naxxramas
EndScriptData */

// 帕奇维克（Patchwerk）构造区1号首领战 AI（仇恨打击接重坦与狂暴软狂暴秒杀机制）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/BossAI.h"
#include "naxxramas.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO1                  = 13068,                    // aggro1 台词广播 ID
    SAY_AGGRO2                  = 13069,                    // aggro2 台词广播 ID
    SAY_SLAY                    = 13071,                    // 击杀 台词广播 ID
    SAY_DEATH                   = 13070,                    // 死亡 台词广播 ID

    EMOTE_GENERIC_BERSERK       = 4428,                     // generic狂暴 表情动作 ID
    EMOTE_GENERIC_ENRAGED       = 2384,                     // genericenraged 表情动作 ID

    SPELL_HATEFULSTRIKE_PRIMER  = 28307,                    // hatefulstrike预备引导 法术 ID
    SPELL_HATEFULSTRIKE         = 28308,                    // hatefulstrike 法术 ID
    SPELL_ENRAGE                = 28131,                    // 狂暴 法术 ID
    SPELL_BERSERK               = 26662,                    // 狂暴 法术 ID
    SPELL_SLIMEBOLT             = 32309,                    // slimebolt 法术 ID

    SPELLSET_NORMAL             = 1602801,                  // normal 标识 ID
    SPELLSET_BERSERK            = 1602802,                  // 狂暴 标识 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum PatchwerkActions
{
    PATCHWERK_BERSERK_HP_CHECK,
    PATCHWERK_BERSERK,
    PATCHWER_ACTIONS_MAX,
};

// 首领战 AI：标识 ID（patchwerk）
struct boss_patchwerkAI : public BossAI
{
    boss_patchwerkAI(Creature* creature) : BossAI(creature, PATCHWER_ACTIONS_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        SetDataType(TYPE_PATCHWERK);
        AddOnKillText(SAY_SLAY);
        AddOnDeathText(SAY_DEATH);
        AddOnAggroText(SAY_AGGRO1, SAY_AGGRO2);
        AddTimerlessCombatAction(PATCHWERK_BERSERK_HP_CHECK, true);         // Soft enrage at 5%
        AddCombatAction(PATCHWERK_BERSERK, 7u * MINUTE * IN_MILLISECONDS);  // Basic berserk (Basic狂暴)
    }

    ScriptedInstance* m_instance;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        BossAI::Reset();
        m_creature->SetSpellList(SPELLSET_NORMAL);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case PATCHWERK_BERSERK_HP_CHECK:
            {
                if (m_creature->GetHealthPercent() < 5.0f)
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_ENRAGE) == CAST_OK)
                    {
                        DoBroadcastText(EMOTE_GENERIC_ENRAGED, m_creature);
                        DisableCombatAction(action);
                    }
                }
                break;
            }
            case PATCHWERK_BERSERK:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_BERSERK) == CAST_OK)
                {
                    DoBroadcastText(EMOTE_GENERIC_BERSERK, m_creature);
                    DisableCombatAction(action);
                    m_creature->SetSpellList(SPELLSET_BERSERK);
                }
                break;
            }
            default:
                break;
        }
    }
};

// 28307 - Hateful Strike Primer (28307 - 仇恨 打击 预备引导)
struct HatefulStrikePrimer : public SpellScript
{
    void OnInit(Spell* spell) const override
    {
        spell->SetFilteringScheme(EFFECT_INDEX_0, true, SCHEME_HIGHEST_HP);
    }

    bool OnCheckTarget(const Spell* spell, Unit* target, SpellEffectIndex /*eff*/) const override
    {
        if (!spell->GetCaster()->CanReachWithMeleeAttack(target))
            return false;

        auto const& threatList = spell->GetCaster()->getThreatManager().getThreatList();
        uint32 i = 0;
        for (auto itr = threatList.begin(); itr != threatList.end() && i < 4; ++itr, ++i)
            if ((*itr)->getTarget() == target)
                return true;
        return false;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            if (Unit* caster = spell->GetCaster())
            {
                // Target is filtered in Spell::FilterTargetMap
                if (Unit* unitTarget = spell->GetUnitTarget())
                    caster->CastSpell(unitTarget, SPELL_HATEFULSTRIKE, TRIGGERED_NONE);
            }
        }
    }
};

// 注册并载入脚本模块：boss_patchwerk
void AddSC_boss_patchwerk()
{
    Script* newScript = new Script;
    newScript->Name = "boss_patchwerk";
    newScript->GetAI = &GetNewAIInstance<boss_patchwerkAI>;
    newScript->RegisterSelf();

    RegisterSpellScript<HatefulStrikePrimer>("spell_patchwerk_hatefulstrike");
}
