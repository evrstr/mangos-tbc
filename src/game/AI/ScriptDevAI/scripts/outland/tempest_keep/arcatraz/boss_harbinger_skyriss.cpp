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
SDName: Boss_Harbinger_Skyriss
SD%Complete: 95
SDComment: Timers will need adjustments.
SDCategory: Tempest Keep, The Arcatraz
EndScriptData */

// 预言者斯克瑞斯（禁魔监狱最终首领）
/* 遭遇战概述：
 * 预言者斯克瑞斯是被囚禁的无面者首领，战斗中施放心灵控制、心智衰退，并在 66% 和 33% 分裂出完全相同的幻象分身。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "arcatraz.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 相关常量、法术与文本条目枚举
enum
{
    SAY_KILL_1                  = 19813,
    SAY_KILL_2                  = 19814,
    SAY_MIND_1                  = 19816,
    SAY_MIND_2                  = 19817,
    SAY_FEAR_1                  = 19818,
    SAY_FEAR_2                  = 19819,
    SAY_IMAGE                   = 19821,
    SAY_DEATH                   = 19815,

    SPELL_FEAR                  = 39415,
    SPELL_MIND_REND             = 36924,
    SPELL_MIND_REND_H           = 39017,
    SPELL_DOMINATION            = 37162,
    SPELL_DOMINATION_H          = 39019,
    SPELL_MANA_BURN_H           = 39020,
    SPELL_66_ILLUSION           = 36931,                    // Summons 21466
    SPELL_33_ILLUSION           = 36932,                    // Summons 21467

    SPELL_BIRTH                 = 26262,                    // Cast by illusions on spawn in EAI
    SPELL_BLINK_VISUAL          = 36937,
};

// HarbingerSkyrissActions 相关枚举
enum HarbingerSkyrissActions
{
    SKYRISS_66,
    SKYRISS_33,
    SKYRISS_ACTION_MAX,
};

// boss_harbinger_skyrissAI 逻辑实现
struct boss_harbinger_skyrissAI : public CombatAI
{
    boss_harbinger_skyrissAI(Creature* creature) : CombatAI(creature, SKYRISS_ACTION_MAX),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddOnKillText(SAY_KILL_1, SAY_KILL_2);
        AddTimerlessCombatAction(SKYRISS_66, true);
        AddTimerlessCombatAction(SKYRISS_33, true);
    }

    ScriptedInstance* m_instance;
    bool m_isRegularMode;

    // 死亡回调
    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_HARBINGERSKYRISS, DONE);
    }

    // 脱战归位回调
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_HARBINGERSKYRISS, FAIL);
    }

    // 召唤生物生成登记回调
    void JustSummoned(Creature* summoned) override
    {
        switch (summoned->GetCreatedBySpellId())
        {
            case SPELL_66_ILLUSION:
            case SPELL_33_ILLUSION:
                summoned->SetAOEImmune(true);
                break;
        }
        if (m_creature->GetVictim())
            summoned->AI()->AttackStart(m_creature->GetVictim());
    }

    // 执行战斗行为动作
    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case SKYRISS_66:
                if (m_creature->GetHealthPercent() <= 66.f)
                {
                    DoCastSpellIfCan(nullptr, SPELL_66_ILLUSION);
                    DoCastSpellIfCan(nullptr, SPELL_BLINK_VISUAL);
                    DoBroadcastText(SAY_IMAGE, m_creature);
                    DisableCombatAction(action);
                }
                break;
            case SKYRISS_33:
                if (m_creature->GetHealthPercent() <= 33.f)
                {
                    DoCastSpellIfCan(nullptr, SPELL_33_ILLUSION);
                    DoCastSpellIfCan(nullptr, SPELL_BLINK_VISUAL);
                    DoBroadcastText(SAY_IMAGE, m_creature);
                    DisableCombatAction(action);
                }
                break;
        }
    }

    // 施法完成回调
    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        if (spellInfo->Id == SPELL_FEAR)
            DoBroadcastText(urand(0, 1) ? SAY_FEAR_1 : SAY_FEAR_2, m_creature);
        else if (spellInfo->Id == SPELL_DOMINATION || spellInfo->Id == SPELL_DOMINATION_H)
            DoBroadcastText(urand(0, 1) ? SAY_MIND_1 : SAY_MIND_2, m_creature);
    }
};

// 注册 boss_harbinger_skyriss 脚本
void AddSC_boss_harbinger_skyriss()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_harbinger_skyriss";
    pNewScript->GetAI = &GetNewAIInstance<boss_harbinger_skyrissAI>;
    pNewScript->RegisterSelf();
}
