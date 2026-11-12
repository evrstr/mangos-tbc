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
SDName: Boss_Akilzon
SD%Complete: 80
SDComment: Timers; Some details may need adjustments.
SDCategory: Zul'Aman
EndScriptData */

// 埃基尔松（Akil'zon）鹰之化身首领战 AI（电气风暴全团集合躲避避雷针、召唤掠食猛禽）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "zulaman.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "Spells/Scripts/SpellScript.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_EVENT1              = -1568024,                     // event1 台词广播 ID
    SAY_EVENT2              = -1568025,                     // event2 台词广播 ID
    SAY_AGGRO               = -1568026,                     // 开战 台词广播 ID
    SAY_SUMMON              = -1568027,                     // 召唤 台词广播 ID
    SAY_SUMMON_ALT          = -1568028,                     // 召唤alt 台词广播 ID
    SAY_ENRAGE              = -1568029,                     // 狂暴 台词广播 ID
    SAY_SLAY1               = 23233,                        // slay1 台词广播 ID
    SAY_SLAY2               = 23234,                        // slay2 台词广播 ID
    SAY_DEATH               = -1568032,                     // 死亡 台词广播 ID
    EMOTE_STORM             = -1568033,                     // storm 表情动作 ID

    SPELL_STATIC_DISRUPTION = 43622,                        // staticdisruption 法术 ID
    SPELL_CALL_LIGHTNING    = 43661,                        // 呼唤闪电 法术 ID
    SPELL_GUST_OF_WIND      = 43621,                        // gustofwind 法术 ID
    SPELL_ELECTRICAL_STORM  = 43648,                        // electricalstorm 法术 ID
    SPELL_BERSERK           = 45078,                        // 狂暴 法术 ID

    // spell used by eagles
    SPELL_EAGLE_SWOOP       = 44732,                        // eagleswoop 法术 ID
    SOUND_EAGLE_SWOOP       = 12196,                        // eagleswoop 音效 ID

    NPC_SOARING_EAGLE       = 24858,                        // soaringeagle Entry ID
    MAX_EAGLE_COUNT         = 8,                            // eaglecount 标识 ID

    PATH_ID_EAGLE_1         = 1,                            // IDeagle1 标识 ID
    PATH_ID_EAGLE_2         = 2,                            // IDeagle2 标识 ID
    PATH_ID_EAGLE_3         = 3,                            // IDeagle3 标识 ID
    PATH_ID_EAGLE_4         = 4,                            // IDeagle4 标识 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum AkilzonActions
{
    AKILZON_ACTION_BERSERK,
    AKILZON_ACTION_STATIC_DISRUPT,
    AKILZON_ACTION_CALL_LIGHTNING,
    AKILZON_ACTION_GUST_OF_WIND,
    AKILZON_ACTION_STORM_WEATHER,
    AKILZON_ACTION_STORM,
    AKILZON_ACTION_SUMMON_EAGLE,
    AKILZON_ACTION_MAX,
    AKILZON_WIND_WALL_DELAY,
};

static Position eagleSummonPositions[] =
{
    {355.3323f, 1410.864f, 91.63957f, 5.856256f}, // PATH_ID_EAGLE_1
    {356.3942f, 1419.819f, 91.7244f,  4.567829f},
    {369.0859f, 1378.073f, 91.78802f, 0.4234844f}, // PATH_ID_EAGLE_2
    {362.5441f, 1379.941f, 91.75828f, 5.741418f},
    {396.0498f, 1398.616f, 91.4911f,  4.309737f}, // PATH_ID_EAGLE_3
    {394.6324f, 1394.008f, 91.79617f, 1.772357f},
    {385.948f,  1430.281f, 92.62734f, 3.174874f}, // PATH_ID_EAGLE_4
    {381.914f,  1433.974f, 91.65897f, 6.053235f},
};

// 首领战 AI：标识 ID（akilzon）
struct boss_akilzonAI : public CombatAI
{
    boss_akilzonAI(Creature* creature) : CombatAI(creature, AKILZON_ACTION_MAX), m_instance(static_cast<instance_zulaman*>(creature->GetInstanceData())), m_eagles(MAX_EAGLE_COUNT)
    {
        AddCombatAction(AKILZON_ACTION_BERSERK, uint32(10 * MINUTE * IN_MILLISECONDS));
        AddCombatAction(AKILZON_ACTION_STATIC_DISRUPT, 7000, 14000);
        AddCombatAction(AKILZON_ACTION_CALL_LIGHTNING, 15000, 25000);
        AddCombatAction(AKILZON_ACTION_GUST_OF_WIND, 20000, 30000);
        AddCombatAction(AKILZON_ACTION_STORM_WEATHER, 42000u);
        AddCombatAction(AKILZON_ACTION_STORM, true);
        AddCombatAction(AKILZON_ACTION_SUMMON_EAGLE, 62000u);
        AddCustomAction(AKILZON_WIND_WALL_DELAY, true, [&]()
        {
            if (m_creature->IsInCombat() && !m_creature->GetCombatManager().IsEvadingHome())
                m_instance->DoUseDoorOrButton(GO_WIND_DOOR);
        });
        m_creature->GetCombatManager().SetLeashingCheck([](Unit*, float x, float /*y*/, float /*z*/)
        {
            return x < 336.259f;
        });
        AddOnKillText(SAY_SLAY1, SAY_SLAY2);
    }

    instance_zulaman* m_instance;
    GuidVector m_eagles;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();
        DespawnGuids(m_eagles);
        m_eagles.resize(MAX_EAGLE_COUNT);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_AKILZON, IN_PROGRESS);

        ResetTimer(AKILZON_WIND_WALL_DELAY, 5000);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        if (!m_instance)
            return;

        m_instance->SetData(TYPE_AKILZON, DONE);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_AKILZON, FAIL);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        if (summoned->GetEntry() == NPC_SOARING_EAGLE)
        {
            summoned->SetLevitate(true);
            summoned->SetInCombatWithZone();
        }
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* /*invoker*/, uint32 /*miscValue*/) override
    {
        if (eventType == AI_EVENT_CUSTOM_A && m_instance)
            m_instance->ChangeWeather(false);
    }

    void DoSummonEagles()
    {
        for (uint32 i = 0; i < MAX_EAGLE_COUNT; ++i)
        {
            if (!m_eagles[i].IsEmpty())
                continue;
            Creature* eagle = m_creature->SummonCreature(NPC_SOARING_EAGLE, eagleSummonPositions[i].x, eagleSummonPositions[i].y, eagleSummonPositions[i].z, eagleSummonPositions[i].o, TEMPSPAWN_DEAD_DESPAWN, 0, false, true);
            m_eagles[i] = eagle->GetObjectGuid();
            SendAIEvent(AI_EVENT_CUSTOM_A, eagle, eagle, i);
        }
    }

    // 召唤生物死亡回调
    void SummonedCreatureJustDied(Creature* summoned) override
    {
        if (summoned->GetEntry() == NPC_SOARING_EAGLE)
        {
            for (uint32 i = 0; i < MAX_EAGLE_COUNT; ++i)
            {
                if (m_eagles[i] == summoned->GetObjectGuid())
                {
                    m_eagles[i] = ObjectGuid();
                    if (i % 2 == 0)
                    {
                        if (!m_eagles[i + 1].IsEmpty())
                            if (Creature * creature = m_creature->GetMap()->GetCreature(m_eagles[i + 1]))
                                SendAIEvent(AI_EVENT_CUSTOM_B, creature, creature, i);
                    }
                    else
                    {
                        if (!m_eagles[i - 1].IsEmpty())
                            if (Creature * creature = m_creature->GetMap()->GetCreature(m_eagles[i - 1]))
                                SendAIEvent(AI_EVENT_CUSTOM_B, creature, creature, i);
                    }
                }
            }
        }
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case AKILZON_ACTION_BERSERK:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_BERSERK) == CAST_OK)
                {
                    DoScriptText(SAY_ENRAGE, m_creature);
                    DisableCombatAction(action);
                }
                break;
            }
            case AKILZON_ACTION_STATIC_DISRUPT:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1, nullptr, SELECT_FLAG_PLAYER))
                {
                    if (DoCastSpellIfCan(target, SPELL_STATIC_DISRUPTION) == CAST_OK)
                        ResetCombatAction(action, urand(7000, 14000));
                }
                break;
            }
            case AKILZON_ACTION_CALL_LIGHTNING:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_TOPAGGRO, 0, nullptr, SELECT_FLAG_PLAYER))
                {
                    if (DoCastSpellIfCan(target, SPELL_CALL_LIGHTNING) == CAST_OK)
                        ResetCombatAction(action, urand(15000, 25000));
                }
                break;
            }
            case AKILZON_ACTION_GUST_OF_WIND:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1, nullptr, SELECT_FLAG_PLAYER))
                {
                    if (DoCastSpellIfCan(target, SPELL_GUST_OF_WIND) == CAST_OK)
                        ResetCombatAction(action, urand(20000, 30000));
                }
                break;
            }
            case AKILZON_ACTION_STORM_WEATHER:
            {
                // change weather 8.5 seconds prior to storm
                if (m_instance)
                    m_instance->ChangeWeather(true);
                ResetCombatAction(action, urand(54000, 60000));
                ResetCombatAction(AKILZON_ACTION_STORM, 8500);
                break;
            }
            case AKILZON_ACTION_STORM:
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                {
                    if (DoCastSpellIfCan(target, SPELL_ELECTRICAL_STORM) == CAST_OK)
                    {
                        DoScriptText(EMOTE_STORM, m_creature);
                        DisableCombatAction(action);
                    }
                }
                break;
            }
            case AKILZON_ACTION_SUMMON_EAGLE:
            {
                DoScriptText(urand(0, 1) ? SAY_SUMMON : SAY_SUMMON_ALT, m_creature);
                DoSummonEagles();
                ResetCombatAction(action, 60000);
                break;
            }
        }
    }
};

// 随从/NPC AI：eagle（soaring_eagle）
struct mob_soaring_eagleAI : public ScriptedAI
{
    mob_soaring_eagleAI(Creature* creature) : ScriptedAI(creature), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        SetReactState(REACT_PASSIVE);
        SetCombatMovement(false);
        SetMeleeEnabled(false);
        Reset();
    }

    ScriptedInstance* m_instance;

    uint32 m_uiEagleSwoopTimer;                             // EagleSwoop 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiEagleSwoopTimer = urand(4000, 12000);
    }

    void JustRespawned() override
    {
        m_creature->SetInCombatWithZone();
    }

    // 移动路径点到达回调
    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (type != EFFECT_MOTION_TYPE || pointId != SPELL_EAGLE_SWOOP)
            return;

        m_uiEagleSwoopTimer = urand(2000, 12000);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* sender, Unit* /*invoker*/, uint32 miscValue) override
    {
        if (eventType == AI_EVENT_CUSTOM_A)
        {
            boss_akilzonAI* ai = static_cast<boss_akilzonAI*>(sender->AI());
            if (miscValue % 2 == 0) // main mob
            {
                m_creature->GetMotionMaster()->Clear(false, true);
                m_creature->GetMotionMaster()->MoveWaypoint((miscValue + 2) / 2);
                if (!ai->m_eagles[miscValue + 1].IsEmpty())
                    if (Creature * creature = m_creature->GetMap()->GetCreature(ai->m_eagles[miscValue + 1]))
                        creature->GetMotionMaster()->MoveFollow(m_creature, 5.f, M_PI_F, true);
            }
            else // follower
            {
                if (Creature * creature = m_creature->GetMap()->GetCreature(ai->m_eagles[miscValue - 1]))
                    m_creature->GetMotionMaster()->MoveFollow(creature, 5.f, M_PI_F, true);
            }
        }
        else if (eventType == AI_EVENT_CUSTOM_B) // if follower - move on WPs
        {
            boss_akilzonAI* ai = static_cast<boss_akilzonAI*>(sender->AI());
            if (miscValue % 2 == 0)
            {
                m_creature->GetMotionMaster()->Clear(false, true);
                m_creature->GetMotionMaster()->MoveWaypoint((miscValue + 2) / 2);
            }
        }
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget())
            return;

        if (m_uiEagleSwoopTimer)
        {
            if (m_uiEagleSwoopTimer <= diff)
            {
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, SPELL_EAGLE_SWOOP) == CAST_OK)
                    {
                        DoPlaySoundToSet(m_creature, SOUND_EAGLE_SWOOP);
                        m_uiEagleSwoopTimer = 0;
                    }
            }
            else
                m_uiEagleSwoopTimer -= diff;
        }
    }
};

// 法术脚本逻辑实现：标识 ID（TeleportSelf）
struct TeleportSelf : public SpellScript
{
    void OnDestTarget(Spell* spell) const override
    {
        spell->m_targets.m_destPos.z += 10.f;
    }
};

// 注册并载入脚本模块：boss_akilzon
void AddSC_boss_akilzon()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_akilzon";
    pNewScript->GetAI = &GetNewAIInstance<boss_akilzonAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "mob_soaring_eagle";
    pNewScript->GetAI = &GetNewAIInstance<mob_soaring_eagleAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<TeleportSelf>("spell_teleport_self_akilzon");
}
