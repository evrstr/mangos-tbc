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
SDName: Boss_Ambassador_Hellmaw
SD%Complete: 80
SDComment: Enrage spell missing/not known
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

// 赫尔默大使（暗影迷宫一号首领）
/* 遭遇战概述：
 * 赫尔默大使是暗影迷宫的一号首领，初始处于暗影议会苦修者的束缚放逐状态中；
 * 当场地内的所有苦修者被击杀后，放逐解除并进入巡逻攻击状态；
 * 战斗中会施放腐蚀强酸、群体恐惧并在生命值低下时激怒狂暴。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "shadow_labyrinth.h"

// 台词与法术枚举
enum
{
    SAY_AGGRO_1             = 17860,                    // 开战台词 1
    SAY_AGGRO_2             = 17861,                    // 开战台词 2
    SAY_AGGRO_3             = 17863,                    // 开战台词 3
    SAY_HELP                = 17859,                    // 呼救台词
    SAY_SLAY_1              = 17864,                    // 击杀玩家台词 1
    SAY_SLAY_2              = 17865,                    // 击杀玩家台词 2
    SAY_DEATH               = 17866,                    // 死亡台词

    SPELL_CORROSIVE_ACID    = 33551,                    // 腐蚀强酸
    SPELL_FEAR              = 33547,                    // 恐惧术
    SPELL_ENRAGE            = 34970                     // 狂暴
};

// 赫尔默大使行为枚举
enum AmbassadorHellmawActions
{
    AMBASSADOR_HELLMAW_ACTION_MAX,
    AMBASSADOR_HELLMAW_UNBANISH_CHECK                   // 解除放逐检测动作
};

// 赫尔默大使 AI
struct boss_ambassador_hellmawAI : public CombatAI
{
    boss_ambassador_hellmawAI(Creature* creature) : CombatAI(creature, AMBASSADOR_HELLMAW_ACTION_MAX),
        m_instance(static_cast<instance_shadow_labyrinth*>(creature->GetInstanceData())), m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddCustomAction(AMBASSADOR_HELLMAW_UNBANISH_CHECK, true, [&]() { UnbanishCheck(); });
        AddOnKillText(SAY_SLAY_1, SAY_SLAY_2);
    }

    instance_shadow_labyrinth* m_instance;              // 副本数据管理指针
    bool m_isRegularMode;                               // 是否为普通难度模式

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override // 接收 AI 事件
    {
        if (eventType == AI_EVENT_CUSTOM_A) // all channelers killed (所有苦修者死亡，解除放逐)
            Unbanish();
        else if (AI_EVENT_CUSTOM_B) // respawn (首领重生检测)
            ResetTimer(AMBASSADOR_HELLMAW_UNBANISH_CHECK, 2000);
    }

    void JustReachedHome() override                     // 脱战回位回调：置状态为失败
    {
        if (m_instance)
            m_instance->SetData(TYPE_HELLMAW, FAIL);
    }

    void Aggro(Unit* /*who*/) override                  // 进入战斗喊话并置状态为进行中
    {
        switch (urand(0, 2))
        {
            case 0: DoBroadcastText(SAY_AGGRO_1, m_creature); break;
            case 1: DoBroadcastText(SAY_AGGRO_2, m_creature); break;
            case 2: DoBroadcastText(SAY_AGGRO_3, m_creature); break;
        }

        if (m_instance)
            m_instance->SetData(TYPE_HELLMAW, IN_PROGRESS);
    }

    void JustDied(Unit* /*killer*/) override            // 首领死亡回调：播放遗言并置状态为已击杀
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_HELLMAW, DONE);
    }

    void Unbanish()                                     // 解除放逐并激活攻击巡逻
    {
        // yell intro and remove banish aura (播放音效并移除放逐光环)
        m_creature->PlayDirectSound(10473, PlayPacketSettings::ZONE);
        m_creature->GetMotionMaster()->Clear(false, true);
        m_creature->GetMotionMaster()->MoveWaypoint();
        m_creature->RemoveAurasDueToSpell(SPELL_BANISH);
        m_creature->SetCanEnterCombat(true);
        SetReactState(REACT_AGGRESSIVE);
    }

    void UnbanishCheck()                                // 检查是否满足解除放逐条件
    {
        if (m_instance && !m_instance->IsHellmawUnbanished())
        {
            DoCastSpellIfCan(nullptr, SPELL_BANISH, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
            m_creature->SetCanEnterCombat(false);
            SetReactState(REACT_PASSIVE);
        }
        else
            Unbanish();
    }
};

// 注册赫尔默大使脚本
void AddSC_boss_ambassador_hellmaw()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_ambassador_hellmaw";
    pNewScript->GetAI = &GetNewAIInstance<boss_ambassador_hellmawAI>;
    pNewScript->RegisterSelf();
}
