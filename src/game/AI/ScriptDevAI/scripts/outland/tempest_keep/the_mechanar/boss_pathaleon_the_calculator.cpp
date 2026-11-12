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
SDName: Boss Pathaleon the Calculator
SD%Complete: 95
SDComment: Timers may need update.
SDCategory: Tempest Keep, The Mechanar
EndScriptData */

// 计算者帕萨雷恩（能源舰最终首领）
/* 遭遇战概述：
 * 计算者帕萨雷恩是凯尔萨斯的重要谋士，战斗中施放统御意志心控玩家，低血量狂暴并召唤多只虚空灵力微粒。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "mechanar.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 相关常量、法术与文本条目枚举
enum
{
    SAY_AGGRO                       = 20041,
    SAY_DOMINATION_1                = 18716,
    SAY_DOMINATION_2                = 20040,
    SAY_SUMMON                      = 18717,
    SAY_ENRAGE                      = 18725,
    SAY_SLAY_1                      = 20042,
    SAY_SLAY_2                      = 20043,
    SAY_DEATH                       = 20044,

    // Spells to be casted
    SPELL_MANA_TAP                  = 36021,
    SPELL_ARCANE_TORRENT            = 36022,
    SPELL_DOMINATION                = 35280,
    SPELL_ARCANE_EXPLOSION_H        = 15453,
    SPELL_ENRAGE                    = 36992,
    SPELL_SUICIDE                   = 35301,        // kill the Nether Wraiths
    SPELL_DISGRUNTLED_ANGER         = 35289,        // empower a Nether Wraith

    SPELL_SUMMON_NETHER_WRAITH_1    = 35285,
    SPELL_SUMMON_NETHER_WRAITH_2    = 35286,        // not used post nerf it seems
    SPELL_SUMMON_NETHER_WRAITH_3    = 35287,
    SPELL_SUMMON_NETHER_WRAITH_4    = 35288,

    SPELL_SUMMON_NETHER_WRAITHS     = 35284,

    // Add Spells
    SPELL_DETONATION                = 35058,
    SPELL_ARCANE_BOLT               = 20720,
};

static const uint32 aWraithSummonSpells[4] = {SPELL_SUMMON_NETHER_WRAITH_1/*, SPELL_SUMMON_NETHER_WRAITH_2*/, SPELL_SUMMON_NETHER_WRAITH_3, SPELL_SUMMON_NETHER_WRAITH_4};

// boss_pathaleon_the_calculatorAI 逻辑实现
struct boss_pathaleon_the_calculatorAI : public CombatAI
{
    boss_pathaleon_the_calculatorAI(Creature* creature) : CombatAI(creature, 0), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())),
        m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        m_creature->GetCombatManager().SetLeashingDisable(true);
        AddOnKillText(SAY_SLAY_1, SAY_SLAY_2);
    }

    ScriptedInstance* m_instance;
    bool m_isRegularMode;

    // 进入战斗回调
    void Aggro(Unit* /*who*/) override
    {
        DoBroadcastText(SAY_AGGRO, m_creature);
    }

    // 死亡回调
    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        m_creature->RemoveGuardians();

        if (m_instance)
            m_instance->SetData(TYPE_PATHALEON, DONE);
    }

    // 脱战归位回调
    void JustReachedHome() override
    {
        m_creature->RemoveGuardians();

        if (m_instance)
            m_instance->SetData(TYPE_PATHALEON, FAIL);
    }

    // 召唤生物生成登记回调
    void JustSummoned(Creature* summoned) override
    {
        summoned->SetInCombatWithZone();
        summoned->AI()->AttackClosestEnemy();
    }

    // 施法完成回调
    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        switch (spellInfo->Id)
        {
            case SPELL_DOMINATION:
                DoBroadcastText(urand(0, 1) ? SAY_DOMINATION_1 : SAY_DOMINATION_2, m_creature);
                break;
            case SPELL_ENRAGE:
                DoCastSpellIfCan(nullptr, SPELL_SUICIDE, CAST_TRIGGERED);
                DoBroadcastText(SAY_ENRAGE, m_creature);
                break;
            case SPELL_SUMMON_NETHER_WRAITHS:
                DoBroadcastText(SAY_SUMMON, m_creature);
                break;
        }
    }
};

// mob_nether_wraithAI 逻辑实现
struct mob_nether_wraithAI : public ScriptedAI
{
    mob_nether_wraithAI(Creature* creature) : ScriptedAI(creature) {}

    // 死亡回调
    void JustDied(Unit* /*killer*/) override
    {
        m_creature->CastSpell(nullptr, SPELL_DETONATION, TRIGGERED_OLD_TRIGGERED);
    }
};

// 35284 - Summon Nether Wraiths
// SummonNetherWraiths 法术脚本
struct SummonNetherWraiths : public SpellScript
{
    // 法术效果执行回调
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        uint8 maxWraith = 3;
        for (uint8 i = 0; i < maxWraith; ++i)
            spell->GetCaster()->CastSpell(nullptr, aWraithSummonSpells[i], TRIGGERED_OLD_TRIGGERED);
    }
};

// 36022 - Arcane Torrent
// ArcaneTorrentPathaleon 法术脚本
struct ArcaneTorrentPathaleon : public SpellScript
{
    // 触发施法回调
    void OnCast(Spell* spell) const override
    {
        Unit* caster = spell->GetCaster();
        if (SpellAuraHolder* manaTap = caster->GetSpellAuraHolder(SPELL_MANA_TAP))
            if (uint32 stack = manaTap->GetStackAmount())
            {
                // not confirmed to do anything despite the tooltip currently
            }
    }
};

// 注册 boss_pathaleon_the_calculator 脚本
void AddSC_boss_pathaleon_the_calculator()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_pathaleon_the_calculator";
    pNewScript->GetAI = &GetNewAIInstance<boss_pathaleon_the_calculatorAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_nether_wraith";
    pNewScript->GetAI = &GetNewAIInstance<mob_nether_wraithAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<SummonNetherWraiths>("spell_summon_nether_wraiths");
    RegisterSpellScript<ArcaneTorrentPathaleon>("spell_arcane_torrent_pathaleon");
}
