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
SDName: Boss_Nethermancer_Sepethrea
SD%Complete: 95
SDComment: May need some small adjustments
SDCategory: Tempest Keep, The Mechanar
EndScriptData */

// 虚空术士塞菲斯蕾亚（能源舰二号首领）
/* 遭遇战概述：
 * 虚空术士塞菲斯蕾亚召唤两只不可选中的怒火炎魔缓慢追击玩家，自身施放龙息术与冰霜霜冻。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "mechanar.h"
#include "Entities/TemporarySpawn.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 相关常量、法术与文本条目枚举
enum
{
    SAY_AGGRO                       = 19959,
    SAY_SUMMON                      = 19965,
    SAY_DRAGONS_BREATH_1            = 19963,
    SAY_DRAGONS_BREATH_2            = 19964,
    SAY_SLAY1                       = 19961,
    SAY_SLAY2                       = 19962,
    SAY_DEATH                       = 19966,

    SPELL_SUMMON_RAGING_FLAMES      = 35275,
    SPELL_SUMMON_RAGING_FLAMES_H    = 39084,
    SPELL_FROST_ATTACK              = 45196, // serverside - triggers 45195
    SPELL_ARCANE_BLAST              = 35314,
    SPELL_DRAGONS_BREATH            = 35250,

    NPC_RAGING_FLAMES               = 20481,
};

// boss_nethermancer_sepethreaAI 逻辑实现
struct boss_nethermancer_sepethreaAI : public CombatAI
{
    boss_nethermancer_sepethreaAI(Creature* creature) : CombatAI(creature, 0), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())),
        m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        m_creature->GetCombatManager().SetLeashingCheck([&](Unit* /*unit*/, float x, float /*y*/, float /*z*/)->bool
        {
            return x < 266.0f;
        });
        AddOnKillText(SAY_SLAY1, SAY_SLAY2);
    }

    ScriptedInstance* m_instance;
    bool m_isRegularMode;

    // 进入战斗回调
    void Aggro(Unit* /*who*/) override
    {
        m_creature->SetInCombatWithZone();
        DoBroadcastText(SAY_AGGRO, m_creature);
        DoCastSpellIfCan(nullptr, SPELL_FROST_ATTACK, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        DoCastSpellIfCan(nullptr, m_isRegularMode ? SPELL_SUMMON_RAGING_FLAMES : SPELL_SUMMON_RAGING_FLAMES_H);

        if (m_instance)
            m_instance->SetData(TYPE_SEPETHREA, IN_PROGRESS);
    }

    // 死亡回调
    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_SEPETHREA, DONE);
    }

    // 脱战归位回调
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_SEPETHREA, FAIL);
    }

    // 施法完成回调
    void OnSpellCast(SpellEntry const* spellInfo, Unit* target) override
    {
        switch (spellInfo->Id)
        {
            case SPELL_ARCANE_BLAST:
                m_creature->getThreatManager().modifyThreatPercent(target, -50.0f);
                break;
            case SPELL_DRAGONS_BREATH:
                if (urand(0, 1))
                    DoBroadcastText(urand(0, 1) ? SAY_DRAGONS_BREATH_1 : SAY_DRAGONS_BREATH_2, m_creature);
                break;
        }
    }
};

// 相关常量、法术与文本条目枚举
enum
{
    SPELL_INVIS_AND_STEALTH_DETECT = 18950,
    SPELL_RAGING_FLAMES = 35281,
    SPELL_INFERNO       = 35268,
    SPELL_INFERNO_H     = 39346,
};

// npc_raging_flamesAI 逻辑实现
struct npc_raging_flamesAI : public CombatAI
{
    npc_raging_flamesAI(Creature* creature) : CombatAI(creature, 0), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())),
        m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        if (m_creature->IsTemporarySummon())
            m_summonerGuid = m_creature->GetSpawnerGuid();
    }

    ScriptedInstance* m_instance;
    bool m_isRegularMode;

    ObjectGuid m_summonerGuid;

    // 遭遇战/状态重置回调
    void Reset() override
    {
        CombatAI::Reset();
        DoCastSpellIfCan(nullptr, SPELL_RAGING_FLAMES, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        DoCastSpellIfCan(nullptr, SPELL_INVIS_AND_STEALTH_DETECT, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    // 进入战斗回调
    void Aggro(Unit* /*who*/) override
    {
        FixateRandomTarget();
    }

    void FixateRandomTarget()
    {
        DoResetThreat();

        if (Creature* summoner = m_creature->GetMap()->GetCreature(m_summonerGuid))
            if (Unit* newTarget = summoner->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                m_creature->AddThreat(newTarget, 1000000.0f);
    }
};

// 35268,39346 - Inferno
// RagingFlamesInferno 光环脚本
struct RagingFlamesInferno : public AuraScript
{
    // 光环应用/移除回调
    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply)
            if (Unit* caster = aura->GetCaster())
                if (Unit* newTarget = caster->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    caster->AddThreat(newTarget, 1000000.0f);
    }
};

// 注册 boss_nethermancer_sepethrea 脚本
void AddSC_boss_nethermancer_sepethrea()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_nethermancer_sepethrea";
    pNewScript->GetAI = &GetNewAIInstance<boss_nethermancer_sepethreaAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_raging_flames";
    pNewScript->GetAI = &GetNewAIInstance<npc_raging_flamesAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<RagingFlamesInferno>("spell_raging_flames_inferno");
}
