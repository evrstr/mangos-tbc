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
SDName: Boss_Loatheb
SD%Complete: 100
SDComment:
SDCategory: Naxxramas
EndScriptData */

// 洛欧塞布（Loatheb）瘟疫区3号首领战 AI（死灵光环治疗封锁、剧毒光环与孢子暴击增益）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/BossAI.h"
#include "naxxramas.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SPELL_CORRUPTED_MIND    = 29201,            // this triggers the following spells on targets (based on class): 29185, 29194, 29196, 29198
    SPELL_POISON_AURA       = 29865,                        // 毒药光环 法术 ID
    SPELL_INEVITABLE_DOOM   = 29204,                        // inevitabledoom 法术 ID
    SPELL_SUMMON_SPORE      = 29234,                        // 召唤孢子 法术 ID
    SPELL_REMOVE_CURSE      = 30281,                        // remove诅咒 法术 ID

    NPC_SPORE               = 16286,                        // 孢子 Entry ID

    SPELLSET_LOATHEB        = 1601101,                      // 洛欧塞布 标识 ID
    SPELLSET_LOATHEB_P2     = 1601102,                      // 洛欧塞布p2 标识 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum LoathebActions
{
    LOATHEB_SOFT_ENRAGE,
    LOATHEB_ACTION_MAX,
};

// 首领战 AI：标识 ID（loatheb）
struct boss_loathebAI : public BossAI
{
    boss_loathebAI(Creature* creature) : BossAI(creature, LOATHEB_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        SetDataType(TYPE_LOATHEB);
        AddCombatAction(LOATHEB_SOFT_ENRAGE, 5min);
    }

    ScriptedInstance* m_instance;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        BossAI::Reset();
        m_creature->SetSpellList(SPELLSET_LOATHEB);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        if (summoned->GetEntry() != NPC_SPORE)
            return;

        summoned->AI()->AttackStart(m_creature);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case LOATHEB_SOFT_ENRAGE:
            {
                m_creature->SetSpellList(SPELLSET_LOATHEB_P2);
                DisableCombatAction(action);
                return;
            }
            default: break;
        }
    }
};

// 29201 - Corrupted Mind
struct CorruptedMind : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            if (Unit* target = spell->GetUnitTarget())
            {
                // This spell only works on players as it triggers spells that override spell class scripts
                if (!target->IsPlayer())
                    return;

                // Determine which sub-spell to trigger for each healing class
                uint32 spellId = 0;
                switch (target->getClass())
                {
                    case CLASS_PALADIN: spellId = 29196; break;
                    case CLASS_PRIEST: spellId = 29185; break;
                    case CLASS_SHAMAN: spellId = 29198; break;
                    case CLASS_DRUID: spellId = 29194; break;
                    default: break;
                }
                if (spellId != 0)
                    spell->GetCaster()->CastSpell(target, spellId, TRIGGERED_OLD_TRIGGERED, nullptr);
            }
        }
    }
};

// 注册并载入脚本模块：boss_loatheb
void AddSC_boss_loatheb()
{
    Script* newScript = new Script;
    newScript->Name = "boss_loatheb";
    newScript->GetAI = &GetNewAIInstance<boss_loathebAI>;
    newScript->RegisterSelf();

    RegisterSpellScript<CorruptedMind>("spell_loatheb_corrupted_mind");
}
