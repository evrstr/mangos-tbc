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
SDName: Boss_Gruul
SD%Complete: 100
SDComment:
SDCategory: Gruul's Lair
EndScriptData */

// 屠龙者格鲁尔（格鲁尔的巢穴最终首领）
/* 遭遇战概述：
 * 屠龙者格鲁尔是戈隆的半神始祖，战斗中核心软狂暴机制为生长（每 30 秒体型变大并提高攻击力）；
 * 技能包含恶毒打击（对第二仇恨目标的高伤害打击）、落石（洞穴坍塌）、回音击与地面震击；
 * 地面震击后将玩家击飞并施加石化倒计时，倒计时结束触发碎裂（彼此靠近的玩家会产生致命的连环爆炸）。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "gruuls_lair.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 台词、表情与法术枚举
enum
{
    SAY_AGGRO                   = -1565010,                 // 开战台词
    SAY_SLAM1                   = -1565011,                 // 地面震击台词 1
    SAY_SLAM2                   = -1565012,                 // 地面震击台词 2
    SAY_SHATTER1                = -1565013,                 // 碎裂台词 1
    SAY_SHATTER2                = -1565014,                 // 碎裂台词 2
    SAY_SLAY1                   = 20121,                    // 击杀玩家台词 1
    SAY_SLAY2                   = 20122,                    // 击杀玩家台词 2
    SAY_SLAY3                   = 20123,                    // 击杀玩家台词 3
    SAY_DEATH                   = -1565018,                 // 死亡台词

    EMOTE_GROW                  = -1565019,                 // 体型长大表情
    EMOTE_SLAM                  = -1565020,                 // 地面猛击碎裂表情

    SPELL_GROWTH                = 36300,                    // 生长（每层增加15%伤害，体型变大）
    SPELL_CAVE_IN               = 36240,                    // 洞穴坍塌（落石伤害）
    SPELL_GROUND_SLAM_DUMMY     = 39188,                    // 地面震击前置
    SPELL_LOOK_AROUND           = 33965,                    // 环视动作
    SPELL_GROUND_SLAM           = 33525,                    // AoE Ground Slam applying Ground Slam to everyone with a script effect (most likely the knock back, we can code it to a set knockback) (地面震击击退)
    SPELL_REVERBERATION         = 36297,                    // 回音击（沉默）
    SPELL_SHATTER               = 33654,                    // 碎裂

    SPELL_SHATTER_EFFECT        = 33671,                    // 碎裂伤害效果
    SPELL_HURTFUL_STRIKE        = 33812,                    // Finds target for 33813 (恶毒打击目标搜寻)
    SPELL_STONED                = 33652,                    // Spell is self cast by target (石化状态)
};

// 格鲁尔战斗行为动作枚举
enum GruulActions
{
    GRUUL_ACTION_SHATTER,                                   // 碎裂动作
    GRUUL_ACTION_GROW,                                      // 生长动作
    GRUUL_ACTION_GROUND_SLAM,                               // 地面震击动作
    GRUUL_ACTION_REVERBERATION,                             // 回音击动作
    GRUUL_ACTION_CAVE_IN,                                   // 洞穴坍塌动作
    GRUUL_ACTION_HURTFUL_STRIKE,                            // 恶毒打击动作
    GRUUL_ACTION_MAX,
};

// 屠龙者格鲁尔 AI
struct boss_gruulAI : public CombatAI
{
    boss_gruulAI(Creature* creature) : CombatAI(creature, GRUUL_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddTimerlessCombatAction(GRUUL_ACTION_SHATTER, false);
        AddCombatAction(GRUUL_ACTION_GROW, 30000u);
        AddCombatAction(GRUUL_ACTION_CAVE_IN, 10000u);
        AddCombatAction(GRUUL_ACTION_REVERBERATION, 115000u);
        AddCombatAction(GRUUL_ACTION_GROUND_SLAM, 35000, 40000);
        AddCombatAction(GRUUL_ACTION_HURTFUL_STRIKE, 6000u);
        AddOnKillText(SAY_SLAY1, SAY_SLAY2, SAY_SLAY3);
    }

    ScriptedInstance* m_instance;                           // 副本数据管理指针

    bool m_lookAround;                                      // 是否处于震击后环视状态

    void Reset() override                                   // 重置遭遇战
    {
        CombatAI::Reset();
        SetCombatMovement(true);
        SetMeleeEnabled(true);
    }

    void Aggro(Unit* /*who*/) override                      // 进入战斗喊话并置状态为进行中
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_GRUUL_EVENT, IN_PROGRESS);
    }

    void JustReachedHome() override                         // 脱战回位回调：置状态为失败
    {
        if (m_instance)
            m_instance->SetData(TYPE_GRUUL_EVENT, FAIL);
    }

    void JustDied(Unit* /*pKiller*/) override               // 首领死亡回调：播放遗言并置状态为已完成
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_GRUUL_EVENT, DONE);
    }

    void ExecuteAction(uint32 action) override              // 执行格鲁尔技能动作
    {
        switch (action)
        {
            case GRUUL_ACTION_SHATTER:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_SHATTER) == CAST_OK)
                {
                    DoScriptText(EMOTE_SLAM, m_creature);
                    DoScriptText(urand(0, 1) ? SAY_SHATTER1 : SAY_SHATTER2, m_creature);
                    SetActionReadyStatus(action, false);
                    return;
                }
                return;
            }
            case GRUUL_ACTION_GROW:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_GROWTH) == CAST_OK)
                {
                    DoScriptText(EMOTE_GROW, m_creature);
                    ResetCombatAction(action, 30000);
                    return;
                }
                return;
            }
            case GRUUL_ACTION_GROUND_SLAM:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_GROUND_SLAM_DUMMY) == CAST_OK)
                {
                    m_creature->CastSpell(nullptr, SPELL_GROUND_SLAM, TRIGGERED_NONE);
                    DoScriptText(urand(0, 1) ? SAY_SLAM1 : SAY_SLAM2, m_creature);
                    m_creature->SetTarget(nullptr);
                    ResetCombatAction(action, urand(70000, 80000));
                    SetActionReadyStatus(GRUUL_ACTION_SHATTER, true); // top priority, blocked by stun (震击后准备碎裂)
                    m_lookAround = true;
                    return;
                }
                return;
            }
            case GRUUL_ACTION_REVERBERATION:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_REVERBERATION) == CAST_OK)
                    ResetCombatAction(action, urand(35000, 45000));
                return;
            }
            case GRUUL_ACTION_CAVE_IN:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, SPELL_CAVE_IN) == CAST_OK)
                        ResetCombatAction(action, 8500);
                return;
            }
            case GRUUL_ACTION_HURTFUL_STRIKE:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_HURTFUL_STRIKE) == CAST_OK)
                    ResetCombatAction(action, 8000);
                break;
            }
        }
    }

    void SpellHitTarget(Unit* /*target*/, const SpellEntry* spell) override // 法术命中：震击后环视四周
    {
        if (spell->Id == SPELL_GROUND_SLAM && m_lookAround)
        {
            m_lookAround = false;
            m_creature->CastSpell(nullptr, SPELL_LOOK_AROUND, TRIGGERED_NONE);
        }
    }
};

// 戈隆之握光环脚本：叠加至 5 层转化为完全石化状态
struct GronnLordsGrasp : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (apply)
            if (aura->GetStackAmount() >= 5 && !aura->GetTarget()->HasAura(33652))
                aura->GetTarget()->CastSpell(nullptr, 33652, TRIGGERED_OLD_TRIGGERED);
    }
};

// 恶毒打击目标引导脚本：搜寻近战范围内威胁值最高的非主坦克玩家
struct HurtfulStrikePrimer : public SpellScript
{
    bool OnCheckTarget(const Spell* spell, Unit* target, SpellEffectIndex /*eff*/) const override
    {
        if (target->GetTypeId() != TYPEID_PLAYER || !spell->GetCaster()->CanReachWithMeleeAttack(target))
            return false;
        return true;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* target = spell->GetUnitTarget();
        Unit* caster = spell->GetCaster();
        auto& targetInfo = spell->GetTargetList();
        if (!target || targetInfo.rbegin()->targetGUID != target->GetObjectGuid())
            return;

        for (auto& targetInfo : targetInfo)
        {
            if (caster->GetMap()->GetPlayer(targetInfo.targetGUID) == caster->GetVictim())
                continue;

            if (caster->getThreatManager().getThreat(caster->GetMap()->GetPlayer(targetInfo.targetGUID)) > caster->getThreatManager().getThreat(target) || target == caster->GetVictim())
                target = caster->GetMap()->GetPlayer(targetInfo.targetGUID);
        }

        caster->CastSpell(target, 33813, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_NORMAL_COMBAT_CAST);
    }
};

// 注册格鲁尔及法术脚本
void AddSC_boss_gruul()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_gruul";
    pNewScript->GetAI = &GetNewAIInstance<boss_gruulAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<GronnLordsGrasp>("spell_gronn_lords_grasp");
    RegisterSpellScript<HurtfulStrikePrimer>("spell_hurtful_strike_primer");
}
