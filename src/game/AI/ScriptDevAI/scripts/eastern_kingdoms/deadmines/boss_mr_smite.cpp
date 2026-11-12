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
SDName: Boss_Mr_Smite
SD%Complete: 100
SDComment:
SDCategory: Deadmines
EndScriptData */

// 重拳先生（Mr. Smite）首领战 AI（三阶段武器切换：单手锤、双持、大剑与践踏昏迷）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "deadmines.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_PHASE_2                     = -1036002,             // 阶段2 台词广播 ID
    SAY_PHASE_3                     = -1036003,             // 阶段3 台词广播 ID

    // EQUIP_ID_SWORD               = 2179,                 // default equipment, not used in code
    EQUIP_ID_AXE                    = 2183,                 // IDaxe 标识 ID
    EQUIP_ID_HAMMER                 = 10756,                // IDhammer 标识 ID

    SPELL_NIBLE_REFLEXES            = 6433,                 // removed after phase 1
    SPELL_SMITE_SLAM                = 6435,                 // only casted in phase 3
    SPELL_SMITE_STOMP               = 6432,                 // 重拳先生stomp 法术 ID
    SPELL_SMITE_HAMMER              = 6436,                 // unclear, not casted in sniff
    SPELL_THRASH                    = 12787,                // only casted in phase 2; unclear, 3391 directly casted in sniff

    PHASE_1                         = 1,                    // 1 阶段 ID
    PHASE_2                         = 2,                    // 2 阶段 ID
    PHASE_3                         = 3,                    // 3 阶段 ID
    PHASE_EQUIP_NULL                = 4,                    // equipnull 阶段 ID
    PHASE_EQUIP_START               = 5,                    // equip开始 阶段 ID
    PHASE_EQUIP_PROCESS             = 6,                    // equipprocess 阶段 ID
    PHASE_EQUIP_END                 = 7,                    // equipend 阶段 ID
};

// 首领战 AI：重拳先生（mr_smite）
struct boss_mr_smiteAI : public ScriptedAI
{
    boss_mr_smiteAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 m_uiPhase;                                       // 战斗阶段标识
    uint32 m_uiEquipTimer;                                  // Equip 技能计时器
    uint32 m_uiSlamTimer;                                   // Slam 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiPhase = PHASE_1;
        m_uiEquipTimer = 0;
        m_uiSlamTimer = 9000;

        DoCastSpellIfCan(m_creature, SPELL_NIBLE_REFLEXES, CAST_TRIGGERED);

        // must assume database has the default equipment set
        SetEquipmentSlots(true);
    }

    void AttackedBy(Unit* pAttacker) override
    {
        if (m_creature->GetVictim())
            return;

        if (m_uiPhase > PHASE_3)
            return;

        AttackStart(pAttacker);
    }

    void AttackStart(Unit* pWho) override
    {
        if (m_uiPhase > PHASE_3)
            return;

        ScriptedAI::AttackStart(pWho);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        DoCastSpellIfCan(m_creature, SPELL_NIBLE_REFLEXES, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    void MovementInform(uint32 uiMotionType, uint32 /*uiPointId*/) override
    {
        if (uiMotionType != POINT_MOTION_TYPE)
            return;

        m_creature->SetSheath(SHEATH_STATE_UNARMED);
        m_creature->SetStandState(UNIT_STAND_STATE_KNEEL);
        SetEquipmentSlots(false, 0, 0);

        m_uiEquipTimer = 3000;
        m_uiPhase = PHASE_EQUIP_PROCESS;
    }

    void PhaseEquipStart()
    {
        ScriptedInstance* pInstance = (ScriptedInstance*)m_creature->GetInstanceData();

        if (!pInstance)
            return;

        GameObject* pChest = pInstance->GetSingleGameObjectFromStorage(GO_SMITE_CHEST);

        if (!pChest)
            return;

        m_uiPhase = PHASE_EQUIP_NULL;

        float fX, fY, fZ;
        pChest->GetContactPoint(m_creature, fX, fY, fZ, CONTACT_DISTANCE);

        m_creature->GetMotionMaster()->Clear();
        m_creature->SetFacingToObject(pChest);
        m_creature->GetMotionMaster()->MovePoint(0, fX, fY, fZ, FORCED_MOVEMENT_RUN);
    }

    void PhaseEquipProcess()
    {
        if (m_creature->GetHealthPercent() < 33.0f)
        {
            // It's Hammer, go Hammer!
            SetEquipmentSlots(false, EQUIP_ID_HAMMER, EQUIP_UNEQUIP);
            DoCastSpellIfCan(m_creature, SPELL_SMITE_HAMMER);
        }
        else
            SetEquipmentSlots(false, EQUIP_ID_AXE, EQUIP_ID_AXE);

        m_creature->SetStandState(UNIT_STAND_STATE_STAND);
        m_uiPhase = PHASE_EQUIP_END;
        m_uiEquipTimer = 1000;
    }

    void PhaseEquipEnd()
    {
        // We don't have GetVictim, so select from threat list
        Unit* pVictim = m_creature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0);

        if (!pVictim)
        {
            EnterEvadeMode();
            return;
        }

        m_creature->SetSheath(SHEATH_STATE_MELEE);

        m_uiPhase = m_creature->GetHealthPercent() < 33.0f ? PHASE_3 : PHASE_2;

        if (m_uiPhase == PHASE_2)
            DoCastSpellIfCan(m_creature, SPELL_THRASH, CAST_TRIGGERED);

        AttackStart(pVictim);
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
        {
            if (m_uiEquipTimer)
            {
                // decrease the cooldown in between equipment change phases
                if (m_uiEquipTimer > uiDiff)
                {
                    m_uiEquipTimer -= uiDiff;
                    return;
                }
                m_uiEquipTimer = 0;
            }

            switch (m_uiPhase)
            {
                case PHASE_EQUIP_START:
                    PhaseEquipStart();
                    break;
                case PHASE_EQUIP_PROCESS:
                    PhaseEquipProcess();
                    break;
                case PHASE_EQUIP_END:
                    PhaseEquipEnd();
                    break;
            }

            return;
        }

        // the normal combat phases
        switch (m_uiPhase)
        {
            case PHASE_1:
            {
                if (m_creature->GetHealthPercent() < 66.0f)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_SMITE_STOMP) == CAST_OK)
                    {
                        DoScriptText(m_creature->GetHealthPercent() < 33.0f ? SAY_PHASE_3 : SAY_PHASE_2, m_creature);
                        m_uiPhase = PHASE_EQUIP_START;
                        m_uiEquipTimer = 2500;

                        // will clear GetVictim (m_attacking) (清除当前受害者目标)
                        m_creature->AttackStop(true);
                        m_creature->RemoveAurasDueToSpell(SPELL_NIBLE_REFLEXES);
                    }
                    return;
                }
                break;
            }
            case PHASE_2:
            {
                if (m_creature->GetHealthPercent() < 33.0f)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_SMITE_STOMP) == CAST_OK)
                    {
                        DoScriptText(SAY_PHASE_3, m_creature);
                        m_uiPhase = PHASE_EQUIP_START;
                        m_uiEquipTimer = 2500;

                        // will clear GetVictim (m_attacking) (清除当前受害者目标)
                        m_creature->AttackStop(true);
                        m_creature->RemoveAurasDueToSpell(SPELL_THRASH);
                    }
                    return;
                }
                break;
            }
            case PHASE_3:
            {
                if (m_uiSlamTimer < uiDiff)
                {
                    if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_SMITE_SLAM) == CAST_OK)
                        m_uiSlamTimer = 11000;
                }
                else
                    m_uiSlamTimer -= uiDiff;

                break;
            }
        }

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_mr_smite
UnitAI* GetAI_boss_mr_smite(Creature* pCreature)
{
    return new boss_mr_smiteAI(pCreature);
}

// 注册并载入脚本模块：boss_mr_smite
void AddSC_boss_mr_smite()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_mr_smite";
    pNewScript->GetAI = &GetAI_boss_mr_smite;
    pNewScript->RegisterSelf();
}
