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
SDName: Boss_NexusPrince_Shaffar
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Mana Tombs
EndScriptData */

/* ContentData
boss_nexusprince_shaffar
EndContentData */

// 节点亲王沙法尔（法力陵墓最终首领）
/* 遭遇战概述：
 * 节点亲王沙法尔是法力陵墓的最终首领，战斗中会定期召唤虚灵信标（信标会变身为虚灵学徒），
 * 同时自身会频繁使用冰霜新星、闪现术、火球术与寒冰箭对玩家进行远程风筝打击。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 台词、法术与点位枚举
enum
{
    SAY_INTRO                       = 17784,                    // 进视线开场台词
    SAY_AGGRO_1                     = 17780,                    // 开战台词 1
    SAY_AGGRO_2                     = 17781,                    // 开战台词 2
    SAY_AGGRO_3                     = 17782,                    // 开战台词 3
    SAY_SLAY_1                      = 17779,                    // 击杀玩家台词 1
    SAY_SLAY_2                      = 17778,                    // 击杀玩家台词 2
    SAY_SUMMON                      = 17783,                    // 召唤信标台词
    SAY_DEAD                        = 17777,                    // 死亡台词

    SPELL_BLINK                     = 34605,                    // 闪现术
    SPELL_FROSTBOLT                 = 32364,                    // 寒冰箭
    SPELL_FIREBALL                  = 32363,                    // 火球术
    SPELL_FROSTNOVA                 = 32365,                    // 冰霜新星

    SPELL_ETHEREAL_BEACON           = 32371,                // Summons 18431 (召唤虚灵信标)
    // SPELL_ETHEREAL_BEACON_VISUAL  = 32368,               // included in creature_template_addon

    POINT_MOVE_DISTANCE = 1,                                    // 移动距离标识
};

// 沙法尔战斗行为枚举（按优先级排序）
enum ShaffarActions // order based on priority
{
    SHAFFAR_ACTION_BEACON,                                      // 召唤信标行为
    SHAFFAR_ACTION_FROSTNOVA,                                   // 冰霜新星行为
    SHAFFAR_ACTION_BLINK,                                       // 闪现术行为
    SHAFFAR_ACTION_PRIMARY_SPELL,                               // 主要法术（火球/寒冰箭）
    SHAFFAR_ACTION_MAX
};

// 节点亲王沙法尔 AI
struct boss_nexusprince_shaffarAI : public CombatAI
{
    boss_nexusprince_shaffarAI(Creature* creature) : CombatAI(creature, SHAFFAR_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())),
        m_isRegularMode(creature->GetMap()->IsRegularDifficulty()), m_hasTaunted(false)
    {
        if (m_isRegularMode)
        {
            AddCombatAction(SHAFFAR_ACTION_BEACON, 16000, 20000);
            AddCombatAction(SHAFFAR_ACTION_FROSTNOVA, 16000, 25000);
            AddCombatAction(SHAFFAR_ACTION_BLINK, 45000, 60000);
            AddCombatAction(SHAFFAR_ACTION_PRIMARY_SPELL, 3000, 5000);
        }
        else
        {
            AddCombatAction(SHAFFAR_ACTION_BEACON, 15000u);
            AddCombatAction(SHAFFAR_ACTION_FROSTNOVA, 10000, 25000);
            AddCombatAction(SHAFFAR_ACTION_BLINK, 20000, 25000);
            AddCombatAction(SHAFFAR_ACTION_PRIMARY_SPELL, 1200, 2400);
        }
        AddDistanceSpell(SPELL_FROSTNOVA);
        SetRangedMode(true, 30.f, TYPE_PROXIMITY);
        AddOnKillText(SAY_SLAY_1, SAY_SLAY_2);
    }

    ScriptedInstance* m_instance;                               // 副本实例数据指针
    bool m_isRegularMode;                                       // 是否为普通难度模式

    bool m_hasTaunted;                                          // 是否已触发进视线嘲讽

    // 获取后续行为的重置时间间隔
    uint32 GetSubsequentActionTimer(ShaffarActions id)
    {
        if (m_isRegularMode)
        {
            switch (id)
            {
                case SHAFFAR_ACTION_BEACON: return urand(16000, 20000);
                case SHAFFAR_ACTION_FROSTNOVA: return urand(16000, 25000);
                case SHAFFAR_ACTION_BLINK: return urand(25000, 35000);
                case SHAFFAR_ACTION_PRIMARY_SPELL: return urand(3000, 5000);
                default: return 0;
            }
        }
        else
        {
            switch (id)
            {
                case SHAFFAR_ACTION_BEACON: return 15000;
                case SHAFFAR_ACTION_FROSTNOVA: return urand(12000, 20000);
                case SHAFFAR_ACTION_BLINK: return urand(20000, 25000);
                case SHAFFAR_ACTION_PRIMARY_SPELL: return urand(1000, 2000);
                default: return 0;
            }
        }
    }

    void MoveInLineOfSight(Unit* who) override                  // 玩家进入视线触发开场台词
    {
        if (!m_hasTaunted && who->GetTypeId() == TYPEID_PLAYER && m_creature->IsWithinDistInMap(who, 100.0f) && m_creature->IsWithinLOSInMap(who))
        {
            DoBroadcastText(SAY_INTRO, m_creature);
            m_hasTaunted = true;
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void Aggro(Unit* /*who*/) override                          // 进入战斗随机喊话
    {
        switch (urand(0, 2))
        {
            case 0: DoBroadcastText(SAY_AGGRO_1, m_creature); break;
            case 1: DoBroadcastText(SAY_AGGRO_2, m_creature); break;
            case 2: DoBroadcastText(SAY_AGGRO_3, m_creature); break;
        }
    }

    void JustSummoned(Creature* summoned) override              // 召唤随从回调：命令其攻击随机玩家
    {
        if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
            summoned->AI()->AttackStart(pTarget);
    }

    void JustDied(Unit* /*killer*/) override                    // 首领死亡喊话
    {
        DoBroadcastText(SAY_DEAD, m_creature);
    }

    void ExecuteAction(uint32 action) override                  // 执行具体的战斗动作
    {
        switch (action)
        {
            case SHAFFAR_ACTION_BEACON:
                if (DoCastSpellIfCan(nullptr, SPELL_ETHEREAL_BEACON) == CAST_OK)
                {
                    if (!urand(0, 3))
                        DoBroadcastText(SAY_SUMMON, m_creature);

                    ResetCombatAction(action, GetSubsequentActionTimer(ShaffarActions(action)));
                    return;
                }
                break;
            case SHAFFAR_ACTION_FROSTNOVA:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_NEAREST_BY, 0, SPELL_FROSTNOVA, SELECT_FLAG_PLAYER | SELECT_FLAG_USE_EFFECT_RADIUS))
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_FROSTNOVA) == CAST_OK)
                    {
                        ResetCombatAction(action, GetSubsequentActionTimer(ShaffarActions(action)));
                        return;
                    }
                }
                break;
            case SHAFFAR_ACTION_BLINK:
                if (m_creature->GetVictim() && m_creature->CanReachWithMeleeAttack(m_creature->GetVictim()) && DoCastSpellIfCan(nullptr, SPELL_BLINK) == CAST_OK)
                {
                    ResetCombatAction(action, GetSubsequentActionTimer(ShaffarActions(action)));
                    return;
                }
                break;
            case SHAFFAR_ACTION_PRIMARY_SPELL:
            {
                uint32 spell = urand(0, 1) ? SPELL_FIREBALL : SPELL_FROSTBOLT;
                if (DoCastSpellIfCan(m_creature->GetVictim(), spell) == CAST_OK)
                    ResetCombatAction(action, GetSubsequentActionTimer(ShaffarActions(action)));
                return;
            }
            break;
        }
    }
};

// 注册节点亲王沙法尔脚本
void AddSC_boss_nexusprince_shaffar()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_nexusprince_shaffar";
    pNewScript->GetAI = &GetNewAIInstance<boss_nexusprince_shaffarAI>;
    pNewScript->RegisterSelf();
}
