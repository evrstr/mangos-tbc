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
SDName: Boss_High_King_Maulgar
SD%Complete: 80
SDComment:
SDCategory: Gruul's Lair
EndScriptData */

// 莫加尔大王（格鲁尔的巢穴一号首领）与食人魔议会
/* 遭遇战概述：
 * 莫加尔大王是戈隆议会的领袖，携四名强力食人魔随从共同参战：
 * 召魔者奥鲁姆（术士）、疯狂的基格勒（萨满/平衡德）、盲眼先知（牧师）以及克洛什·火怒（火法）；
 * 随从死亡时会激怒莫加尔大王，生命值降至 50% 时进入二阶段狂暴并使用旋风斩、惊骇咆哮与冲锋。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "gruuls_lair.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 台词与法术枚举
enum
{
    SAY_AGGRO                   = 20066,                    // 开战台词
    SAY_ENRAGE                  = 20067,                    // 狂暴台词
    SAY_OGRE_DEATH1             = 20068,                    // 随从阵亡台词 1
    SAY_OGRE_DEATH2             = 20069,                    // 随从阵亡台词 2
    SAY_OGRE_DEATH3             = 20070,                    // 随从阵亡台词 3
    SAY_OGRE_DEATH4             = 20071,                    // 随从阵亡台词 4
    SAY_SLAY1                   = 20072,                    // 击杀玩家台词 1
    SAY_SLAY2                   = 20073,                    // 击杀玩家台词 2
    SAY_SLAY3                   = 20074,                    // 击杀玩家台词 3
    SAY_DEATH                   = 20075,                    // 死亡台词

    // High King Maulgar Spells (莫加尔大王法术)
    SPELL_ARCING_SMASH          = 39144,                    // 弧形顺劈
    SPELL_MIGHTY_BLOW           = 33230,                    // 强力重击
    SPELL_WHIRLWIND             = 33238,                    // 旋风斩
    SPELL_FLURRY                = 33232,                    // 乱击狂暴
    SPELL_CHARGE                = 26561,                    // 冲锋
    SPELL_FEAR                  = 16508,                    // 恐惧术

    // Olm the Summoner Spells (召魔者奥鲁姆法术)
    SPELL_DARK_DECAY            = 33129,                    // 黑暗衰退
    SPELL_DEATH_COIL            = 33130,                    // 死亡缠绕
    SPELL_SUMMON_WILD_FELHUNTER = 33131,                    // 召唤野生地狱犬

    // Kiggler the Crazed Spells (疯狂的基格勒法术)
    SPELL_GREATER_POLYMORPH     = 33173,                    // 强效变形术
    SPELL_LIGHTNING_BOLT        = 36152,                    // 闪电箭
    SPELL_ARCANE_SHOCK          = 33175,                    // 奥术震击
    SPELL_ARCANE_EXPLOSION      = 33237,                    // 奥术冲击（击退并降仇恨）

    // Blindeye the Seer Spells (盲眼先知法术)
    SPELL_GREATER_PW_SHIELD     = 33147,                    // 强效真言术：盾
    SPELL_HEAL                  = 33144,                    // 治疗术
    SPELL_PRAYEROFHEALING       = 33152,                    // 治疗祷言

    // Krosh Firehand Spells (克洛什·火怒法术)
    SPELL_GREATER_FIREBALL      = 33051,                    // 强效火球术
    SPELL_SPELLSHIELD           = 33054,                    // 法术护盾（法师偷取）
    SPELL_BLAST_WAVE            = 33061,                    // 冲击波
};

// 莫加尔大王战斗动作枚举
enum MaulgarActions
{
    MAULGAR_PHASE_2,                                        // 50% 二阶段乱击狂暴
    MAULGAR_ARCING_SMASH,                                   // 弧形顺劈动作
    MAULGAR_MIGHTY_BLOW,                                    // 强力重击动作
    MAULGAR_WHIRLWIND,                                      // 旋风斩动作
    MAULGAR_CHARGE,                                         // 冲锋动作
    MAULGAR_FEAR,                                           // 恐惧动作
    MAULGAR_ACTION_MAX,
};

// High King Maulgar AI (莫加尔大王 AI)
struct boss_high_king_maulgarAI : public CombatAI
{
    boss_high_king_maulgarAI(Creature* creature) : CombatAI(creature, MAULGAR_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddTimerlessCombatAction(MAULGAR_PHASE_2, true);
        AddCombatAction(MAULGAR_ARCING_SMASH, 8000, 14000);
        AddCombatAction(MAULGAR_MIGHTY_BLOW, 15000, 25000);
        AddCombatAction(MAULGAR_WHIRLWIND, 30000u);
        AddCombatAction(MAULGAR_CHARGE, true);
        AddCombatAction(MAULGAR_FEAR, true);
        AddOnKillText(SAY_SLAY1, SAY_SLAY2, SAY_SLAY3);
    }

    ScriptedInstance* m_instance;                           // 副本数据管理指针

    uint32 m_uiCouncilDeathCount;                           // 已阵亡议会成员计数

    void Reset() override                                   // 重置遭遇战
    {
        CombatAI::Reset();
        m_uiCouncilDeathCount = 0;
    }

    void JustReachedHome() override                         // 脱战回位回调：置状态为失败
    {
        if (m_instance)
            m_instance->SetData(TYPE_MAULGAR_EVENT, FAIL);
    }

    void JustDied(Unit* /*killer*/) override                // 首领死亡回调：播放遗言并置状态为已完成
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        // Set data to Done on Death (死亡时更新进度为完成)
        if (m_instance)
            m_instance->SetData(TYPE_MAULGAR_EVENT, DONE);
    }

    void Aggro(Unit* /*who*/) override                      // 进入战斗喊话并置状态为进行中
    {
        DoBroadcastText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_MAULGAR_EVENT, IN_PROGRESS);
    }

    void EventCouncilDeath()                                 // 议会随从阵亡触发喊话
    {
        switch (++m_uiCouncilDeathCount)
        {
            case 1: DoBroadcastText(SAY_OGRE_DEATH1, m_creature); break;
            case 2: DoBroadcastText(SAY_OGRE_DEATH2, m_creature); break;
            case 3: DoBroadcastText(SAY_OGRE_DEATH3, m_creature); break;
            case 4: DoBroadcastText(SAY_OGRE_DEATH4, m_creature); break;
        }
    }

    void ExecuteAction(uint32 action) override              // 执行莫加尔战斗技能动作
    {
        switch (action)
        {
            case MAULGAR_PHASE_2:
                if (m_creature->GetHealthPercent() < 50.0f)
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_FLURRY) == CAST_OK)
                    {
                        DoBroadcastText(SAY_ENRAGE, m_creature);
                        SetActionReadyStatus(action, false);
                        ResetCombatAction(MAULGAR_FEAR, urand(10000, 25000));
                        ResetCombatAction(MAULGAR_CHARGE, 2000);
                    }
                }
                break;
            case MAULGAR_ARCING_SMASH:
                if (DoCastSpellIfCan(nullptr, SPELL_ARCING_SMASH) == CAST_OK)
                    ResetCombatAction(action, urand(8000, 12000));
                break;
            case MAULGAR_MIGHTY_BLOW:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MIGHTY_BLOW) == CAST_OK)
                    ResetCombatAction(action, urand(20000, 35000));
                break;
            case MAULGAR_WHIRLWIND:
                if (DoCastSpellIfCan(nullptr, SPELL_WHIRLWIND) == CAST_OK)
                    ResetCombatAction(action, urand(30000, 40000));
                break;
            case MAULGAR_CHARGE:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1, SPELL_CHARGE, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, SPELL_CHARGE) == CAST_OK)
                        ResetCombatAction(action, urand(14000, 20000));
                break;
            case MAULGAR_FEAR:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_FEAR) == CAST_OK)
                    ResetCombatAction(action, urand(20000, 35000));
                break;
        }
    }
};

// Base AI for every council member (食人魔议会成员基类 AI)
struct Council_Base_AI : public CombatAI
{
    Council_Base_AI(Creature* creature, uint32 actions) : CombatAI(creature, actions), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())) { }

    ScriptedInstance* m_instance;                           // 副本数据管理指针

    void JustDied(Unit* /*pVictim*/) override                // 随从死亡时通知莫加尔大王
    {
        if (!m_instance)
            return;

        Creature* pMaulgar = m_instance->GetSingleCreatureFromStorage(NPC_MAULGAR);
        if (pMaulgar && pMaulgar->IsAlive())
        {
            if (boss_high_king_maulgarAI* pMaulgarAI = dynamic_cast<boss_high_king_maulgarAI*>(pMaulgar->AI()))
                pMaulgarAI->EventCouncilDeath();
        }
    }
};

// 召魔者奥鲁姆行为动作枚举
enum OlmActions
{
    OLM_DARK_DECAY,
    OLM_DEATH_COIL,
    OLM_SUMMON,
    OLM_ACTION_MAX,
};

// Olm The Summoner AI (召魔者奥鲁姆 AI)
struct boss_olm_the_summonerAI : public Council_Base_AI
{
    boss_olm_the_summonerAI(Creature* creature) : Council_Base_AI(creature, OLM_ACTION_MAX)
    {
        AddCombatAction(OLM_DARK_DECAY, 18000u);
        AddCombatAction(OLM_DEATH_COIL, 14000u);
        AddCombatAction(OLM_SUMMON, 10000u);
    }

    void ExecuteAction(uint32 action) override              // 执行术士技能（黑暗衰退/死亡缠绕/召唤地狱犬）
    {
        switch (action)
        {
            case OLM_DARK_DECAY:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_DARK_DECAY) == CAST_OK)
                    ResetCombatAction(action, 5000);
                break;
            case OLM_DEATH_COIL:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_DEATH_COIL) == CAST_OK)
                    ResetCombatAction(action, urand(8000, 13000));
                break;
            case OLM_SUMMON:
                if (DoCastSpellIfCan(nullptr, SPELL_SUMMON_WILD_FELHUNTER) == CAST_OK)
                    ResetCombatAction(action, urand(25000, 35000));
                break;
        }
    }
};

// 疯狂的基格勒行为动作枚举
enum KigglerActions
{
    KIGGLER_ACTION_GREATER_POLYMORPH,
    KIGGLER_ACTION_ARCANE_EXPLOSION,
    KIGGLER_ACTION_ARCANE_SHOCK,
    KIGGLER_ACTION_LIGHTNING_BOLT,
    KIGGLER_ACTION_MAX,
};

// Kiggler The Crazed AI (疯狂的基格勒 AI)
struct boss_kiggler_the_crazedAI : public Council_Base_AI
{
    boss_kiggler_the_crazedAI(Creature* creature) : Council_Base_AI(creature, KIGGLER_ACTION_MAX)
    {
        AddCombatAction(KIGGLER_ACTION_GREATER_POLYMORPH, 5000u);
        AddCombatAction(KIGGLER_ACTION_ARCANE_EXPLOSION, 30000u);
        AddCombatAction(KIGGLER_ACTION_ARCANE_SHOCK, 20000u);
        AddCombatAction(KIGGLER_ACTION_LIGHTNING_BOLT, 0u);
        AddMainSpell(SPELL_LIGHTNING_BOLT);
        SetRangedMode(true, 35.f, TYPE_PROXIMITY);
    }

    void SpellHitTarget(Unit* pVictim, const SpellEntry* pSpell) override // 奥术冲击命中降低目标仇恨
    {
        // Spell currently not supported by core. Knock back effect should lower threat (奥术冲击击退降仇恨)
        // Workaround in script:
        if (pSpell->Id == SPELL_ARCANE_EXPLOSION)
        {
            if (pVictim->GetTypeId() != TYPEID_PLAYER)
                return;

            m_creature->getThreatManager().modifyThreatPercent(pVictim, -75);
        }
    }

    void ExecuteAction(uint32 action) override              // 执行萨满/平衡技能
    {
        switch (action)
        {
            case KIGGLER_ACTION_GREATER_POLYMORPH:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_GREATER_POLYMORPH) == CAST_OK)
                    ResetCombatAction(action, 11000);
                return;
            case KIGGLER_ACTION_ARCANE_EXPLOSION:
                if (m_creature->SelectAttackingTarget(ATTACKING_TARGET_NEAREST_BY, 0, SPELL_ARCANE_EXPLOSION, SELECT_FLAG_PLAYER | SELECT_FLAG_USE_EFFECT_RADIUS))
                    if (DoCastSpellIfCan(nullptr, SPELL_ARCANE_EXPLOSION) == CAST_OK)
                        ResetCombatAction(action, 30000);
                return;
            case KIGGLER_ACTION_ARCANE_SHOCK:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_ARCANE_SHOCK) == CAST_OK)
                    ResetCombatAction(action, urand(15000, 20000));
                return;
            case KIGGLER_ACTION_LIGHTNING_BOLT:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_LIGHTNING_BOLT) == CAST_OK)
                    ResetCombatAction(action, GetCurrentRangedMode() ? urand(2000, 3000) : urand(5000, 8000));
            break;
        }
    }
};

// 盲眼先知行为动作枚举
enum BlindeyeActions
{
    BLINDEYE_GREATER_POWER_WORD_SHIELD,
    BLINDEYE_HEAL,
    BLINDEYE_ACTION_MAX,
};

// Blindeye The Seer AI (盲眼先知 AI)
struct boss_blindeye_the_seerAI : public Council_Base_AI
{
    boss_blindeye_the_seerAI(Creature* creature) : Council_Base_AI(creature, BLINDEYE_ACTION_MAX)
    {
        AddCombatAction(BLINDEYE_GREATER_POWER_WORD_SHIELD, 30000u);
        AddCombatAction(BLINDEYE_HEAL, 25000, 40000);
    }

    void ExecuteAction(uint32 action) override              // 执行牧师技能（盾/治疗术/治疗祷言）
    {
        switch (action)
        {
            case BLINDEYE_GREATER_POWER_WORD_SHIELD:
                if (DoCastSpellIfCan(nullptr, SPELL_GREATER_PW_SHIELD) == CAST_OK)
                {
                    DoCastSpellIfCan(nullptr, SPELL_PRAYEROFHEALING);
                    ResetCombatAction(action, urand(30000, 40000));
                }
                break;
            case BLINDEYE_HEAL:
                if (Unit* target = DoSelectLowestHpFriendly(50.0f))
                    if (DoCastSpellIfCan(target, SPELL_HEAL) == CAST_OK)
                        ResetCombatAction(action, urand(15000, 40000));
                break;
        }
    }
};

// 克洛什·火怒行为动作枚举
enum KroshActions
{
    KROSH_BLAST_WAVE,
    KROSH_SPELL_SHIELD,
    KROSH_GREATER_FIREBALL,
    KROSH_ACTION_MAX,
};

// Krosh Firehand AI (克洛什·火怒 AI)
struct boss_krosh_firehandAI : public Council_Base_AI
{
    boss_krosh_firehandAI(Creature* creature) : Council_Base_AI(creature, KROSH_ACTION_MAX)
    {
        AddCombatAction(KROSH_BLAST_WAVE, 4000u);
        AddCombatAction(KROSH_SPELL_SHIELD, 1000u);
        AddCombatAction(KROSH_GREATER_FIREBALL, 12000u);
        SetRangedMode(true, 30.f, TYPE_FULL_CASTER);
    }

    void ExecuteAction(uint32 action) override              // 执行法师技能（法术护盾/强效火球术/冲击波）
    {
        switch (action)
        {
            case KROSH_BLAST_WAVE:
                if (m_creature->SelectAttackingTarget(ATTACKING_TARGET_NEAREST_BY, 0, nullptr, SELECT_FLAG_PLAYER | SELECT_FLAG_USE_EFFECT_RADIUS))
                    if (DoCastSpellIfCan(nullptr, SPELL_BLAST_WAVE) == CAST_OK)
                        ResetCombatAction(action, 6000);
                break;
            case KROSH_SPELL_SHIELD:
                if (DoCastSpellIfCan(nullptr, SPELL_SPELLSHIELD) == CAST_OK)
                    ResetCombatAction(action, 30000);
                break;
            case KROSH_GREATER_FIREBALL:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_GREATER_FIREBALL) == CAST_OK)
                    ResetCombatAction(action, 3200);
                break;
        }
    }
};

// 注册莫加尔大王与议会随从脚本
void AddSC_boss_high_king_maulgar()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_high_king_maulgar";
    pNewScript->GetAI = &GetNewAIInstance<boss_high_king_maulgarAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_kiggler_the_crazed";
    pNewScript->GetAI = &GetNewAIInstance<boss_kiggler_the_crazedAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_blindeye_the_seer";
    pNewScript->GetAI = &GetNewAIInstance<boss_blindeye_the_seerAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_olm_the_summoner";
    pNewScript->GetAI = &GetNewAIInstance<boss_olm_the_summonerAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_krosh_firehand";
    pNewScript->GetAI = &GetNewAIInstance<boss_krosh_firehandAI>;
    pNewScript->RegisterSelf();
}
