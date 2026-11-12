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
SDName: Boss_Baroness_Anastari
SD%Complete: 100
SDComment:
SDCategory: Stratholme
EndScriptData */

// 安娜丝塔丽男爵夫人（Baroness Anastari）首领战 AI（附身心控玩家、女妖哀嚎与女妖诅咒）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "Spells/Scripts/SpellScript.h"
#include "Spells/SpellAuras.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SPELL_BANSHEE_WAIL      = 16565,                        // 女妖wail 法术 ID
    SPELL_BANSHEE_CURSE     = 16867,                        // 女妖诅咒 法术 ID
    SPELL_SILENCE           = 18327,                        // 沉默 法术 ID
    SPELL_POSSESS           = 17244,                        // possess 法术 ID
    SPELL_POSSESSED         = 17246,                        // possessed 法术 ID
    SPELL_POSSESS_INV       = 17250,        // baroness becomes invisible while possessing a target
};

// 首领战 AI：安娜丝塔丽（baroness_anastari）
struct boss_baroness_anastariAI : public ScriptedAI
{
    boss_baroness_anastariAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        // Note: This should be generic Player AI behaviour - if no target - break charm
        m_creature->GetCombatManager().SetLeashingCheck([](Unit* unit, float, float, float) -> bool
        {
            if (unit->HasAura(SPELL_POSSESS_INV) && unit->getThreatManager().getThreatList().size() <= 1)
            {
                unit->RemoveAurasDueToSpell(SPELL_POSSESS_INV);
                unit->BreakCharmOutgoing();
                return true;
            }
            return false;
        });
        Reset();
    }

    uint32 m_uiBansheeWailTimer;                            // BansheeWail 技能计时器
    uint32 m_uiBansheeCurseTimer;                           // BansheeCurse 技能计时器
    uint32 m_uiSilenceTimer;                                // Silence 技能计时器
    uint32 m_uiPossessTimer;                                // Possess 技能计时器
    uint32 m_uiPossessEndTimer;                             // PossessEnd 技能计时器

    ObjectGuid m_possessedPlayer;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiBansheeWailTimer    = 0;
        m_uiBansheeCurseTimer   = 10000;
        m_uiSilenceTimer        = 25000;
        m_uiPossessTimer        = 15000;
        m_uiPossessEndTimer     = 0;
    }

    // 进入脱战闪避模式
    void EnterEvadeMode() override
    {
        // If it's invisible don't evade
        if (m_uiPossessEndTimer)
            return;

        ScriptedAI::EnterEvadeMode();
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiPossessEndTimer)
        {
            // Check if the possessed player has been damaged
            if (m_uiPossessEndTimer <= uiDiff)
            {
                // If aura has expired, return to fight
                if (!m_creature->HasAura(SPELL_POSSESS_INV))
                {
                    m_uiPossessEndTimer = 0;
                    return;
                }

                // Check for possessed player
                Player* pPlayer = m_creature->GetMap()->GetPlayer(m_possessedPlayer);
                if (!pPlayer || !pPlayer->IsAlive())
                {
                    m_creature->RemoveAurasDueToSpell(SPELL_POSSESS_INV);
                    m_uiPossessEndTimer = 0;
                    return;
                }

                // If possessed player has less than 50% health
                if (pPlayer->GetHealth() <= pPlayer->GetMaxHealth() * .5f)
                {
                    m_creature->RemoveAurasDueToSpell(SPELL_POSSESS_INV);
                    pPlayer->RemoveAurasDueToSpell(SPELL_POSSESSED);
                    pPlayer->RemoveAurasDueToSpell(SPELL_POSSESS);
                    m_uiPossessEndTimer = 0;
                    return;
                }

                m_uiPossessEndTimer = 1000;
            }
            else
                m_uiPossessEndTimer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // BansheeWail
        if (m_uiBansheeWailTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_BANSHEE_WAIL) == CAST_OK)
                    m_uiBansheeWailTimer = urand(2000, 3000);
            }
        }
        else
            m_uiBansheeWailTimer -= uiDiff;

        // BansheeCurse
        if (m_uiBansheeCurseTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_BANSHEE_CURSE) == CAST_OK)
                m_uiBansheeCurseTimer = 20000;
        }
        else
            m_uiBansheeCurseTimer -= uiDiff;

        // Silence (沉默)
        if (m_uiSilenceTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_SILENCE) == CAST_OK)
                    m_uiSilenceTimer = 25000;
            }
        }
        else
            m_uiSilenceTimer -= uiDiff;

        // Possess
        if (m_uiPossessTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1, SPELL_POSSESS, SELECT_FLAG_PLAYER))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_POSSESS) == CAST_OK)
                {
                    m_possessedPlayer = pTarget->GetObjectGuid();
                    m_uiPossessEndTimer = 1000;
                    m_uiPossessTimer = 30000;
                }
            }
        }
        else
            m_uiPossessTimer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

struct AnastariPossess : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (apply)
        {
            Unit* caster = aura->GetCaster();
            if (caster)
            {
                caster->CastSpell(aura->GetTarget(), SPELL_POSSESSED, TRIGGERED_OLD_TRIGGERED);
                caster->CastSpell(nullptr, SPELL_POSSESS_INV, TRIGGERED_OLD_TRIGGERED);
            }
            aura->ForcePeriodicity(1000);
        }
        else
        {
            if (Unit* caster = aura->GetCaster())
                caster->RemoveAurasDueToSpell(SPELL_POSSESS_INV);
            aura->GetTarget()->RemoveAurasDueToSpell(SPELL_POSSESSED);
        }
    }

    void OnPeriodicTickEnd(Aura* aura) const override
    {
        if (aura->GetTarget()->GetHealthPercent() < 50.f)
            aura->GetTarget()->RemoveAurasDueToSpell(SPELL_POSSESS);
    }
};

// 注册并载入脚本模块：boss_baroness_anastari
void AddSC_boss_baroness_anastari()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_baroness_anastari";
    pNewScript->GetAI = &GetNewAIInstance<boss_baroness_anastariAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<AnastariPossess>("spell_anastari_possess");
}
