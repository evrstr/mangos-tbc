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
SDName: Boss_Janalai
SD%Complete: 90
SDComment: The hatchers may need some additional behavior adjustments.
SDCategory: Zul'Aman
EndScriptData */

// 埃加拉什（Jan'alai）龙鹰之化身首领战 AI（召唤龙鹰孵化者快速孵蛋、烈焰炸弹封路大爆炸）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "zulaman.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO                       = -1568000,             // 开战 台词广播 ID
    SAY_FIRE_BOMBS                  = -1568001,             // 火焰bombs 台词广播 ID
    SAY_SUMMON_HATCHER              = -1568002,             // 召唤hatcher 台词广播 ID
    SAY_ALL_EGGS                    = -1568003,             // alleggs 台词广播 ID
    SAY_BERSERK                     = -1568004,             // 狂暴 台词广播 ID
    SAY_SLAY_1                      = 23585,                // 击杀1 台词广播 ID
    SAY_SLAY_2                      = 23586,                // 击杀2 台词广播 ID
    SAY_DEATH                       = -1568007,             // 死亡 台词广播 ID
    SAY_EVENT_STRANGERS             = -1568008,             // 事件strangers 台词广播 ID
    SAY_EVENT_FRIENDS               = -1568009,             // 事件friends 台词广播 ID

    // Jan'alai (埃加拉什)
    SPELL_FLAME_BREATH              = 43140,                // 烈焰吐息 法术 ID
    SPELL_HATCH_ALL_EGGS            = 43144,                // triggers 42493
    SPELL_TELEPORT_TO_CENTER        = 43098,                // 传送tocenter 法术 ID
    SPELL_SUMMON_ALL_PLAYERS        = 43096,                // triggers 43097
    SPELL_ENRAGE                    = 44779,                // 狂暴 法术 ID
    SPELL_BERSERK                   = 47008,                // 狂暴 法术 ID
    SPELL_SUMMON_HATCHER_1          = 43962,                // 召唤hatcher1 法术 ID
    SPELL_SUMMON_HATCHER_2          = 45340,                // 召唤hatcher2 法术 ID

    // Fire Bob Spells
    SPELL_FIRE_BOMB_CHANNEL         = 42621,                // 火焰炸弹channel 法术 ID
    SPELL_FIRE_BOMB_THROW           = 42628,                // triggers 42629
    SPELL_FIRE_BOMB_EXPLODE         = 42631,                // triggers 42630

    // NPCs
    NPC_FIRE_BOMB                   = 23920,                // 火焰炸弹 Entry ID
    NPC_AMANI_HATCHER_1             = 23818,                // amanihatcher1 Entry ID
    NPC_AMANI_HATCHER_2             = 24504,                // amanihatcher2 Entry ID
    // NPC_HATCHLING                   = 23598,
    // NPC_DRAGONHAWK_EGG              = 23817,
    // NPC_WORLD_TRIGGER_NOT_IMMUNE_PC = 21252,                // used for fire wall

    // Hatcher Spells
    SPELL_HATCH_EGG_1               = 43734,                // hatch龙蛋1 法术 ID
    SPELL_HATCH_EGG_2               = 42471,                // hatch龙蛋2 法术 ID

    // Fire Wall (火焰Wall)
    SPELL_FIRE_WALL                 = 43113,                // 火焰wall 法术 ID

    // Eggs spells
    SPELL_SUMMON_DRAGONHAWK         = 42493,                // 召唤dragonhawk 法术 ID

    MAX_EGGS_ON_SIDE                = 20,                   // there are 20 eggs spawned on each side

    PATH_ID_RIGHT                   = 1,                    // IDright 标识 ID
    PATH_ID_LEFT                    = 2,                    // IDleft 标识 ID
    PATH_ID_RIGHT_SHORT             = 3,                    // IDrightshort 标识 ID
    PATH_ID_LEFT_SHORT              = 4,                    // IDleftshort 标识 ID

    RIGHT_LAST_POINT                = 3,                    // last路径点 标识 ID
    LEFT_LAST_POINT                 = 3,                    // last路径点 标识 ID
    SHORT_LAST_POINT                = 1,                    // last路径点 标识 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum JanalaiActions
{
    JANALAI_ACTION_BERSERK,
    JANALAI_ACTION_ENRAGE_HP,
    JANALAI_ACTION_ENRAGE_TIMER,
    JANALAI_ACTION_HATCH_ALL_EGGS,
    JANALAI_ACTION_SUMMON_HATCHER,
    JANALAI_ACTION_FIRE_BOMBS,
    JANALAI_ACTION_FIRE_BREATH,
    JANALAI_ACTION_MAX,
    JANALAI_EXPLODE_BOMBS,
};

// 首领战 AI：标识 ID（janalai）
struct boss_janalaiAI : public CombatAI
{
    boss_janalaiAI(Creature* creature) : CombatAI(creature, JANALAI_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddCombatAction(JANALAI_ACTION_BERSERK, uint32(10 * MINUTE * IN_MILLISECONDS));
        AddTimerlessCombatAction(JANALAI_ACTION_ENRAGE_HP, true);
        AddCombatAction(JANALAI_ACTION_ENRAGE_TIMER, uint32(5 * MINUTE * IN_MILLISECONDS));
        AddTimerlessCombatAction(JANALAI_ACTION_HATCH_ALL_EGGS, true);
        AddCombatAction(JANALAI_ACTION_SUMMON_HATCHER, 10000u);
        AddCombatAction(JANALAI_ACTION_FIRE_BOMBS, 55000u);
        AddCombatAction(JANALAI_ACTION_FIRE_BREATH, 8000u);
        AddCustomAction(JANALAI_EXPLODE_BOMBS, false, [&]()
        {
            Creature* bomb = GetClosestCreatureWithEntry(m_creature, NPC_FIRE_BOMB, 30.f);
            if (bomb)
                bomb->CastSpell(nullptr, SPELL_FIRE_BOMB_EXPLODE, TRIGGERED_NONE);
        });
        m_creature->GetCombatManager().SetLeashingCheck([](Unit*, float x, float /*y*/, float /*z*/)
        {
            return x > -8.f || x < -57.f;
        });
        AddOnKillText(SAY_SLAY_1, SAY_SLAY_2);
        Reset();
    }

    ScriptedInstance* m_instance;

    uint8 m_uiEggsHatchedLeft;
    uint8 m_uiEggsHatchedRight;

    bool m_bIsFlameWall;                                    // IsFlameWall 状态标记
    bool m_bHasHatchedEggs;                                 // HasHatchedEggs 状态标记
    bool m_bIsEnraged;                                      // IsEnraged 状态标记

    ObjectGuid m_hatcherOneGuid;                            // hatcherOne 实体 GUID
    ObjectGuid m_hatcherTwoGuid;                            // hatcherTwo 实体 GUID

    GuidVector m_summons;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();

        m_uiEggsHatchedLeft = 0;
        m_uiEggsHatchedRight = 0;

        m_bHasHatchedEggs   = false;
        m_bIsEnraged        = false;
        m_bIsFlameWall      = false;

        DespawnGuids(m_summons);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_JANALAI, FAIL);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_JANALAI, DONE);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_JANALAI, IN_PROGRESS);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override
    {
        if (eventType == AI_EVENT_CUSTOM_A)
        {
            m_creature->RemoveAurasDueToSpell(SPELL_FIRE_BOMB_CHANNEL);
            ResetTimer(JANALAI_EXPLODE_BOMBS, 3500);
        }
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        switch (summoned->GetEntry())
        {
            case NPC_AMANI_HATCHER_1:
                m_hatcherOneGuid = summoned->GetObjectGuid();
                // If all the eggs from one side are hatched, move to the other side
                if (m_uiEggsHatchedRight == MAX_EGGS_ON_SIDE)
                    summoned->GetMotionMaster()->MoveWaypoint(PATH_ID_LEFT);
                else
                    summoned->GetMotionMaster()->MoveWaypoint(PATH_ID_RIGHT);
                break;
            case NPC_AMANI_HATCHER_2:
                m_hatcherTwoGuid = summoned->GetObjectGuid();
                // If all the eggs from one side are hatched, move to the other side
                if (m_uiEggsHatchedLeft == MAX_EGGS_ON_SIDE)
                    summoned->GetMotionMaster()->MoveWaypoint(PATH_ID_RIGHT);
                else
                    summoned->GetMotionMaster()->MoveWaypoint(PATH_ID_LEFT);
                break;
            case NPC_FIRE_BOMB:
                summoned->AI()->SetCombatMovement(false);
                m_creature->CastSpell(summoned, SPELL_FIRE_BOMB_THROW, TRIGGERED_OLD_TRIGGERED);
                m_summons.push_back(summoned->GetObjectGuid());
                break;
            case NPC_HATCHLING: // forwarded from hatcher
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    summoned->AI()->AttackStart(target);
                summoned->SetInCombatWithZone();
                // Count the Hatched eggs
                summoned->GetPositionY() > 1100.0f ? ++m_uiEggsHatchedLeft : ++m_uiEggsHatchedRight;
                // Notify the script when all the eggs were hatched
                if (m_uiEggsHatchedRight == MAX_EGGS_ON_SIDE && m_uiEggsHatchedLeft == MAX_EGGS_ON_SIDE)
                {
                    DisableCombatAction(JANALAI_ACTION_SUMMON_HATCHER);
                    SetActionReadyStatus(JANALAI_ACTION_HATCH_ALL_EGGS, false);
                    if (Creature* hatcher = m_creature->GetMap()->GetCreature(m_hatcherOneGuid))
                        hatcher->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, m_creature, hatcher);
                    if (Creature* hatcher = m_creature->GetMap()->GetCreature(m_hatcherTwoGuid))
                        hatcher->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, m_creature, hatcher);
                }
                // Change the side of the hatcher if necessary
                if (m_uiEggsHatchedRight == MAX_EGGS_ON_SIDE && m_uiEggsHatchedLeft < MAX_EGGS_ON_SIDE)
                {
                    if (Creature* hatcher = m_creature->GetMap()->GetCreature(m_hatcherOneGuid))
                        hatcher->AI()->SendAIEvent(AI_EVENT_CUSTOM_B, m_creature, hatcher, PATH_ID_LEFT_SHORT);
                }
                if (m_uiEggsHatchedLeft == MAX_EGGS_ON_SIDE && m_uiEggsHatchedRight < MAX_EGGS_ON_SIDE)
                {
                    if (Creature* hatcher = m_creature->GetMap()->GetCreature(m_hatcherTwoGuid))
                        hatcher->AI()->SendAIEvent(AI_EVENT_CUSTOM_B, m_creature, hatcher, PATH_ID_RIGHT_SHORT);
                }
                break;
        }
    }

    // Wrapper to create the firewalls during Bomb phase
    void DoCreateFireWall()
    {
        CreatureList triggers;
        GetCreatureListWithEntryInGrid(triggers, m_creature, NPC_WORLD_TRIGGER_NOT_IMMUNE_PC, 60.f);
        for (Creature* creature : triggers)
            creature->CastSpell(nullptr, SPELL_FIRE_WALL, TRIGGERED_OLD_TRIGGERED);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case JANALAI_ACTION_BERSERK:
                if (DoCastSpellIfCan(nullptr, SPELL_BERSERK) == CAST_OK)
                {
                    DoScriptText(SAY_BERSERK, m_creature);
                    DisableCombatAction(action);
                }
                return;
            case JANALAI_ACTION_ENRAGE_HP:
                if (m_creature->GetHealthPercent() > 20.0f)
                    return;
            case JANALAI_ACTION_ENRAGE_TIMER:
                if (DoCastSpellIfCan(nullptr, SPELL_ENRAGE) == CAST_OK)
                {
                    SetActionReadyStatus(JANALAI_ACTION_ENRAGE_HP, false);
                    DisableCombatAction(JANALAI_ACTION_ENRAGE_TIMER);
                }
                return;
            case JANALAI_ACTION_HATCH_ALL_EGGS:
                if (m_creature->GetHealthPercent() > 35.0f)
                    return;
                if (DoCastSpellIfCan(nullptr, SPELL_HATCH_ALL_EGGS) == CAST_OK)
                {
                    DoScriptText(SAY_ALL_EGGS, m_creature);
                    SetActionReadyStatus(JANALAI_ACTION_HATCH_ALL_EGGS, false);
                    DisableCombatAction(JANALAI_ACTION_SUMMON_HATCHER);
                }
                return;
            case JANALAI_ACTION_SUMMON_HATCHER:
            {
                DoScriptText(SAY_SUMMON_HATCHER, m_creature);

                Creature* hatcher1 = m_creature->GetMap()->GetCreature(m_hatcherOneGuid);
                Creature* hatcher2 = m_creature->GetMap()->GetCreature(m_hatcherTwoGuid);

                if (!hatcher1 || !hatcher1->IsAlive())
                    DoCastSpellIfCan(nullptr, SPELL_SUMMON_HATCHER_1, CAST_TRIGGERED);

                if (!hatcher2 || !hatcher2->IsAlive())
                    DoCastSpellIfCan(nullptr, SPELL_SUMMON_HATCHER_2, CAST_TRIGGERED);

                ResetCombatAction(action, 90000);
                return;
            }
            case JANALAI_ACTION_FIRE_BOMBS:
                if (DoCastSpellIfCan(nullptr, SPELL_FIRE_BOMB_CHANNEL) == CAST_OK)
                {
                    DoCastSpellIfCan(nullptr, SPELL_TELEPORT_TO_CENTER, CAST_TRIGGERED);
                    DoCastSpellIfCan(nullptr, SPELL_SUMMON_ALL_PLAYERS, CAST_TRIGGERED);
                    DoScriptText(SAY_FIRE_BOMBS, m_creature);
                    DoCreateFireWall();

                    ResetCombatAction(action, 30000);
                }
                return;
            case JANALAI_ACTION_FIRE_BREATH:
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(pTarget, SPELL_FLAME_BREATH) == CAST_OK)
                        ResetCombatAction(action, 8000);
                return;
        }
    }
};

// 随从/NPC AI：hatcher（amanishi_hatcher）
struct npc_amanishi_hatcherAI : public ScriptedAI
{
    npc_amanishi_hatcherAI(Creature* creature) : ScriptedAI(creature), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_hatchlingCount(1)
    {
        SetReactState(REACT_PASSIVE);
        SetCombatScriptStatus(true);
        SetCombatMovement(false);
        Reset();
    }

    ScriptedInstance* m_instance;

    uint32 m_uiWaypoint;
    uint32 m_uiHatchlingTimer;                              // Hatchling 技能计时器
    uint8 m_hatchlingCount;
    bool m_bWaypointEnd;                                    // WaypointEnd 状态标记

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiWaypoint        = 0;
        m_uiHatchlingTimer  = 0;
        m_bWaypointEnd      = false;

        m_creature->SetWalk(false);
    }

    // 移动路径点到达回调
    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (type != WAYPOINT_MOTION_TYPE)
            return;

        switch (m_creature->GetMotionMaster()->GetPathId())
        {
            case PATH_ID_LEFT: if (pointId != LEFT_LAST_POINT) return; break;
            case PATH_ID_RIGHT: if (pointId != RIGHT_LAST_POINT) return; break;
            case PATH_ID_LEFT_SHORT:
            case PATH_ID_RIGHT_SHORT: break;
        }

		m_creature->GetMotionMaster()->Clear(false, true);
		m_creature->GetMotionMaster()->MoveIdle();
        m_uiHatchlingTimer = 1000;
        m_bWaypointEnd = true;
    }

    void SpellHitTarget(Unit* target, SpellEntry const* spell) override
    {
        if ((spell->Id != SPELL_HATCH_EGG_1 && spell->Id != SPELL_HATCH_EGG_2) || target->GetEntry() != NPC_DRAGONHAWK_EGG)
            return;

        if (!m_instance)
            return;

        target->CastSpell(nullptr, SPELL_SUMMON_DRAGONHAWK, TRIGGERED_OLD_TRIGGERED);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* invoker, uint32 miscValue) override
    {
        if (eventType == AI_EVENT_CUSTOM_A)
        {
            SetReactState(REACT_AGGRESSIVE);
            SetCombatScriptStatus(false);
            SetCombatMovement(true);
            if (Unit* target = static_cast<Creature*>(invoker)->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                AttackStart(target);
            m_creature->SetInCombatWithZone();
            m_uiHatchlingTimer = 0;
        }
        else if (eventType == AI_EVENT_CUSTOM_B)
        {
            m_bWaypointEnd = false;
            m_creature->GetMotionMaster()->Clear(false, true);
            m_creature->GetMotionMaster()->MoveWaypoint(miscValue);
        }
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 diff) override
    {
        if (m_bWaypointEnd && m_uiHatchlingTimer)
        {
            if (m_uiHatchlingTimer <= diff)
            {
                // Note: there are 2 Hatch Eggs spells. Not sure which one to use
                if (DoCastSpellIfCan(nullptr, SPELL_HATCH_EGG_2) == CAST_OK)
                {
                    m_uiHatchlingTimer = 4500;
                    if (m_hatchlingCount < 5)
                        ++m_hatchlingCount;
                }
            }
            else
                m_uiHatchlingTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget())
            return;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 法术脚本逻辑实现：标识 ID（HatchEggs）
struct HatchEggs : public SpellScript
{
    void OnInit(Spell* spell) const override
    {
        if (npc_amanishi_hatcherAI* ai = dynamic_cast<npc_amanishi_hatcherAI*>(static_cast<Unit*>(spell->GetCaster())->AI()))
            spell->SetMaxAffectedTargets(ai->m_hatchlingCount);
        else
            spell->SetMaxAffectedTargets(1);
    }
};

// 注册并载入脚本模块：boss_janalai
void AddSC_boss_janalai()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_janalai";
    pNewScript->GetAI = &GetNewAIInstance<boss_janalaiAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_amanishi_hatcher";
    pNewScript->GetAI = &GetNewAIInstance<npc_amanishi_hatcherAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<HatchEggs>("spell_hatch_eggs");
}
