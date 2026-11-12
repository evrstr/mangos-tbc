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
SDName: Boss_Thekal
SD%Complete: 90
SDComment: Needs a good shake-up.
SDCategory: Zul'Gurub
EndScriptData */

// 高阶祭司塞卡尔（High Priest Thekal）狂暴猎豹祭司首领战 AI（两位追随者必须同时死亡否则复活、合体狂暴虎神）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "zulgurub.h"
#include "AI//ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO               = -1309009,                     // 开战 台词广播 ID
    SAY_DEATH               = -1309010,                     // 死亡 台词广播 ID

    SAY_DIES_EMOTE          = 10453,                        // dies表情 台词广播 ID

    SPELL_PACIFY_SELF       = 19951,                        // pacifyself 法术 ID

    SPELL_MORTAL_CLEAVE     = 22859,                        // mortal顺劈斩 法术 ID
    SPELL_SILENCE           = 22666,                        // 沉默 法术 ID
    SPELL_FRENZY            = 23128,                        // 狂乱 法术 ID
    SPELL_FORCE_PUNCH       = 24189,                        // forcepunch 法术 ID
    SPELL_CHARGE            = 24408,                        // 冲锋 法术 ID
    SPELL_ENRAGE            = 8269,                         // 狂暴 法术 ID
    SPELL_SUMMON_TIGERS     = 24183,                        // 召唤tigers 法术 ID
    SPELL_TIGER_FORM        = 24169,                        // tigerform 法术 ID
    SPELL_RESURRECT         = 24173,                        // resurrect 法术 ID
    SPELL_THEKAL_TRIGGER    = 24172,        // TODO: Need to find use for this
    SPELL_BLOODLUST         = 24185,                        // 嗜血 法术 ID
    SPELL_RESSURECTION_IMPACT_VISUAL = 24171,               // ressurectionimpact视觉特效 法术 ID

    // Zealot Lor'Khan Spells
    SPELL_LIGHTNING_SHIELD  = 20545,                        // 闪电护盾 法术 ID
    SPELL_DISPEL_MAGIC      = 17201,                        // dispelmagic 法术 ID
    SPELL_GREAT_HEAL        = 24208,                        // great治疗 法术 ID
    SPELL_DISARM            = 6713,                         // 缴械 法术 ID

    // Zealot Zath Spells
    SPELL_SWEEPING_STRIKES  = 18765,                        // sweepingstrikes 法术 ID
    SPELL_SINISTER_STRIKE   = 15581,                        // sinister打击 法术 ID
    SPELL_GOUGE             = 12540,                        // gouge 法术 ID
    SPELL_KICK              = 15614,                        // kick 法术 ID
    SPELL_BLIND             = 21060,                        // blind 法术 ID

    PHASE_NORMAL            = 1,                            // normal 阶段 ID
    PHASE_FAKE_DEATH        = 2,                            // fake死亡 阶段 ID
    PHASE_WAITING           = 3,                            // waiting 阶段 ID
    PHASE_TIGER             = 4,                            // tiger 阶段 ID

    ACTION_RESSURECTION     = 10,                           // ressurection 动作指令 ID

    SPELL_LIST_PHASE_1 = 1450901,                           // list阶段1 法术 ID
    SPELL_LIST_PHASE_2 = 1450902,                           // list阶段2 法术 ID
};

// abstract base class for faking death
struct boss_thekalBaseAI : public CombatAI
{
    boss_thekalBaseAI(Creature* creature, uint32 actions) : CombatAI(creature, actions),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        m_uiPhase = PHASE_NORMAL;
        SetDeathPrevention(true);
    }

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();

        SetCombatMovement(true);
        SetDeathPrevention(true);
        SetCombatScriptStatus(false);
    }

    ScriptedInstance* m_instance;

    uint8 m_uiPhase;

    virtual void OnFakeingDeath() {}
    virtual bool OnRevive() { return false; }

    void JustPreventedDeath(Unit* /*attacker*/) override
    {
        m_creature->InterruptNonMeleeSpells(true);
        m_creature->StopMoving();
        m_creature->SetTarget(nullptr);
        m_creature->ClearComboPointHolders();
        m_creature->RemoveAllAurasOnDeath();
        m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);
        m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, false);
        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
        m_creature->ClearAllReactives();
        m_creature->GetMotionMaster()->Clear();
        m_creature->GetMotionMaster()->MoveIdle();
        m_creature->SetStandState(UNIT_STAND_STATE_DEAD);
        m_creature->CastSpell(nullptr, SPELL_PACIFY_SELF, TRIGGERED_OLD_TRIGGERED);

        SetCombatMovement(false);
        SetCombatScriptStatus(true);

        m_uiPhase = PHASE_FAKE_DEATH;

        OnFakeingDeath();
    }

    void Revive(bool bOnlyFlags = false)
    {
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
        m_creature->SetStandState(UNIT_STAND_STATE_STAND);

        m_creature->RemoveAurasDueToSpell(SPELL_PACIFY_SELF);

        if (bOnlyFlags)
            return;

        m_creature->SetHealth(m_creature->GetMaxHealth());
        m_uiPhase = PHASE_NORMAL;

        DoResetThreat();
        Reset();

        // Assume Attack (Assume攻击)
        if (!OnRevive())
        {
            SetCombatMovement(true, true);

            SetDeathPrevention(true);

            SetCombatScriptStatus(false);
        }
    }

    virtual void PreventRevive()
    {
        if (m_creature->IsNonMeleeSpellCasted(true))
            m_creature->InterruptNonMeleeSpells(true);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum ThekalActions
{
    THEKAL_TIGER_ENRAGE,
    THEKAL_ACTION_MAX,
    THEKAL_RESS_PHASE_2_DELAY,
};

// 首领战 AI：标识 ID（thekal）
struct boss_thekalAI : public boss_thekalBaseAI
{
    boss_thekalAI(Creature* creature) : boss_thekalBaseAI(creature, THEKAL_ACTION_MAX)
    {
        AddTimerlessCombatAction(THEKAL_TIGER_ENRAGE, false);
        AddCustomAction(ACTION_RESSURECTION, true, [&]()
        {
            // resurrect him in any case
            DoCastSpellIfCan(nullptr, SPELL_RESURRECT);

            m_uiPhase = PHASE_WAITING;
            if (m_instance)
            {
                CanPreventAddsResurrect();
                m_instance->SetData(TYPE_THEKAL, IN_PROGRESS);
            }
        });
        AddCustomAction(THEKAL_RESS_PHASE_2_DELAY, true, [&]()
        {
            DoCastSpellIfCan(nullptr, SPELL_TIGER_FORM);
            m_uiPhase = PHASE_TIGER;
            SetDeathPrevention(false);
            SetCombatScriptStatus(false);
            SetMeleeEnabled(true);
            SetCombatMovement(true, true);
            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
            m_creature->SetSpellList(SPELL_LIST_PHASE_2);
            SetActionReadyStatus(THEKAL_TIGER_ENRAGE, true);
        });
    }

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        boss_thekalBaseAI::Reset();

        m_uiPhase               = PHASE_NORMAL;

        // remove fake death
        Revive(true);

        SetMeleeEnabled(true);

        m_creature->SetSpellList(SPELL_LIST_PHASE_1);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (!m_instance)
            return;

        m_instance->SetData(TYPE_THEKAL, DONE);

        // remove the two adds
        if (Creature* zath = m_instance->GetSingleCreatureFromStorage(NPC_ZATH))
        {
            DoBroadcastText(SAY_DIES_EMOTE, zath);
            zath->ForcedDespawn(1000);
        }
        if (Creature* lorkhan = m_instance->GetSingleCreatureFromStorage(NPC_LORKHAN))
        {
            DoBroadcastText(SAY_DIES_EMOTE, lorkhan);
            lorkhan->ForcedDespawn(1000);
        }
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_THEKAL, FAIL);
    }

    // Only call in context where m_instance is valid
    bool CanPreventAddsResurrect() const
    {
        // If any add is alive, return false
        if (m_instance->GetData(TYPE_ZATH) != SPECIAL || m_instance->GetData(TYPE_LORKHAN) != SPECIAL)
            return false;

        // Else Prevent them Resurrecting
        if (Creature* lorkhan = m_instance->GetSingleCreatureFromStorage(NPC_LORKHAN))
        {
            if (boss_thekalBaseAI* pFakerAI = dynamic_cast<boss_thekalBaseAI*>(lorkhan->AI()))
                pFakerAI->PreventRevive();
        }
        if (Creature* zath = m_instance->GetSingleCreatureFromStorage(NPC_ZATH))
        {
            if (boss_thekalBaseAI* pFakerAI = dynamic_cast<boss_thekalBaseAI*>(zath->AI()))
                pFakerAI->PreventRevive();
        }

        return true;
    }

    void OnFakeingDeath() override
    {
        ResetTimer(ACTION_RESSURECTION, 10000);

        if (m_instance)
        {
            m_instance->SetData(TYPE_THEKAL, SPECIAL);

            // If both Adds are already dead, don't wait 10 seconds
            if (CanPreventAddsResurrect())
                ResetTimer(ACTION_RESSURECTION, 1000);
        }
    }

    bool OnRevive() override
    {
        if (!m_instance)
            return false;

        // Both Adds are 'dead' enter tiger phase
        if (CanPreventAddsResurrect())
        {
            m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
            SetMeleeEnabled(false);
            DoCastSpellIfCan(nullptr, SPELL_RESSURECTION_IMPACT_VISUAL);
            m_creature->SetTarget(nullptr);
            ResetTimer(THEKAL_RESS_PHASE_2_DELAY, 5000);
            return true;
        }
        return false;
    }

    // 进入脱战闪避模式
    void EnterEvadeMode() override
    {
        CombatAI::EnterEvadeMode();
        if (Creature* lorkhan = m_instance->GetSingleCreatureFromStorage(NPC_LORKHAN))
            lorkhan->AI()->EnterEvadeMode();
        if (Creature* zath = m_instance->GetSingleCreatureFromStorage(NPC_ZATH))
            zath->AI()->EnterEvadeMode();
    }

    void ExecuteAction(uint32 action) override
    {
        if (action == THEKAL_TIGER_ENRAGE)
        {
            if (m_creature->GetHealthPercent() < 11.0f)
            {
                if (DoCastSpellIfCan(nullptr, SPELL_ENRAGE) == CAST_OK)
                    DisableCombatAction(action);
            }
        }
    }
};

/*######
## mob_zealot_lorkhan
######*/

// 随从/NPC AI：lorkhan（zealot_lorkhan）
struct mob_zealot_lorkhanAI : public boss_thekalBaseAI
{
    mob_zealot_lorkhanAI(Creature* creature) : boss_thekalBaseAI(creature, 0)
    {
        AddCustomAction(ACTION_RESSURECTION, true, [&]()
        {
            if (m_instance->GetData(TYPE_THEKAL) != SPECIAL || m_instance->GetData(TYPE_ZATH) != SPECIAL)
            {
                DoCastSpellIfCan(nullptr, SPELL_RESURRECT);
                m_instance->SetData(TYPE_LORKHAN, IN_PROGRESS);
            }
        });
    }

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        boss_thekalBaseAI::Reset();

        m_uiPhase = PHASE_NORMAL;

        if (m_instance)
            m_instance->SetData(TYPE_LORKHAN, NOT_STARTED);

        Revive(true);
    }

    void Aggro(Unit* /*who*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_LORKHAN, IN_PROGRESS);
    }

    void OnFakeingDeath() override
    {
        ResetTimer(ACTION_RESSURECTION, 10000);

        if (m_instance)
            m_instance->SetData(TYPE_LORKHAN, SPECIAL);
    }

    void PreventRevive() override
    {
        boss_thekalBaseAI::PreventRevive();
        DisableTimer(ACTION_RESSURECTION);
    }
};

/*######
## npc_zealot_zath
######*/

// 随从/NPC AI：zath（zealot_zath）
struct mob_zealot_zathAI : public boss_thekalBaseAI
{
    mob_zealot_zathAI(Creature* creature) : boss_thekalBaseAI(creature, 0)
    {
        AddCustomAction(ACTION_RESSURECTION, true, [&]()
        {
            if (m_instance->GetData(TYPE_THEKAL) != SPECIAL || m_instance->GetData(TYPE_LORKHAN) != SPECIAL)
            {
                DoCastSpellIfCan(nullptr, SPELL_RESURRECT);
                m_instance->SetData(TYPE_ZATH, IN_PROGRESS);
            }
        });
    }

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        boss_thekalBaseAI::Reset();

        if (m_instance)
            m_instance->SetData(TYPE_ZATH, NOT_STARTED);

        Revive(true);
    }

    void Aggro(Unit* /*who*/) override
    {
        if (m_instance)
            m_instance->SetData(TYPE_ZATH, IN_PROGRESS);
    }

    void OnFakeingDeath() override
    {
        ResetTimer(ACTION_RESSURECTION, 10000);

        if (m_instance)
            m_instance->SetData(TYPE_ZATH, SPECIAL);
    }

    void PreventRevive() override
    {
        boss_thekalBaseAI::PreventRevive();
        DisableTimer(ACTION_RESSURECTION);
    }
};

// 法术脚本逻辑实现：标识 ID（ThekalResurrect）
struct ThekalResurrect : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx == EFFECT_INDEX_0)
            if (boss_thekalBaseAI* fakerAI = dynamic_cast<boss_thekalBaseAI*>(spell->GetCaster()->AI()))
                fakerAI->Revive();
    }
};

// 注册并载入脚本模块：boss_thekal
void AddSC_boss_thekal()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_thekal";
    pNewScript->GetAI = &GetNewAIInstance<boss_thekalAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_zealot_lorkhan";
    pNewScript->GetAI = &GetNewAIInstance<mob_zealot_lorkhanAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_zealot_zath";
    pNewScript->GetAI = &GetNewAIInstance<mob_zealot_zathAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<ThekalResurrect>("spell_thekal_resurrect");
}
