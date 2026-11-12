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
SDName: Generic_Creature
SD%Complete: 80
SDComment: Should be replaced with core based AI (通用野怪基础 AI，支持自动施法与增益)
SDCategory: Creatures
EndScriptData */

#include "AI/ScriptDevAI/include/sc_common.h"

#define GENERIC_CREATURE_COOLDOWN   5000                    // 通用生物公共冷却时间 (5秒)

// 通用野怪 AI 结构体 (generic_creatureAI)
struct generic_creatureAI : public ScriptedAI
{
    generic_creatureAI(Creature* pCreature) : ScriptedAI(pCreature) {Reset();}

    uint32 GlobalCooldown;      // This variable acts like the global cooldown that players have (1.5 seconds) (模拟公共冷却时间)
    uint32 BuffTimer;           // This variable keeps track of buffs (脱战自我增益冷却计时)
    bool IsSelfRooted;          // 施法时自定身标记

    // 重置状态
    void Reset() override
    {
        GlobalCooldown = 0;
        BuffTimer = 0;          // Rebuff as soon as we can (尽快施加增益)
        IsSelfRooted = false;
    }

    // 进入战斗：若无法直接近战击中目标则定身准备施法
    void Aggro(Unit* who) override
    {
        if (!m_creature->CanReachWithMeleeAttack(who))
        {
            IsSelfRooted = true;
        }
    }

    // AI 核心更新：脱战增益与进战技能/近战循环
    void UpdateAI(const uint32 diff) override
    {
        // Always decrease our global cooldown first (递减公共冷却)
        if (GlobalCooldown > diff)
            GlobalCooldown -= diff;
        else GlobalCooldown = 0;

        // Buff timer (only buff when we are alive and not in combat (脱战且存活时自动补增益)
        if (!m_creature->IsInCombat() && m_creature->IsAlive())
        {
            if (BuffTimer < diff)
            {
                // Find a spell that targets friendly and applies an aura (these are generally buffs) (检索友方光环增益法术)
                SpellEntry const* info = SelectSpell(m_creature, -1, -1, SELECT_TARGET_ANY_FRIEND, 0, 0, 0, 0, SELECT_EFFECT_AURA);

                if (info && !GlobalCooldown)
                {
                    // Cast the buff spell (施放增益法术)
                    m_creature->CastSpell(m_creature, info, TRIGGERED_NONE);

                    // Set our global cooldown (进入公共冷却)
                    GlobalCooldown = GENERIC_CREATURE_COOLDOWN;

                    // Set our timer to 10 minutes before rebuff (10 分钟后再次补充)
                    BuffTimer = 600000;
                }// Try agian in 30 seconds (30 秒后重试)
                else BuffTimer = 30000;
            }
            else BuffTimer -= diff;
        }

        // Return since we have no target (无战斗目标则返回)
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // Return if we already cast a spell (当前正在施法中则返回)
        if (m_creature->IsNonMeleeSpellCasted(false))
            return;

        // If we are within range melee the target (近战距离内攻击目标)
        if (m_creature->CanReachWithMeleeAttack(m_creature->GetVictim()))
        {
            // Make sure our attack is ready (攻击冷却就绪)
            if (m_creature->isAttackReady())
            {
                bool Healing = false;
                SpellEntry const* info = nullptr;

                // Select a healing spell if less than 30% hp (生命低于 30% 时优先检索治疗法术)
                if (m_creature->GetHealthPercent() < 30.0f)
                    info = SelectSpell(m_creature, -1, -1, SELECT_TARGET_ANY_FRIEND, 0, 0, 0, 0, SELECT_EFFECT_HEALING);

                // No healing spell available, select a hostile spell (无治疗法术则检索敌对技能)
                if (info) Healing = true;
                else info = SelectSpell(m_creature->GetVictim(), -1, -1, SELECT_TARGET_ANY_ENEMY, 0, 0, 0, 0, SELECT_EFFECT_DONTCARE);

                // 50% chance if elite or higher, 20% chance if not, to replace our white hit with a spell (精英 50% 概率、普通 20% 概率用法术替代普攻)
                if (info && (rand() % (m_creature->GetCreatureInfo()->Rank > 1 ? 2 : 5) == 0) && !GlobalCooldown)
                {
                    // Cast the spell (施放技能)
                    if (Healing)
                        m_creature->CastSpell(m_creature, info, TRIGGERED_NONE);
                    else
                        m_creature->CastSpell(m_creature->GetVictim(), info, TRIGGERED_NONE);

                    // Set our global cooldown (触发公共冷却)
                    GlobalCooldown = GENERIC_CREATURE_COOLDOWN;
                }
                else m_creature->AttackerStateUpdate(m_creature->GetVictim());

                m_creature->resetAttackTimer();
            }
        }
        else
        {
            bool Healing = false;
            SpellEntry const* info = nullptr;

            // Select a healing spell if less than 30% hp ONLY 33% of the time (生命低于 30% 且 33% 概率尝试治疗)
            if (m_creature->GetHealthPercent() < 30.0f && !urand(0, 2))
                info = SelectSpell(m_creature, -1, -1, SELECT_TARGET_ANY_FRIEND, 0, 0, 0, 0, SELECT_EFFECT_HEALING);

            // No healing spell available, See if we can cast a ranged spell (Range must be greater than ATTACK_DISTANCE) (无治疗则检索远程敌对法术)
            if (info) Healing = true;
            else info = SelectSpell(m_creature->GetVictim(), -1, -1, SELECT_TARGET_ANY_ENEMY, 0, 0, ATTACK_DISTANCE, 0, SELECT_EFFECT_DONTCARE);

            // Found a spell, check if we arn't on cooldown (找到可用法术且不在冷却中)
            if (info && !GlobalCooldown)
            {
                // Stop us and set the movement generator (定身并施法)
                IsSelfRooted = true;

                // Cast spell
                if (Healing)
                    m_creature->CastSpell(m_creature, info, TRIGGERED_NONE);
                else
                    m_creature->CastSpell(m_creature->GetVictim(), info, TRIGGERED_NONE);

                // Set our global cooldown
                GlobalCooldown = GENERIC_CREATURE_COOLDOWN;
            }// If no spells available and we arn't moving run to target (无法术可用则恢复向目标移动)
            else if (IsSelfRooted)
            {
                // Cancel our current spell and then allow movement agian (打断法术恢复移动)
                m_creature->InterruptNonMeleeSpells(false);
                IsSelfRooted = false;
            }
        }
    }
};

// 创建通用生物 AI 实例
UnitAI* GetAI_generic_creature(Creature* pCreature)
{
    return new generic_creatureAI(pCreature);
}

// 注册通用生物脚本
void AddSC_generic_creature()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "generic_creature";
    pNewScript->GetAI = &GetAI_generic_creature;
    pNewScript->RegisterSelf(false);
}
