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
SDName: boss_dalliah
SD%Complete: 100
SDComment:
SDCategory: Tempest Keep, The Arcatraz
EndScriptData */

// 末日预言者达尔莉安（禁魔监狱二号首领）
/* 遭遇战概述：
 * 达尔莉安与天怒预言者索克斯拉提斯相互守望，施放治疗毁灭、回旋投掷与绝望之礼。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "arcatraz.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 相关常量、法术与文本条目枚举
enum
{
    SAY_AGGRO                       = 19978,
    SAY_SOCCOTHRATES_TAUNT_1        = 19975,
    SAY_SOCCOTHRATES_TAUNT_2        = 19974,
    SAY_SOCCOTHRATES_TAUNT_3        = 19976,
    SAY_HEAL_1                      = 19984,
    SAY_HEAL_2                      = 19985,
    SAY_KILL_1                      = 19980,
    SAY_KILL_2                      = 19981,
    SAY_WHIRLWIND_1                 = 19983,
    SAY_WHIRLWIND_2                 = 19982,
    SAY_DEATH                       = 19986,

    SPELL_DOUBLE_ATTACK             = 19818,
    SPELL_GIFT_DOOMSAYER            = 36173,
    SPELL_GIFT_DOOMSAYER_H          = 39009,
    SPELL_HEAL                      = 36144,
    SPELL_HEAL_H                    = 39013,
    SPELL_WHIRLWIND                 = 36142,
    SPELL_SHADOW_WAVE               = 39016,                // heroic spell only
};

// DaliahActions 相关枚举
enum DaliahActions
{
    DALIAH_TAUNT,
    DALIAH_ACTION_MAX,
};

// boss_dalliahAI 逻辑实现
struct boss_dalliahAI : public CombatAI
{
    boss_dalliahAI(Creature* creature) : CombatAI(creature, DALIAH_ACTION_MAX),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddTimerlessCombatAction(DALIAH_TAUNT, true);
        AddOnKillText(SAY_KILL_1, SAY_KILL_2);
    }

    ScriptedInstance* m_instance;
    bool m_isRegularMode;

    // 遭遇战/状态重置回调
    void Reset() override
    {
        CombatAI::Reset();

        DoCastSpellIfCan(nullptr, SPELL_DOUBLE_ATTACK, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    // 进入战斗回调
    void Aggro(Unit* /*who*/) override
    {
        DoBroadcastText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_DALLIAH, IN_PROGRESS);

        m_creature->SetCombatStartPosition(Position(aDalliahStartPos[0], aDalliahStartPos[1], aDalliahStartPos[2]));
    }

    void JustDied(Unit* /*who*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_DALLIAH, DONE);
    }

    void EnterEvadeMode() override
    {
        CombatAI::EnterEvadeMode();

        if (m_instance)
            m_instance->SetData(TYPE_DALLIAH, FAIL);
    }

    void MovementInform(uint32 moveType, uint32 pointId) override
    {
        if (moveType != POINT_MOTION_TYPE)
            return;

        // Adjust orientation
        if (pointId)
            m_creature->SetFacingTo(aDalliahStartPos[3]);
    }

    // 执行战斗行为动作
    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case DALIAH_TAUNT:
                if (m_creature->GetHealthPercent() < 25.0f)
                {
                    // Taunt if Soccothares isn't dead yet
                    if (m_instance && m_instance->GetData(TYPE_SOCCOTHRATES) != DONE)
                    {
                        if (Creature* soccothrates = m_instance->GetSingleCreatureFromStorage(NPC_SOCCOTHRATES))
                        {
                            switch (urand(0, 2))
                            {
                                case 0: DoBroadcastText(SAY_SOCCOTHRATES_TAUNT_1, soccothrates); break;
                                case 1: DoBroadcastText(SAY_SOCCOTHRATES_TAUNT_2, soccothrates); break;
                                case 2: DoBroadcastText(SAY_SOCCOTHRATES_TAUNT_3, soccothrates); break;
                            }
                        }
                    }
                    DisableCombatAction(action);
                }
                break;
        }
    }

    // 施法完成回调
    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        switch (spellInfo->Id)
        {
            case SPELL_WHIRLWIND:
                DoBroadcastText(urand(0, 1) ? SAY_WHIRLWIND_1 : SAY_WHIRLWIND_2, m_creature);
                break;
            case SPELL_HEAL:
            case SPELL_HEAL_H:
                DoBroadcastText(urand(0, 1) ? SAY_HEAL_1 : SAY_HEAL_2, m_creature);
                break;
        }
    }
};

// 36142 - Whirlwind
// WhirlwindDaliah 光环脚本
struct WhirlwindDaliah : public AuraScript
{
    // 光环应用/移除回调
    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply && aura->GetTarget()->AI())
            aura->GetTarget()->AI()->DoCastSpellIfCan(nullptr, aura->GetTarget()->GetMap()->IsRegularDifficulty() ? SPELL_HEAL : SPELL_HEAL_H, CAST_INTERRUPT_PREVIOUS);
    }
};

// 注册 boss_dalliah 脚本
void AddSC_boss_dalliah()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_dalliah";
    pNewScript->GetAI = &GetNewAIInstance<boss_dalliahAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<WhirlwindDaliah>("spell_whirlwind_daliah");
}
