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
SDName: Boss_Darkweaver_Syth
SD%Complete: 100
SDComment:
SDCategory: Auchindoun, Sethekk Halls
EndScriptData */

// 暗织者斯瑟（塞泰克大厅一号首领）
/* 遭遇战概述：
 * 暗织者斯瑟是塞泰克大厅的一号首领，精通各类震击法术与闪电链；
 * 当生命值分别降至 90%、55%、15% 时，会同时召唤四个元素微粒（火焰、奥术、冰霜、暗影）协助战斗。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 台词、法术与随从生物枚举
enum
{
    SAY_SUMMON                  = 17733,                    // 召唤元素台词
    SAY_AGGRO_1                 = 17734,                    // 开战台词 1
    SAY_AGGRO_2                 = 17735,                    // 开战台词 2
    SAY_AGGRO_3                 = 17736,                    // 开战台词 3
    SAY_SLAY_1                  = 17737,                    // 击杀玩家台词 1
    SAY_SLAY_2                  = 17738,                    // 击杀玩家台词 2
    SAY_DEATH                   = 17739,                    // 死亡台词

    SPELL_FROST_SHOCK           = 12548,                    // 冰霜震击（普通难度）
    SPELL_FROST_SHOCK_H         = 21401,                    // 冰霜震击（英雄难度）
    SPELL_FLAME_SHOCK           = 15039,                    // 烈焰震击（普通难度）
    SPELL_FLAME_SHOCK_H         = 15616,                    // 烈焰震击（英雄难度）
    SPELL_SHADOW_SHOCK          = 33620,                    // 暗影震击（普通难度）
    SPELL_SHADOW_SHOCK_H        = 38136,                    // 暗影震击（英雄难度）
    SPELL_ARCANE_SHOCK          = 33534,                    // 奥术震击（普通难度）
    SPELL_ARCANE_SHOCK_H        = 38135,                    // 奥术震击（英雄难度）

    SPELL_CHAIN_LIGHTNING       = 15659,                    // 闪电链（普通难度）
    SPELL_CHAIN_LIGHTNING_H     = 15305,                    // 闪电链（英雄难度）

    SPELL_SUMMON_SYTH_FIRE      = 33537,                    // Spawns 19203 (召唤火元素微粒)
    SPELL_SUMMON_SYTH_ARCANE    = 33538,                    // Spawns 19205 (召唤奥术元素微粒)
    SPELL_SUMMON_SYTH_FROST     = 33539,                    // Spawns 19204 (召唤冰元素微粒)
    SPELL_SUMMON_SYTH_SHADOW    = 33540,                    // Spawns 19206 (召唤暗影元素微粒)

    // Npc entries (随从生物条目)
    NPC_FIRE_ELEMENTAL          = 19203,                    // 火元素微粒
    NPC_FROST_ELEMENTAL         = 19204,                    // 冰霜元素微粒
    NPC_ARCANE_ELEMENTAL        = 19205,                    // 奥术元素微粒
    NPC_SHADOW_ELEMENTAL        = 19206,                    // 暗影元素微粒
};

// 斯瑟阶段动作枚举（血量百分比触发）
enum SythActions
{
    SYTH_90,                                                // 90% 血量召唤动作
    SYTH_55,                                                // 55% 血量召唤动作
    SYTH_15,                                                // 15% 血量召唤动作
    SYTH_ACTION_MAX
};

// 暗织者斯瑟 AI
struct boss_darkweaver_sythAI : public CombatAI
{
    boss_darkweaver_sythAI(Creature* creature) : CombatAI(creature, SYTH_ACTION_MAX), m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddTimerlessCombatAction(SYTH_90, true);
        AddTimerlessCombatAction(SYTH_55, true);
        AddTimerlessCombatAction(SYTH_15, true);
        AddOnKillText(SAY_SLAY_1, SAY_SLAY_2);
    }

    bool m_isRegularMode;                                   // 是否为普通难度模式

    void Aggro(Unit* /*who*/) override                      // 进入战斗喊话
    {
        switch (urand(0, 2))
        {
            case 0: DoBroadcastText(SAY_AGGRO_1, m_creature); break;
            case 1: DoBroadcastText(SAY_AGGRO_2, m_creature); break;
            case 2: DoBroadcastText(SAY_AGGRO_3, m_creature); break;
        }
    }
    
    void JustReachedHome() override                         // 脱战回位回调：清除召唤随从
    {
        m_creature->RemoveGuardians();
    }

    void JustDied(Unit* /*killer*/) override                // 首领死亡回调：播放遗言并清除随从
    {
        DoBroadcastText(SAY_DEATH, m_creature);
        m_creature->RemoveGuardians();
    }

    void JustSummoned(Creature* summoned) override          // 随从召唤回调：命令攻击随机玩家
    {
        if (Unit* Target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
            summoned->AI()->AttackStart(Target);
    }

    // Wrapper to handle the elementals summon (同时召唤四系元素微粒封装函数)
    void SythSummoning()
    {
        DoBroadcastText(SAY_SUMMON, m_creature);

        m_creature->CastSpell(nullptr, SPELL_SUMMON_SYTH_ARCANE, TRIGGERED_IGNORE_COOLDOWNS); // front (前方召唤奥术微粒)
        m_creature->CastSpell(nullptr, SPELL_SUMMON_SYTH_FIRE, TRIGGERED_IGNORE_COOLDOWNS); // back (后方召唤火焰微粒)
        m_creature->CastSpell(nullptr, SPELL_SUMMON_SYTH_FROST, TRIGGERED_IGNORE_COOLDOWNS); // left (左侧召唤冰霜微粒)
        m_creature->CastSpell(nullptr, SPELL_SUMMON_SYTH_SHADOW, TRIGGERED_IGNORE_COOLDOWNS); // right (右侧召唤暗影微粒)
    }

    void ExecuteAction(uint32 action) override              // 执行血量百分比召唤行为
    {
        if (!CanExecuteCombatAction())
            return;

        switch (action)
        {
            case SYTH_90:
                if (m_creature->GetHealthPercent() < 90.0f)
                {
                    SythSummoning();
                    SetActionReadyStatus(action, false);
                }
                break;
            case SYTH_55:
                if (m_creature->GetHealthPercent() < 55.0f)
                {
                    SythSummoning();
                    SetActionReadyStatus(action, false);
                }
                break;
            case SYTH_15:
                if (m_creature->GetHealthPercent() < 15.0f)
                {
                    SythSummoning();
                    SetActionReadyStatus(action, false);
                }
                break;
        }
    }
};

// 注册暗织者斯瑟脚本
void AddSC_boss_darkweaver_syth()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_darkweaver_syth";
    pNewScript->GetAI = &GetNewAIInstance<boss_darkweaver_sythAI>;
    pNewScript->RegisterSelf();
}
