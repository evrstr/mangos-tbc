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
SDName: Boss_Hakkar
SD%Complete: 100
SDComment:
SDCategory: Zul'Gurub
EndScriptData */

// 夺灵者哈卡（Hakkar the Soulflayer）最终首领战 AI（堕落之血瘟疫传播、血液虹吸治疗与击杀哈卡之子吃毒血破盾）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "zulgurub.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO                   = -1309020,                 // 开战 台词广播 ID
    SAY_FLEEING                 = -1309021,                 // fleeing 台词广播 ID

    SPELL_BLOOD_SIPHON          = 24324,                    // triggers 24322 or 24323 on caster
    SPELL_CORRUPTED_BLOOD       = 24328,                    // corrupted鲜血 法术 ID
    SPELL_CAUSE_INSANITY        = 24327,                    // causeinsanity 法术 ID
    SPELL_WILL_OF_HAKKAR        = 24178,                    // willof哈卡 法术 ID
    SPELL_ENRAGE                = 24318,                    // 狂暴 法术 ID
    SPELL_DOUBLE_ATTACK         = 19818,                    // 双重攻击 法术 ID

    // The Aspects of all High Priests
    SPELL_ASPECT_OF_JEKLIK      = 24687,                    // aspectof耶克里克 法术 ID
    SPELL_ASPECT_OF_VENOXIS     = 24688,                    // aspectof温诺希斯 法术 ID
    SPELL_ASPECT_OF_MARLI       = 24686,                    // aspectof玛尔里 法术 ID
    SPELL_ASPECT_OF_THEKAL      = 24689,                    // aspectof塞卡尔 法术 ID
    SPELL_ASPECT_OF_ARLOKK      = 24690                     // aspectof阿罗克 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum HakkarActions
{
    HAKKAR_ENRAGE_LOW,
    HAKKAR_ACTION_MAX,
};

// 首领战 AI：标识 ID（hakkar）
struct boss_hakkarAI : public CombatAI
{
    boss_hakkarAI(Creature* creature) : CombatAI(creature, HAKKAR_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddTimerlessCombatAction(HAKKAR_ENRAGE_LOW, true);
    }

    ScriptedInstance* m_instance;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();

        DoCastSpellIfCan(nullptr, SPELL_DOUBLE_ATTACK, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);

        InitiateHakkarPowerStacks();
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
    }

    // For each of the High Priests that is alive, update Hakkar's Power Stacks (updating Hakkar's HP)
    void InitiateHakkarPowerStacks()
    {
        m_creature->RemoveAurasDueToSpell(SPELL_HAKKAR_POWER);
        for (uint8 i = 0; i < MAX_PRIESTS; i++)
        {
            if (m_instance->GetData(i) != DONE)
                m_creature->CastSpell(nullptr, SPELL_HAKKAR_POWER, TRIGGERED_NONE);
        }
    }

    void ExecuteAction(uint32 action) override
    {
        if (action == HAKKAR_ENRAGE_LOW)
        {
            if (m_creature->GetHealthPercent() < 5.f)
            {
                m_creature->RemoveSpellCooldown(SPELL_ENRAGE);
                DisableCombatAction(action);
            }
        }
    }
};

// 法术脚本逻辑实现：标识 ID（HakkarPowerDown）
struct HakkarPowerDown : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
        {
            if (Unit* target = spell->GetUnitTarget())
            {
                if (target->HasAura(SPELL_HAKKAR_POWER))
                    target->RemoveAuraStack(SPELL_HAKKAR_POWER);
            }
        }
    }
};

// 法术脚本逻辑实现：标识 ID（BloodSiphon）
struct BloodSiphon : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx != EFFECT_INDEX_0)
            return;

        Unit* target = spell->GetUnitTarget();
        target->CastSpell(spell->GetCaster(), target->HasAura(24321) ? 24323 : 24322, TRIGGERED_OLD_TRIGGERED);
    }
};

// 注册并载入脚本模块：boss_hakkar
void AddSC_boss_hakkar()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_hakkar";
    pNewScript->GetAI = &GetNewAIInstance<boss_hakkarAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<HakkarPowerDown>("spell_hakkar_power_down");
    RegisterSpellScript<BloodSiphon>("spell_hakkar_blood_siphon");
}
