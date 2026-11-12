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
SDName: Boss_Blackheart_the_Inciter
SD%Complete: 90
SDComment: Not all yells are implemented.
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

// 煽动者布莱卡特（暗影迷宫二号首领）
/* 遭遇战概述：
 * 煽动者布莱卡特是暗影迷宫的二号首领，战斗中会定期施放煽动混乱（心控所有玩家互相攻击并提升输出）；
 * 心控期间首领自身定身并狂笑，心控结束后清空仇恨，此外还会施放冲锋与战争践踏。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "shadow_labyrinth.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "Spells/Scripts/SpellScript.h"
#include "World/WorldStateDefines.h"

// 台词、法术、阵营与随从条目枚举
enum
{
    SPELL_CHARGE                = 33709,                       // 冲锋
    SPELL_WAR_STOMP             = 33707,                       // 战争践踏
    
    // incite chaos mechanic spells (煽动混乱机制法术)
    SPELL_INCITE_CHAOS          = 33676,                       // spawns triggers and roots/stuns blackheart (生成触发器并定身布莱卡特)
    SPELL_INCITE_CHAOS_CHARM    = 33684,                       // cast by triggers into players (心控玩家法术)
    SPELL_INCITE_CHAOS_UNK      = 33687,
    SPELL_THREAT                = 34915,                       // Unk purpose, threat is removed right after (仇恨处理法术)
    SPELL_INCITER_THREAT_TRIGGER= 45339,
    SPELL_LAUGH                 = 33716,                       // used during incite chaos onto one player every 2-4 seconds TODO: add usage (狂笑法术)
    SPELL_LAUGH_PERIODIC        = 33722,                       // 周期性狂笑法术
    SPELL_LAUGH_UNK             = 33797,

    SPELL_DUMMY_NUKE            = 21912,                       // 占位伤害法术

    SPELL_INCITE_CHAOS_SPAWN_1  = 33677,                       // 召唤心控触发器 1
    SPELL_INCITE_CHAOS_SPAWN_2  = 33680,                       // 召唤心控触发器 2
    SPELL_INCITE_CHAOS_SPAWN_3  = 33681,                       // 召唤心控触发器 3
    SPELL_INCITE_CHAOS_SPAWN_4  = 33682,                       // 召唤心控触发器 4
    SPELL_INCITE_CHAOS_SPAWN_5  = 33683,                       // 召唤心控触发器 5

    SAY_AGGRO_1             = 17563,                           // 开战台词 1
    SAY_AGGRO_2             = 17565,                           // 开战台词 2
    SAY_SLAY1               = 17573,                           // 击杀玩家台词 1
    SAY_SLAY2               = 19528,                           // 击杀玩家台词 2
    SAY_INCITE              = 16433,                           // 煽动混乱台词
    SAY_DEATH               = 17569,                           // 死亡台词

    // faction IDs used during incite chaos - each trigger has one (心控期间分配的互相对立阵营 ID)
    FACTION_INCITER_1       = 1761,
    FACTION_INCITER_2       = 1762,
    FACTION_INCITER_3       = 1763,
    FACTION_INCITER_4       = 1764,
    FACTION_INCITER_5       = 1765,

    // npcs summoned during incite chaos, each one charms a different player - propagates factions above to them (心控触发器生物条目)
    NPC_INCITER_TRIGGER_1   = 19300, // summoned by spellid 33677
    NPC_INCITER_TRIGGER_2   = 19301, // summoned by spellid 33680
    NPC_INCITER_TRIGGER_3   = 19302, // summoned by spellid 33681
    NPC_INCITER_TRIGGER_4   = 19303, // summoned by spellid 33682
    NPC_INCITER_TRIGGER_5   = 19304, // summoned by spellid 33683
};

// 战斗行为动作枚举
enum BlackheartActions
{
    BLACKHEART_ACTION_MAX,
    BLACKHEART_INCITE_TIMER,                                    // 混乱结束计时动作
};

// 煽动者布莱卡特 AI
struct boss_blackheart_the_inciterAI : public CombatAI
{
    boss_blackheart_the_inciterAI(Creature* creature) : CombatAI(creature, BLACKHEART_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddCustomAction(BLACKHEART_INCITE_TIMER, true, [&]() { HandleInciteEnd(); });
        AddOnKillText(SAY_SLAY1, SAY_SLAY2);
    }

    ScriptedInstance* m_instance;                               // 副本数据管理指针
    bool m_isRegularMode;                                       // 是否为普通难度模式
    GuidVector m_vTargetsGuids;                                 // 目标 GUID 列表

    void JustDied(Unit* /*killer*/) override                    // 首领死亡回调
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_INCITER, DONE);
    }

    void Aggro(Unit* /*who*/) override                          // 进入战斗喊话并置状态为进行中
    {
        switch (urand(0, 1))
        {
            case 0: DoBroadcastText(SAY_AGGRO_1, m_creature); break;
            case 1: DoBroadcastText(SAY_AGGRO_2, m_creature); break;
        }

        m_creature->GetMap()->GetVariableManager().SetVariable(WORLD_STATE_SHADOW_LAB_ASSASIN_01, 0);
        m_creature->GetMap()->GetVariableManager().SetVariable(WORLD_STATE_SHADOW_LAB_ASSASIN_02, 0);

        if (m_instance)
            m_instance->SetData(TYPE_INCITER, IN_PROGRESS);
    }

    void JustReachedHome() override                             // 脱战回位回调：置状态为失败
    {
        if (m_instance)
            m_instance->SetData(TYPE_INCITER, FAIL);
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override // 施法完成回调
    {
        if (spellInfo->Id == SPELL_INCITE_CHAOS)
            HandleInciteStart();
        else if (spellInfo->Id == SPELL_CHARGE)
            DoResetThreat();
    }

    void HandleInciteStart()                                     // 开启煽动混乱：喊话、定身首领并启动周期性狂笑
    {
        DoBroadcastText(SAY_INCITE, m_creature);
        SetCombatScriptStatus(true);
        SetCombatMovement(false);
        SetMeleeEnabled(false);
        m_creature->CastSpell(nullptr, SPELL_LAUGH_PERIODIC, TRIGGERED_NONE);
        ResetTimer(BLACKHEART_INCITE_TIMER, 19000);
    }

    void HandleInciteEnd()                                       // 混乱结束：清空仇恨并恢复近战攻击
    {
        DoResetThreat();
        SetCombatMovement(true);
        SetMeleeEnabled(true);
        SetCombatScriptStatus(false);
        m_creature->SetInCombatWithZone(false);
        AttackClosestEnemy();
    }
};

// 煽动混乱心控触发器 AI
struct dummy_blackheart_the_inciterAI : public ScriptedAI
{
    dummy_blackheart_the_inciterAI(Creature* creature) : ScriptedAI(creature), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_threat(false),
        m_attack(false)
    {
        SetCombatMovement(false);
    }

    ScriptedInstance* m_instance;                               // 副本数据管理指针
    bool m_threat;                                              // 是否已建立初始仇恨
    bool m_attack;                                              // 是否已开始命令被心控玩家攻击

    void Reset() override
    {
    }

    void JustRespawned() override                               // 触发器生成回调：向目标玩家施加心控
    {
        if (m_instance)
            if (Creature* blackheartBoss = m_instance->GetSingleCreatureFromStorage(NPC_BLACKHEART_THE_INCITER))
                m_creature->CastSpell(blackheartBoss, SPELL_THREAT, TRIGGERED_NONE);
        m_creature->CastSpell(m_creature->GetSpawner(), SPELL_INCITE_CHAOS_CHARM, TRIGGERED_NONE);

        DoResetThreat();
    }

    void SpellHitTarget(Unit* target, const SpellEntry* spellInfo) override // 法术命中：将被心控目标间仇恨拉满
    {
        if (spellInfo->Id == SPELL_INCITER_THREAT_TRIGGER)
        {
            Unit* myCharm = m_creature->GetCharm();
            Unit* hisCharm = target->GetCharm();
            if (myCharm && hisCharm)
                myCharm->AddThreat(hisCharm);
        }
    }

    void UpdateAI(const uint32 /*diff*/) override               // 步进更新心控攻击逻辑
    {
        if (!m_threat)
        {
            m_threat = true;
            m_creature->CastSpell(nullptr, SPELL_INCITER_THREAT_TRIGGER, TRIGGERED_NONE);
        }
        else if (!m_attack)
        {
            m_attack = true;
            if (Unit* charmed = m_creature->GetCharm())
                charmed->AI()->AttackClosestEnemy();
        }

        if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_DUMMY_NUKE, SELECT_FLAG_PLAYER))
            m_creature->CastSpell(target, SPELL_DUMMY_NUKE, TRIGGERED_NONE);
    }
};

// 煽动混乱法术脚本：为每位玩家分配不同阵营的心控触发器
struct InciteChaos : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* unitTarget = spell->GetUnitTarget();
        if (!unitTarget || unitTarget->GetTypeId() != TYPEID_PLAYER)
            return;

        uint32 spellId;

        switch (spell->GetScriptValue())
        {
            case 0: spellId = SPELL_INCITE_CHAOS_SPAWN_1; break;
            case 1: spellId = SPELL_INCITE_CHAOS_SPAWN_2; break;
            case 2: spellId = SPELL_INCITE_CHAOS_SPAWN_3; break;
            case 3: spellId = SPELL_INCITE_CHAOS_SPAWN_4; break;
            case 4: spellId = SPELL_INCITE_CHAOS_SPAWN_5; break;
        }

        spell->SetScriptValue(spell->GetScriptValue() + 1);

        unitTarget->CastSpell(nullptr, spellId, TRIGGERED_OLD_TRIGGERED); // shouldnt be sent to client (触发器召唤法术)
    }
};

// 注册煽动者布莱卡特及心控触发器脚本
void AddSC_boss_blackheart_the_inciter()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_blackheart_the_inciter";
    pNewScript->GetAI = &GetNewAIInstance<boss_blackheart_the_inciterAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "dummy_blackheart_the_inciter";
    pNewScript->GetAI = &GetNewAIInstance<dummy_blackheart_the_inciterAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<InciteChaos>("spell_incite_chaos");
}
