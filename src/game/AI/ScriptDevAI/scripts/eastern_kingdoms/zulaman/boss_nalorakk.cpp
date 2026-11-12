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
SDName: Boss_Nalorakk
SD%Complete: 95
SDComment: Small adjustments may be required
SDCategory: Zul'Aman
EndScriptData */

// 纳洛拉克（Nalorakk）熊之化身首领战 AI（巨魔形态撕裂流血与巨熊形态强力狂暴交替）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "zulaman.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_EVENT1_SACRIFICE    = -1568014,                     // event1sacrifice 台词广播 ID
    SAY_EVENT2_SACRIFICE    = -1568015,                     // event2sacrifice 台词广播 ID

    SAY_AGGRO               = -1568016,                     // 开战 台词广播 ID
    SAY_SURGE               = -1568017,                     // surge 台词广播 ID
    SAY_TOBEAR              = -1568018,                     // tobear 台词广播 ID
    SAY_TOBEAR_EMOTE        = -1568172,                     // tobear表情 台词广播 ID
    SAY_TOTROLL             = -1568019,                     // totroll 台词广播 ID
    SAY_BERSERK             = -1568020,                     // 狂暴 台词广播 ID
    SAY_SLAY1               = 23171,                        // slay1 台词广播 ID
    SAY_SLAY2               = 23172,                        // slay2 台词广播 ID
    SAY_DEATH               = -1568023,                     // 死亡 台词广播 ID

    SPELL_BERSERK           = 45078,                        // unsure, this increases damage, size and speed

    // Defines for Troll form
    SPELL_BRUTAL_SWIPE      = 42384,                        // brutalswipe 法术 ID
    SPELL_MANGLE            = 42389,                        // mangle 法术 ID
    SPELL_SURGE             = 42402,                        // surge 法术 ID
    SPELL_SURGE_MELEE_SUPPRESS = 44960,                     // surgemeleesuppress 法术 ID
    SPELL_BEAR_SHAPE        = 42377,                        // bearshape 法术 ID

    // Defines for Bear form
    SPELL_LACERATING_SLASH  = 42395,                        // lacerating斩击 法术 ID
    SPELL_REND_FLESH        = 42397,                        // 雷德flesh 法术 ID
    SPELL_DEAFENING_ROAR    = 42398,                        // deafeningroar 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum NalorakkActions
{
    NALORAKK_ACTION_BERSERK,
    NALORAKK_ACTION_BEAR_SHAPE,
    NALORAKK_ACTION_SWIPE,
    NALORAKK_ACTION_MANGLE,
    NALORAKK_ACTION_SURGE,
    NALORAKK_ACTION_SLASH,
    NALORAKK_ACTION_REND,
    NALORAKK_ACTION_ROAR,
    NALORAKK_ACTION_MAX,
};

// 首领战 AI：标识 ID（nalorakk）
struct boss_nalorakkAI : public CombatAI
{
    boss_nalorakkAI(Creature* creature) : CombatAI(creature, NALORAKK_ACTION_MAX), m_instance(static_cast<instance_zulaman*>(creature->GetInstanceData())), m_uiCurrentWave(0)
    {
        AddCombatAction(NALORAKK_ACTION_BERSERK, uint32(10 * MINUTE * IN_MILLISECONDS));
        AddCombatAction(NALORAKK_ACTION_BEAR_SHAPE, 45000u);
        AddCombatAction(NALORAKK_ACTION_SWIPE, 12000u);
        AddCombatAction(NALORAKK_ACTION_MANGLE, 15000u);
        AddCombatAction(NALORAKK_ACTION_SURGE, 20000u);
        AddCombatAction(NALORAKK_ACTION_SLASH, true);
        AddCombatAction(NALORAKK_ACTION_REND, true);
        AddCombatAction(NALORAKK_ACTION_ROAR, true);
        m_creature->GetCombatManager().SetLeashingCheck([](Unit*, float /*x*/, float y, float /*z*/)
        {
            return y > 1378.009f;
        });
        AddOnKillText(SAY_SLAY1, SAY_SLAY2);
        Reset();
    }

    instance_zulaman* m_instance;

    uint8 m_uiCurrentWave;
    bool m_bIsInBearForm;                                   // IsInBearForm 状态标记

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();
        m_bIsInBearForm             = false;
    }

    void MoveInLineOfSight(Unit* who) override
    {
        ScriptedAI::MoveInLineOfSight(who);

        if (m_instance && m_instance->IsBearPhaseInProgress())
            return;

        if (who->GetTypeId() == TYPEID_PLAYER && !static_cast<Player*>(who)->IsGameMaster() && m_creature->IsWithinDistInMap(who, aBearEventInfo[m_uiCurrentWave].aggroDist))
        {
            DoScriptText(aBearEventInfo[m_uiCurrentWave].yellId, m_creature);
            if (m_instance)
                m_instance->SendNextBearWave(who);
        }
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_NALORAKK, FAIL);
    }

    // 移动路径点到达回调
    void MovementInform(uint32 motionType, uint32 pointId) override
    {
        if (motionType != POINT_MOTION_TYPE)
            return;

        if (pointId)
        {
            m_creature->GetMotionMaster()->Clear(false, true);
            m_creature->GetMotionMaster()->MoveIdle();
            m_creature->SetFacingTo(aBearEventInfo[m_uiCurrentWave + 1].ori); // First point is spawn (First路径点is生成)

            if (m_uiCurrentWave < MAX_BEAR_WAVES - 1)
            {
                if (m_instance)
                    m_instance->SetBearEventProgress(false);
                ++m_uiCurrentWave;
            }
        }
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_NALORAKK, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (!m_instance)
            return;

        m_instance->SetData(TYPE_NALORAKK, DONE);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case NALORAKK_ACTION_BERSERK:
                if (DoCastSpellIfCan(m_creature, SPELL_BERSERK) == CAST_OK)
                {
                    DoScriptText(SAY_BERSERK, m_creature);
                    DisableCombatAction(action);
                }
                return;
            case NALORAKK_ACTION_BEAR_SHAPE:
                if (m_bIsInBearForm)
                {
                    DoScriptText(SAY_TOTROLL, m_creature);
                    ResetCombatAction(action, 45000);
                    m_bIsInBearForm = false;
                    // Reset troll form timers
                    DisableCombatAction(NALORAKK_ACTION_SLASH);
                    DisableCombatAction(NALORAKK_ACTION_REND);
                    DisableCombatAction(NALORAKK_ACTION_ROAR);
                    ResetCombatAction(NALORAKK_ACTION_SURGE, urand(15000, 32000));
                    ResetCombatAction(NALORAKK_ACTION_SWIPE, urand(7000, 20000));
                    ResetCombatAction(NALORAKK_ACTION_MANGLE, urand(3000, 20000));
                    SetEquipmentSlots(true);
                }
                else
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_BEAR_SHAPE, CAST_INTERRUPT_PREVIOUS) == CAST_OK)
                    {
                        DoScriptText(SAY_TOBEAR_EMOTE, m_creature);
                        DoScriptText(SAY_TOBEAR, m_creature);
                        ResetCombatAction(action, 30000);
                        m_bIsInBearForm = true;
                        // Reset bear form timers
                        DisableCombatAction(NALORAKK_ACTION_SWIPE);
                        DisableCombatAction(NALORAKK_ACTION_MANGLE);
                        DisableCombatAction(NALORAKK_ACTION_SURGE);
                        ResetCombatAction(NALORAKK_ACTION_SLASH, urand(5000, 8000));
                        ResetCombatAction(NALORAKK_ACTION_REND, urand(6000, 10000));
                        ResetCombatAction(NALORAKK_ACTION_ROAR, urand(15000, 25000));
                        SetEquipmentSlots(false, 0, 0, 0);
                        m_creature->resetAttackTimer(BASE_ATTACK);
                    }
                }
                return;
            case NALORAKK_ACTION_SWIPE:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_BRUTAL_SWIPE) == CAST_OK)
                    ResetCombatAction(action, urand(7000, 15000));
                return;
            case NALORAKK_ACTION_MANGLE:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MANGLE) == CAST_OK)
                    ResetCombatAction(action, urand(3000, 15000));
                return;
            case NALORAKK_ACTION_SURGE:
            {
                // select a random unit other than the main tank
                Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER | SELECT_FLAG_NOT_IN_MELEE_RANGE);

                // if there aren't other units, cast on random
                if (!target)
                    target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER);

                if (DoCastSpellIfCan(target, SPELL_SURGE) == CAST_OK)
                {
                    DoScriptText(SAY_SURGE, m_creature);
                    ResetCombatAction(action, urand(15000, 32500));
                }
                return;
            }
            case NALORAKK_ACTION_SLASH:
            {
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_LACERATING_SLASH) == CAST_OK)
                    ResetCombatAction(action, 20000);
                return;
            }
            case NALORAKK_ACTION_REND:
            {
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_REND_FLESH) == CAST_OK)
                    ResetCombatAction(action, urand(6000, 10000));
                return;
            }
            case NALORAKK_ACTION_ROAR:
            {
                if (DoCastSpellIfCan(m_creature, SPELL_DEAFENING_ROAR) == CAST_OK)
                    ResetCombatAction(action, urand(15000, 20000));
                return;
            }
        }
    }
};

// 法术脚本逻辑实现：标识 ID（SurgeNalorakk）
struct SurgeNalorakk : public SpellScript
{
    void OnCast(Spell* spell) const override
    {
        spell->GetCaster()->CastSpell(nullptr, SPELL_SURGE_MELEE_SUPPRESS, TRIGGERED_OLD_TRIGGERED);
    }
};

// 注册并载入脚本模块：boss_nalorakk
void AddSC_boss_nalorakk()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_nalorakk";
    pNewScript->GetAI = &GetNewAIInstance<boss_nalorakkAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<SurgeNalorakk>("spell_surge_nalorakk");
}
