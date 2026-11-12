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
SDName: Boss_Pyroguard_Emberseer
SD%Complete: 90
SDComment: Dummy spells used during the transformation may need further research
SDCategory: Blackrock Spire
EndScriptData */

// 烈焰卫士艾博希尔（Pyroguard Emberseer）首领战 AI

#include "AI/ScriptDevAI/include/sc_common.h"
#include "blackrock_spire.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    // Intro emote/say (开场表情与台词)
    EMOTE_NEAR              = -1229001,                     // near 表情动作 ID
    EMOTE_FULL              = -1229002,                     // full 表情动作 ID
    SAY_FREE                = -1229003,                     // 释放 台词广播 ID

    MAX_GROWING_STACKS      = 20,                           // growingstacks 标识 ID

    // Intro spells (开场仪式法术)
    SPELL_ENCAGE_EMBERSEER  = 15281,                        // cast by Blackhand Incarcerator (黑手监禁者施放)

    SPELL_FIRE_SHIELD       = 13377,                        // 火焰护盾 法术 ID
    SPELL_DESPAWN_EMBERSEER = 16078,                        // not sure what's the purpose of this (用途待进一步确认)
    SPELL_FREEZE_ANIM       = 16245,                        // not sure what's the purpose of this (用途待进一步确认)
    SPELL_FULL_STRENGHT     = 16047,                        // fullstrenght 法术 ID
    SPELL_GROWING           = 16049,                        // stacking aura (叠加层数光环)
    SPELL_BONUS_DAMAGE      = 16534,                        // triggered on full grow (体型完全成长时触发)
    SPELL_TRANSFORM         = 16052,                        // 变形 法术 ID

    // Combat spells (战斗技能法术)
    SPELL_FIRENOVA          = 23462,                        // firenova 法术 ID
    SPELL_FLAMEBUFFET       = 23341,                        // flamebuffet 法术 ID
    SPELL_PYROBLAST         = 20228                         // guesswork, but best fitting in spells-area, was 17274 (has mana cost) (推测参数，最匹配的无蓝耗法术)
};

// 首领战 AI：艾博希尔（pyroguard_emberseer）
struct boss_pyroguard_emberseerAI : public ScriptedAI
{
    boss_pyroguard_emberseerAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (instance_blackrock_spire*) pCreature->GetInstanceData();
        Reset();
    }

    instance_blackrock_spire* m_pInstance;

    uint32 m_uiEncageTimer;                                 // Encage 技能计时器
    uint32 m_uiFireNovaTimer;                               // FireNova 技能计时器
    uint32 m_uiFlameBuffetTimer;                            // FlameBuffet 技能计时器
    uint32 m_uiPyroBlastTimer;                              // PyroBlast 技能计时器
    uint8 m_uiGrowingStacks;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiEncageTimer         = 10000;
        m_uiFireNovaTimer       = 6000;
        m_uiFlameBuffetTimer    = 3000;
        m_uiPyroBlastTimer      = 14000;
        m_uiGrowingStacks       = 0;

        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
        m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PLAYER);

        DoCastSpellIfCan(m_creature, SPELL_FIRE_SHIELD, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_EMBERSEER, DONE);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_pInstance)
            m_pInstance->SetData(TYPE_EMBERSEER, FAIL);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override
    {
        if (eventType == AI_EVENT_CUSTOM_A)
            DoHandleEmberseerGrowing();
    }

    // Wrapper to handle the transformation
    void DoHandleEmberseerGrowing()
    {
        ++m_uiGrowingStacks;

        if (m_uiGrowingStacks == MAX_GROWING_STACKS / 2)
            DoScriptText(EMOTE_NEAR, m_creature);
        else if (m_uiGrowingStacks == MAX_GROWING_STACKS)
        {
            DoScriptText(EMOTE_FULL, m_creature);
            DoScriptText(SAY_FREE, m_creature);

            // Note: the spell order needs further research
            DoCastSpellIfCan(m_creature, SPELL_FULL_STRENGHT, CAST_TRIGGERED);
            DoCastSpellIfCan(m_creature, SPELL_BONUS_DAMAGE, CAST_TRIGGERED);
            DoCastSpellIfCan(m_creature, SPELL_TRANSFORM, CAST_TRIGGERED);

            // activate all runes (激活all符文)
            if (m_pInstance)
            {
                m_pInstance->DoUseEmberseerRunes();
                // Redundant check: if for some reason the event isn't set in progress until this point - avoid using the altar again when the boss is fully grown
                m_pInstance->SetData(TYPE_EMBERSEER, IN_PROGRESS);
            }

            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PLAYER);
        }
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        // Cast Encage spell on OOC timer
        if (m_uiEncageTimer)
        {
            if (m_uiEncageTimer <= uiDiff)
            {
                if (!m_pInstance)
                {
                    script_error_log("Instance Blackrock Spire: ERROR Failed to load instance data for this instace.");
                    return;
                }

                GuidList m_lIncarceratorsGuid;
                m_pInstance->GetIncarceratorGUIDList(m_lIncarceratorsGuid);

                for (GuidList::const_iterator itr = m_lIncarceratorsGuid.begin(); itr != m_lIncarceratorsGuid.end(); ++itr)
                {
                    if (Creature* pIncarcerator = m_creature->GetMap()->GetCreature(*itr))
                        pIncarcerator->CastSpell(m_creature, SPELL_ENCAGE_EMBERSEER, TRIGGERED_NONE);
                }

                m_uiEncageTimer = 0;
            }
            else
                m_uiEncageTimer -= uiDiff;
        }

        // Return since we have no target (无有效目标，退出更新)
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // FireNova Timer (FireNova计时器)
        if (m_uiFireNovaTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FIRENOVA) == CAST_OK)
                m_uiFireNovaTimer = 6000;
        }
        else
            m_uiFireNovaTimer -= uiDiff;

        // FlameBuffet Timer (FlameBuffet计时器)
        if (m_uiFlameBuffetTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_FLAMEBUFFET) == CAST_OK)
                m_uiFlameBuffetTimer = 14000;
        }
        else
            m_uiFlameBuffetTimer -= uiDiff;

        // PyroBlast Timer (炎爆术计时器)
        if (m_uiPyroBlastTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_PYROBLAST) == CAST_OK)
                    m_uiPyroBlastTimer = 15000;
            }
        }
        else
            m_uiPyroBlastTimer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：boss_pyroguard_emberseer
UnitAI* GetAI_boss_pyroguard_emberseer(Creature* pCreature)
{
    return new boss_pyroguard_emberseerAI(pCreature);
}

// 注册并载入脚本模块：boss_pyroguard_emberseer
void AddSC_boss_pyroguard_emberseer()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_pyroguard_emberseer";
    pNewScript->GetAI = &GetAI_boss_pyroguard_emberseer;
    pNewScript->RegisterSelf();
}
