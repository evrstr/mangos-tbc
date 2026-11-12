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
SDName: Boss_Talon_King_Ikiss
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Sethekk Halls
EndScriptData */

// 利爪之王艾吉斯（塞泰克大厅最终首领）
/* 遭遇战概述：
 * 利爪之王艾吉斯是塞泰克大厅的最终首领，战斗中会周期性使用奥术箭雨、变形术与减速术（英雄模式）；
 * 在生命值降至 80%、50%、25% 时会闪现至中央引导致命的奥术暴爆（玩家需躲入石柱后卡视角避难）；
 * 生命值低于 15% 时开启法力护盾吸收伤害。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "sethekk_halls.h"

// 台词、表情与法术枚举
enum
{
    SAY_INTRO                   = 17769,                    // 进视线开场台词
    SAY_AGGRO_1                 = 17765,                    // 开战台词 1
    SAY_AGGRO_2                 = 17767,                    // 开战台词 2
    SAY_AGGRO_3                 = 17768,                    // 开战台词 3
    SAY_SLAY_1                  = 17763,                    // 击杀玩家台词 1
    SAY_SLAY_2                  = 17764,                    // 击杀玩家台词 2
    SAY_DEATH                   = 17762,                    // 死亡台词
    EMOTE_ARCANE_EXP            = 19738,                    // 奥术暴爆表情提示

    SPELL_BLINK                 = 38194,                    // 闪现术
    SPELL_MANA_SHIELD           = 38151,                    // 法力护盾
    SPELL_ARCANE_BUBBLE         = 9438,                     // 奥术气泡视觉
    SPELL_SLOW_H                = 35032,                    // 减速术（英雄难度）

    SPELL_POLYMORPH             = 38245,                    // 变形术（普通难度）
    SPELL_POLYMORPH_H           = 43309,                    // 变形术（英雄难度）

    SPELL_ARCANE_VOLLEY         = 35059,                    // 奥术箭雨（普通难度）
    SPELL_ARCANE_VOLLEY_H       = 40424,                    // 奥术箭雨（英雄难度）

    SPELL_ARCANE_EXPLOSION      = 38197,                    // 奥术暴爆（普通难度）
    SPELL_ARCANE_EXPLOSION_H    = 40425,                    // 奥术暴爆（英雄难度）
};

// 艾吉斯战斗行为枚举（按优先级排序）
enum TalonKingIkissActions // order based on priority
{
    TALON_KING_IKISS_MANA_SHIELD,                           // 15% 血量法力护盾动作
    TALON_KING_IKISS_BLINK_START,                           // 闪现术触发阶段动作
    TALON_KING_IKISS_ACTION_SLOW,                           // 减速术动作
    TALON_KING_IKISS_ACTION_POLYMORPH,                      // 变形术动作
    TALON_KING_IKISS_ACTION_ARCANE_VOLLEY,                  // 奥术箭雨动作
    TALON_KING_IKISS_ACTION_MAX,
    TALON_KING_IKISS_ARCANE_EXPLOSION_EMOTE,                // 奥术暴爆表情计时动作
    TALON_KING_IKISS_ARCANE_EXPLOSION_CAST,                 // 奥术暴爆施法计时动作
    TALON_KING_IKISS_ARCANE_EXPLOSION_ATTACK_START,         // 奥术暴爆恢复攻击计时动作
};

// 利爪之王艾吉斯 AI
struct boss_talon_king_ikissAI : public CombatAI
{
    boss_talon_king_ikissAI(Creature* creature) : CombatAI(creature, TALON_KING_IKISS_ACTION_MAX),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_isRegularMode(creature->GetMap()->IsRegularDifficulty()),
            m_Intro(false), m_uiBlinkPhase(0), m_HealthPercent(80.f)
    {
        AddTimerlessCombatAction(TALON_KING_IKISS_MANA_SHIELD, true);
        AddTimerlessCombatAction(TALON_KING_IKISS_BLINK_START, true);
        AddCombatAction(TALON_KING_IKISS_ACTION_SLOW, 9000, 13000);
        AddCombatAction(TALON_KING_IKISS_ACTION_POLYMORPH, 6000, 10000);
        AddCombatAction(TALON_KING_IKISS_ACTION_ARCANE_VOLLEY, 5000, 12000);
        AddCustomAction(TALON_KING_IKISS_ARCANE_EXPLOSION_EMOTE, true, [&]() { HandleExplosionEmote(); }, TIMER_COMBAT_COMBAT);
        AddCustomAction(TALON_KING_IKISS_ARCANE_EXPLOSION_CAST, true, [&]() { HandleArcaneExplosionCast(); }, TIMER_COMBAT_COMBAT);
        AddCustomAction(TALON_KING_IKISS_ARCANE_EXPLOSION_ATTACK_START, true, [&]() { HandleArcaneExplosionAttackStart(); }, TIMER_COMBAT_COMBAT);
        AddOnKillText(SAY_SLAY_1, SAY_SLAY_2);
    }

    ScriptedInstance* m_instance;                           // 副本实例数据指针
    bool m_isRegularMode;                                   // 是否为普通难度模式

    bool m_Intro;                                           // 是否已播放进视线开场
    uint8 m_uiBlinkPhase;                                   // 闪现阶段计数（0=80%, 1=50%, 2=25%）
    float m_HealthPercent;                                  // 当前待触发闪现的血量阈值

    void Reset() override                                   // 遭遇战重置回调
    {
        CombatAI::Reset();

        SetCombatMovement(true);
        SetCombatScriptStatus(false);
        SetMeleeEnabled(true);

        m_uiBlinkPhase = 0;
        m_HealthPercent = 80.0f;
    }

    uint32 GetSubsequentActionTimer(const uint32 action) const // 获取后续动作冷却时间
    {
        switch (action)
        {
            case TALON_KING_IKISS_ACTION_SLOW: return urand(15000, 20000);
            case TALON_KING_IKISS_ACTION_POLYMORPH: return urand(10000, 15000);
            case TALON_KING_IKISS_ACTION_ARCANE_VOLLEY: return urand(8000, 12000);
            default: return 0; // never occurs but for compiler (编译器安全默认值)
        }
    }

    void MoveInLineOfSight(Unit* pWho) override              // 玩家进入视线触发开场台词
    {
        if (!m_creature->GetVictim() && m_creature->CanAttackOnSight(pWho) && pWho->isInAccessablePlaceFor(m_creature))
        {
            if (!m_Intro && m_creature->IsWithinDistInMap(pWho, 100.0f))
            {
                m_Intro = true;
                DoBroadcastText(SAY_INTRO, m_creature);
            }
        }

        ScriptedAI::MoveInLineOfSight(pWho);
    }

    void Aggro(Unit* /*who*/) override                      // 进入战斗喊话并置副本状态为进行中
    {
        switch (urand(0, 2))
        {
            case 0: DoBroadcastText(SAY_AGGRO_1, m_creature); break;
            case 1: DoBroadcastText(SAY_AGGRO_2, m_creature); break;
            case 2: DoBroadcastText(SAY_AGGRO_3, m_creature); break;
        }

        if (m_instance)
            m_instance->SetData(TYPE_IKISS, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override                // 首领死亡回调：播放遗言并置状态为已完成
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_IKISS, DONE);
    }

    void JustReachedHome() override                         // 脱战回位回调：置状态为失败
    {
        if (m_instance)
            m_instance->SetData(TYPE_IKISS, FAIL);
    }

    void HandleArcaneExplosionCast()                         // 施放奥术暴爆与奥术气泡
    {
        DoCastSpellIfCan(nullptr, m_isRegularMode ? SPELL_ARCANE_EXPLOSION : SPELL_ARCANE_EXPLOSION_H);
        DoCastSpellIfCan(nullptr, SPELL_ARCANE_BUBBLE, CAST_TRIGGERED);
    }

    void HandleExplosionEmote()                              // 播放奥术暴爆表情警示
    {
        DoBroadcastText(EMOTE_ARCANE_EXP, m_creature);
    }

    void HandleArcaneExplosionAttackStart()                  // 暴爆结束：清空仇恨并恢复追击近战
    {
        DoResetThreat();
        SetCombatMovement(true);
        SetMeleeEnabled(true);
        SetCombatScriptStatus(false);
    }

    void ExecuteAction(uint32 action) override              // 执行就绪的战斗动作
    {
        switch (action)
        {
            case TALON_KING_IKISS_MANA_SHIELD:
                if (m_creature->GetHealthPercent() < 15.0f)
                    if (DoCastSpellIfCan(nullptr, SPELL_MANA_SHIELD) == CAST_OK)
                        DisableCombatAction(action);
                break;
            case TALON_KING_IKISS_BLINK_START:
                if (m_creature->GetHealthPercent() < m_HealthPercent)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_BLINK) == CAST_OK)
                    {
                        SetMeleeEnabled(false);
                        SetCombatMovement(false);
                        SetCombatScriptStatus(true);
                        ResetTimer(TALON_KING_IKISS_ARCANE_EXPLOSION_EMOTE, 1500);
                        ResetTimer(TALON_KING_IKISS_ARCANE_EXPLOSION_CAST, 2500);
                        ResetTimer(TALON_KING_IKISS_ARCANE_EXPLOSION_ATTACK_START, 8500);
                        

                        // There is no relationship between the health percentages (更新下一阶段血量触发线)
                        switch (m_uiBlinkPhase)
                        {
                            case 0: m_HealthPercent = 50.0f; break;
                            case 1: m_HealthPercent = 25.0f; break;
                            case 2: m_HealthPercent = 0.0f; break;
                        }

                        ++m_uiBlinkPhase;
                    }
                }
                break;
            case TALON_KING_IKISS_ACTION_SLOW:
            {
                if (!m_isRegularMode)
                    if (DoCastSpellIfCan(nullptr, SPELL_SLOW_H) == CAST_OK)
                        ResetCombatAction(action, GetSubsequentActionTimer(action));
                break;
            }
            case TALON_KING_IKISS_ACTION_POLYMORPH:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER | SELECT_FLAG_NOT_AURA | SELECT_FLAG_SKIP_TANK))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_POLYMORPH : SPELL_POLYMORPH_H) == CAST_OK)
                        ResetCombatAction(action, GetSubsequentActionTimer(action));
                break;
            }
            case TALON_KING_IKISS_ACTION_ARCANE_VOLLEY:
            {
                if (DoCastSpellIfCan(nullptr, m_isRegularMode ? SPELL_ARCANE_VOLLEY : SPELL_ARCANE_VOLLEY_H) == CAST_OK)
                    ResetCombatAction(action, GetSubsequentActionTimer(action));
            }
            break;
        }
    }
};

// 注册利爪之王艾吉斯脚本
void AddSC_boss_talon_king_ikiss()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_talon_king_ikiss";
    pNewScript->GetAI = &GetNewAIInstance<boss_talon_king_ikissAI>;
    pNewScript->RegisterSelf();
}
