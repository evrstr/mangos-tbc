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
SDName: Boss_Highlord_Kruul
SD%Complete: 50
SDComment: Alot of this info is based on guesswork/hearsay (大领主库鲁尔：燃烧的远征前夕世界入侵首领)
SDCategory: Bosses
EndScriptData
*/

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/TimerAI.h"
#include "World/WorldState.h"

enum
{
    // 台词文本 ID 与技能/生物常量
    SAY_INTRO1                          = -1015073,
    SAY_INTRO2                          = -1015074,
    SAY_AGGRO1                          = -1015075,
    SAY_AGGRO2                          = -1015076,
    SAY_AGGRO3                          = -1015077,
    SAY_KILL                            = -1015078,
    SAY_DESPAWN                         = -1015079,
    EMOTE_FRENZY                        = -1000002,

    SPELL_SHADOW_VOLLEY                 = 21341,    // 暗影箭雨
    SPELL_BERSERK                       = 21340,    // triggers 21341 (狂暴：触发暗影箭雨)
    SPELL_CLEAVE                        = 20691,    // 顺劈斩
    SPELL_THUNDERCLAP                   = 26554,    // 雷霆一击
    SPELL_VOIDBOLT                      = 21066,    // 虚空箭
    SPELL_MARK_OF_KAZZAK                = 21056,    // triggers 21058 when target gets to 0 mana (卡扎克印记：目标法力归零时爆炸)
    SPELL_CAPTURESOUL                   = 21053,    // procs 21054 on kill (Doom Lord Kazzak uses 48473 Capture Soul, procs 32966 on kill) (灵魂吸取：击杀玩家时回血)
    SPELL_TWISTED_REFLECTION            = 21063,    // 扭曲反射

    SPELL_SUMMON_INFERNALING            = 33533,    // summons creature Infernal Hound 19207 (召唤地狱犬)
    SPELL_CREATE_INFERNALING_SUMMONER   = 33536,    // summons gameobjects 183283 Infernaling Summoner Visual and 183282 Infernaling Summoner Damage Trap - triggers spell 29115 Flames (创建地狱犬传送门)
    SPELL_CREATE_INFERNALING_SUMMONER_C = 33541,    // triggered by 33536, summons trap gameobject 183278 Infernaling Summoner (go_infernaling_summoner)
    SPELL_INFERNALING_SPAWN_IN          = 33543,    // 地狱犬生成视觉
    SPELL_INFERNALING_DURATION          = 33544,    // server-side (地狱犬持续时间)
    SPELL_INFERNALING_DESPAWN           = 33545,    // 地狱犬消失
    SPELL_INFERNALING_IMMOLATION        = 19483,    // 地狱犬献祭
    SPELL_KAZZAKS_ASSAULT               = 33512,    // used to insta-kill npcs (卡扎克的突袭：秒杀 NPC)

    NPC_HAND_OF_THE_HIGHLORD            = 19214,
    NPC_HIGHLORD_KRUUL                  = 18338,
    NPC_INFERNAL_HOUND                  = 19207,

    MAX_INFERNAL_HOUNDS                 = 15,
};

// 库鲁尔战斗动作枚举（按优先级排序）
enum KruulActions // order based on priority
{
    KRUUL_ACTION_SUPREME,
    KRUUL_ACTION_VOIDBOLT,
    KRUUL_ACTION_TWISTED_REFLECTION,
    KRUUL_ACTION_MARK_OF_KAZZAK,
    KRUUL_ACTION_SHADOW_BOLT_VOLLEY,
    KRUUL_ACTION_THUNDERCLAP,
    KRUUL_ACTION_CLEAVE,
    KRUUL_ACTION_KAZZAKS_ASSAULT,

    KRUUL_ACTION_MAX,
};


// 大领主库鲁尔 AI (boss_highlord_kruulAI)
// 前夕世界入侵 Boss：高伤害近战、顺劈、雷霆一击、虚空箭、卡扎克印记、扭曲反射与召唤地狱犬，90秒狂暴施放暗影箭雨
struct boss_highlord_kruulAI : public ScriptedAI
{
    boss_highlord_kruulAI(Creature* creature) : ScriptedAI(creature, KRUUL_ACTION_MAX)
    {
        AddCombatAction(KRUUL_ACTION_SUPREME, 0u);
        AddCombatAction(KRUUL_ACTION_VOIDBOLT, 0u);
        AddCombatAction(KRUUL_ACTION_TWISTED_REFLECTION, 0u);
        AddCombatAction(KRUUL_ACTION_MARK_OF_KAZZAK, 0u);
        AddCombatAction(KRUUL_ACTION_SHADOW_BOLT_VOLLEY, 0u);
        AddCombatAction(KRUUL_ACTION_THUNDERCLAP, 0u);
        AddCombatAction(KRUUL_ACTION_CLEAVE, 0u);
        AddCombatAction(KRUUL_ACTION_KAZZAKS_ASSAULT, 0u);

        m_uiCreateInfernalingSummonerTimer = 6 * MINUTE * IN_MILLISECONDS;
        m_uiDespawnTimer = urand(4 * HOUR * IN_MILLISECONDS, 6 * HOUR * IN_MILLISECONDS);

        Reset();
        m_creature->SetActiveObjectState(true);
        SummonFormation();
        DoCastSpellIfCan(m_creature, SPELL_CREATE_INFERNALING_SUMMONER);
    }

    uint32 m_uiCreateInfernalingSummonerTimer;
    uint32 m_uiDespawnTimer;

    // 召唤库鲁尔之手护卫阵型
    void SummonFormation()
    {
        if (Creature* hand1 = m_creature->SummonCreature(NPC_HAND_OF_THE_HIGHLORD, m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), 0.0f, TEMPSPAWN_DEAD_DESPAWN, 1800000, true))
        {
            hand1->GetMotionMaster()->MoveFollow(m_creature, 15.f, 220.f * float(M_PI) / 180.0f, true);
        }
        if (Creature* hand2 = m_creature->SummonCreature(NPC_HAND_OF_THE_HIGHLORD, m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), 0.0f, TEMPSPAWN_DEAD_DESPAWN, 1800000, true))
        {
            hand2->GetMotionMaster()->MoveFollow(m_creature, 15.f, 140.f * float(M_PI) / 180.0f, true);
        }
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() == NPC_INFERNAL_HOUND)
        {
            pSummoned->GetMotionMaster()->MoveRandomAroundPoint(pSummoned->GetPositionX(), pSummoned->GetPositionY(), pSummoned->GetPositionZ(), 25.f);
        }
    }

    // 重置首领状态与所有战斗技能计时器
    void Reset() override
    {
        for (uint32 i = 0; i < KRUUL_ACTION_MAX; ++i)
            SetActionReadyStatus(i, false);

        ResetTimer(KRUUL_ACTION_SUPREME, GetInitialActionTimer(KRUUL_ACTION_SUPREME));
        ResetTimer(KRUUL_ACTION_VOIDBOLT, GetInitialActionTimer(KRUUL_ACTION_VOIDBOLT));
        ResetTimer(KRUUL_ACTION_TWISTED_REFLECTION, GetInitialActionTimer(KRUUL_ACTION_TWISTED_REFLECTION));
        ResetTimer(KRUUL_ACTION_MARK_OF_KAZZAK, GetInitialActionTimer(KRUUL_ACTION_MARK_OF_KAZZAK));
        ResetTimer(KRUUL_ACTION_SHADOW_BOLT_VOLLEY, GetInitialActionTimer(KRUUL_ACTION_SHADOW_BOLT_VOLLEY));
        ResetTimer(KRUUL_ACTION_THUNDERCLAP, GetInitialActionTimer(KRUUL_ACTION_THUNDERCLAP));
        ResetTimer(KRUUL_ACTION_CLEAVE, GetInitialActionTimer(KRUUL_ACTION_CLEAVE));
        ResetTimer(KRUUL_ACTION_KAZZAKS_ASSAULT, GetInitialActionTimer(KRUUL_ACTION_KAZZAKS_ASSAULT));

        SetCombatMovement(true);
        SetCombatScriptStatus(false);
    }

    // 获取技能初始冷却时间
    uint32 GetInitialActionTimer(const uint32 action) const
    {
        switch (action)
        {
            case KRUUL_ACTION_SUPREME: return 90000;
            case KRUUL_ACTION_VOIDBOLT: return 30000;
            case KRUUL_ACTION_TWISTED_REFLECTION: return 33000;
            case KRUUL_ACTION_MARK_OF_KAZZAK: return 25000;
            case KRUUL_ACTION_SHADOW_BOLT_VOLLEY: return urand(3000, 12000);
            case KRUUL_ACTION_THUNDERCLAP: return urand(16000, 20000);
            case KRUUL_ACTION_CLEAVE: return 7000;
            case KRUUL_ACTION_KAZZAKS_ASSAULT: return urand(10000, 18000);
            default: return 0; // never occurs but for compiler
        }
    }

    // 获取技能后续冷却时间
    uint32 GetSubsequentActionTimer(const uint32 action) const
    {
        switch (action)
        {
            case KRUUL_ACTION_SUPREME: return 0;
            case KRUUL_ACTION_VOIDBOLT: return urand(15000, 28000);
            case KRUUL_ACTION_TWISTED_REFLECTION: return 15000;
            case KRUUL_ACTION_MARK_OF_KAZZAK: return  20000;
            case KRUUL_ACTION_SHADOW_BOLT_VOLLEY: return urand(5000, 30000);
            case KRUUL_ACTION_THUNDERCLAP: return urand(10000, 14000);
            case KRUUL_ACTION_CLEAVE: return urand(8000, 12000);
            case KRUUL_ACTION_KAZZAKS_ASSAULT: return urand(20000, 30000);
            default: return 0; // never occurs but for compiler
        }
    }

    // 死亡回调：通知世界状态库鲁尔被击杀
    void JustDied(Unit* /*killer*/) override
    {
        sWorldState.HandleExternalEvent(CUSTOM_EVENT_HIGHLORD_KRUUL_DIED, 0);
    }

    // 重生回调：随机喊话
    void JustRespawned() override
    {
        DoScriptText(urand(0, 1) ? SAY_INTRO1 : SAY_INTRO2, m_creature);
    }

    // 进入战斗回调：附加灵魂吸取光环并喊话
    void Aggro(Unit* /*pWho*/) override
    {
        DoCastSpellIfCan(m_creature, SPELL_CAPTURESOUL, CAST_TRIGGERED);

        switch (urand(0, 3))
        {
            case 0: DoScriptText(SAY_AGGRO1, m_creature); break;
            case 1: DoScriptText(SAY_AGGRO2, m_creature); break;
            case 2: DoScriptText(SAY_AGGRO3, m_creature); break;
        }
    }

    // 击杀目标回调：击杀玩家时播放嘲讽喊话
    void KilledUnit(Unit* pVictim) override
    {
        if (pVictim->GetTypeId() != TYPEID_PLAYER)
            return;
        
        DoScriptText(SAY_KILL, m_creature, pVictim);
    }

    // 战斗技能执行循环
    void ExecuteActions() override
    {
        if (!CanExecuteCombatAction())
            return;

        for (uint32 i = 0; i < KRUUL_ACTION_MAX; ++i)
        {
            if (GetActionReadyStatus(i))
            {
                switch (i)
                {
                    case KRUUL_ACTION_SUPREME:
                    {
                        // Enrage - cast shadowbolt volley every second
                        if (DoCastSpellIfCan(m_creature, SPELL_BERSERK) == CAST_OK)
                        {
                            DoScriptText(EMOTE_FRENZY, m_creature);
                            DisableCombatAction(KRUUL_ACTION_SUPREME);
                            return;
                        }
                        break;
                    }
                    case KRUUL_ACTION_VOIDBOLT:
                    {
                        if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_VOIDBOLT) == CAST_OK)
                        {
                            ResetTimer(i, GetSubsequentActionTimer(i));
                            SetActionReadyStatus(i, false);
                            return;
                        }
                        break;
                    }
                    case KRUUL_ACTION_TWISTED_REFLECTION:
                    {
                        if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1, SPELL_TWISTED_REFLECTION, SELECT_FLAG_PLAYER))
                        {
                            if (DoCastSpellIfCan(pTarget, SPELL_TWISTED_REFLECTION) == CAST_OK)
                            {
                                ResetTimer(i, GetSubsequentActionTimer(i));
                                SetActionReadyStatus(i, false);
                                return;
                            }
                        }
                        break;
                    }
                    case KRUUL_ACTION_MARK_OF_KAZZAK:
                    {
                        if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_MARK_OF_KAZZAK, SELECT_FLAG_PLAYER | SELECT_FLAG_POWER_MANA))
                        {
                            if (DoCastSpellIfCan(pTarget, SPELL_MARK_OF_KAZZAK) == CAST_OK)
                            {
                                ResetTimer(i, GetSubsequentActionTimer(i));
                                SetActionReadyStatus(i, false);
                                return;
                            }
                            break;
                        }
                    }
                    case KRUUL_ACTION_SHADOW_BOLT_VOLLEY:
                    {
                        // Cast shadowbolt volley on timer before Berserk
                        if (DoCastSpellIfCan(m_creature, SPELL_SHADOW_VOLLEY, SELECT_FLAG_USE_EFFECT_RADIUS) == CAST_OK)
                        {
                            ResetTimer(i, GetSubsequentActionTimer(i));
                            SetActionReadyStatus(i, false);
                            return;
                        }
                        break;
                    }
                    case KRUUL_ACTION_THUNDERCLAP:
                    {
                        if (DoCastSpellIfCan(nullptr, SPELL_THUNDERCLAP, SELECT_FLAG_USE_EFFECT_RADIUS) == CAST_OK)
                        {
                            ResetTimer(i, GetSubsequentActionTimer(i));
                            SetActionReadyStatus(i, false);
                            return;
                        }
                        break;
                    }
                    case KRUUL_ACTION_CLEAVE:
                    {
                        if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_CLEAVE) == CAST_OK)
                        {
                            ResetTimer(i, GetSubsequentActionTimer(i));
                            SetActionReadyStatus(i, false);
                            return;
                        }
                        break;
                    }
                    case KRUUL_ACTION_KAZZAKS_ASSAULT:
                    {
                        if (m_creature->GetVictim()->GetTypeId() != TYPEID_PLAYER)
                        {
                            if (DoCastSpellIfCan(m_creature, SPELL_KAZZAKS_ASSAULT) == CAST_OK)
                            {
                                ResetTimer(i, GetSubsequentActionTimer(i));
                                SetActionReadyStatus(i, false);
                                return;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    // AI 每帧更新：处理计时器、门召唤、超时消失及技能施放
    void UpdateAI(const uint32 diff) override
    {
        UpdateTimers(diff, m_creature->IsInCombat());

        if (m_uiCreateInfernalingSummonerTimer)
        {
            if (m_uiCreateInfernalingSummonerTimer <= diff)
            {
                DoCastSpellIfCan(m_creature, SPELL_CREATE_INFERNALING_SUMMONER);
                m_uiCreateInfernalingSummonerTimer = 6 * MINUTE * IN_MILLISECONDS;
            }
            else
                m_uiCreateInfernalingSummonerTimer -= diff;
        }

        if (m_uiDespawnTimer)
        {
            if (m_uiDespawnTimer <= diff)
            {
                if (m_creature->IsAlive())
                {
                    DoScriptText(SAY_DESPAWN, m_creature);
                    sWorldState.HandleExternalEvent(CUSTOM_EVENT_HIGHLORD_KRUUL_DIED, 0);
                    m_creature->ForcedDespawn(5000);
                }
            }
            else
                m_uiDespawnTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        ExecuteActions();
        DoMeleeAttackIfReady();
    }
};

UnitAI* GetAI_boss_highlord_kruul(Creature* pCreature)
{
    return new boss_highlord_kruulAI(pCreature);
}

// 地狱犬 AI (npc_infernal_houndAI)
struct npc_infernal_houndAI : public ScriptedAI
{
    npc_infernal_houndAI(Creature* creature) : ScriptedAI(creature)
    {
        Reset();
        DoCastSpellIfCan(m_creature, SPELL_INFERNALING_SPAWN_IN);
    }

    // 重置首领状态与所有战斗技能计时器
    void Reset() override
    {   
        DoCastSpellIfCan(m_creature, SPELL_INFERNALING_IMMOLATION);
    }
};

UnitAI* GetAI_npc_infernal_hound(Creature* pCreature)
{
    return new npc_infernal_houndAI(pCreature);
}

// 地狱犬召唤门游戏对象 AI (go_infernaling_summoner)
struct go_infernaling_summoner : public GameObjectAI
{
    go_infernaling_summoner(GameObject* go) : GameObjectAI(go)
    {
        m_uiSummonTimer = 15000;
    }

    uint32 m_uiSummonTimer;

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_go->IsSpawned())
            return;

        if (m_uiSummonTimer <= uiDiff)
        {
            if (Creature* pKruul = ((ScriptedInstance*)m_go->GetMap()->GetInstanceData())->GetSingleCreatureFromStorage(NPC_HIGHLORD_KRUUL))
            {
                if (m_go->GetMap()->SpawnedCountForEntry(NPC_INFERNAL_HOUND) < MAX_INFERNAL_HOUNDS)
                {
                    pKruul->CastSpell(pKruul, SPELL_SUMMON_INFERNALING, TRIGGERED_OLD_TRIGGERED, nullptr, nullptr, m_go->GetObjectGuid());
                    m_uiSummonTimer = urand(30000, 45000);
                }
            }
        }
        else
            m_uiSummonTimer -= uiDiff;
    }
};

GameObjectAI* GetAI_go_infernaling_summoner(GameObject* go)
{
    return new go_infernaling_summoner(go);
}

// 21056 - Mark of Kazzak
// 卡扎克印记光环脚本：周期检测目标法力，法力归零时造成巨额爆炸伤害
struct MarkOfLordKazzak : public AuraScript
{
    void OnPeriodicTickEnd(Aura* aura) const override
    {
        if (aura->GetTarget()->HasMana())
        {
            if (aura->GetTarget()->GetPower(POWER_MANA) == 0)
            {
                aura->GetTarget()->CastSpell(nullptr, 21058, TRIGGERED_OLD_TRIGGERED);
                aura->GetTarget()->RemoveAurasDueToSpell(aura->GetId());
            }
        }
    }
};

// 注册大领主库鲁尔相关脚本
void AddSC_boss_highlord_kruul()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_highlord_kruul";
    pNewScript->GetAI = &GetAI_boss_highlord_kruul;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_infernal_hound";
    pNewScript->GetAI = &GetAI_npc_infernal_hound;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_infernaling_summoner";
    pNewScript->GetGameObjectAI = &GetAI_go_infernaling_summoner;
    pNewScript->RegisterSelf();

    RegisterSpellScript<MarkOfLordKazzak>("spell_mark_of_lord_kazzak");
}