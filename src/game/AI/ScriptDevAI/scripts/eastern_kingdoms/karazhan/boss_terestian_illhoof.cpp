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
SDName: Boss_Terestian_Illhoof
SD%Complete: 100
SDComment:
ILLHOOF_ACTION_SUMMON_KILREK 45sec post 2.1, else 30
ILLHOOF_ACTION_SACRIFICE random topaggro + 1 post 2.1. else 0
SDCategory: Karazhan
EndScriptData */

// 特雷斯坦·邪蹄（Terestian Illhoof）首领战 AI（恶魔之链束缚献祭与小鬼狂潮）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "karazhan.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_SLAY1                   = 15145,                    // slay1 台词广播 ID
    SAY_SLAY2                   = 15146,                    // slay2 台词广播 ID
    SAY_DEATH                   = 15137,                    // 死亡 台词广播 ID
    SAY_AGGRO                   = 15141,                    // 开战 台词广播 ID
    SAY_SACRIFICE1              = 15147,                    // sacrifice1 台词广播 ID
    SAY_SACRIFICE2              = 15148,                    // sacrifice2 台词广播 ID
    SAY_SUMMON1                 = 15150,                    // summon1 台词广播 ID
    SAY_SUMMON2                 = 15151,                    // summon2 台词广播 ID
    SAY_KILREK_DEATH            = 13582,                    // kilrek死亡 台词广播 ID

    // spells (技能法术)
    SPELL_SUMMON_DEMONCHAINS    = 30120,                    // Summons demonic chains that maintain the ritual of sacrifice. (召唤 demonic chains that maintain the ritual of sacrifice.)
    SPELL_SHADOW_BOLT           = 30055,                    // Hurls a bolt of dark magic at an enemy, inflicting Shadow damage.
    SPELL_SACRIFICE             = 30115,                    // Teleports and adds the debuff
    SPELL_BERSERK               = 32965,                    // Increases attack speed by 75%. Periodically casts Shadow Bolt Volley.
    SPELL_SUMMON_IMP            = 30066,                    // Summons Kil'rek (召唤 Kil'rek)
    SPELL_FIENDISH_PORTAL       = 30171,                    // Opens portal and summons Fiendish Portal, 2 sec cast
    SPELL_FIENDISH_PORTAL_1     = 30179,                    // Opens portal and summons Fiendish Portal, instant cast

    // Chains spells (锁链spells)
    SPELL_DEMON_CHAINS          = 30206,                    // Instant - Visual Effect (Instant-视觉特效效果)

    // Portal spells (传送门spells)
    SPELL_SUMMON_FIENDISH_IMP   = 30184,                    // 召唤fiendishimp 法术 ID

    // Kilrek
    SPELL_BROKEN_PACT           = 30065,                    // All damage taken increased by 25%, TODO: Sends AI event for Imp Timer

    // summoned npcs
    NPC_DEMONCHAINS             = 17248,                    // demonchains Entry ID
    NPC_FIENDISHIMP             = 17267,                    // fiendishimp Entry ID
    NPC_PORTAL                  = 17265,                    // 传送门 Entry ID
    NPC_KILREK                  = 17229                     // kilrek Entry ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum IllhoofActions
{
    ILLHOOF_ACTION_SUMMON_KILREK,
    ILLHOOF_ACTION_SUMMON,
    ILLHOOF_ACTION_BERSERK,
    ILLHOOF_ACTION_MAX,
};

// 首领战 AI：标识 ID（terestian）
struct boss_terestianAI : public CombatAI
{
    boss_terestianAI(Creature* creature) : CombatAI(creature, ILLHOOF_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        AddCustomAction(ILLHOOF_ACTION_BERSERK, uint32(10 * MINUTE * IN_MILLISECONDS), [&]() { HandleBerserk(); }, TIMER_COMBAT_COMBAT);
        AddCustomAction(ILLHOOF_ACTION_SUMMON_KILREK, true, [&]() { HandleSummonKilrek(); });
        AddCustomAction(ILLHOOF_ACTION_SUMMON, 10000u, [&]() { HandleSummonPortal(); }, TIMER_COMBAT_COMBAT);
        AddOnKillText(SAY_SLAY1, SAY_SLAY2);
    }

    ScriptedInstance* m_instance;

    ObjectGuid m_kilrekGuid;                                // kilrek 实体 GUID
    ObjectGuid m_sacrificeGuid;                             // sacrifice 实体 GUID
    ObjectGuid m_chainsGuid;                                // chains 实体 GUID
    
    bool m_bSummonedPortals;                                // SummonedPortals 状态标记

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();

        m_bSummonedPortals = false;
    }

    uint32 GetSubsequentActionTimer(uint32 id)
    {
        switch (id)
        {
#ifdef PRENERF_2_0_3
            case ILLHOOF_ACTION_SUMMON_KILREK: return 30000;
#else
            case ILLHOOF_ACTION_SUMMON_KILREK: return 45000;
#endif                                                      // 结束头文件防重复包含宏
            default: return 0; // never occurs but for compiler (仅供编译器检查，实际不触发)
        }
    }

     // 施法事件回调
     void OnSpellCast(SpellEntry const* spellInfo, Unit* target) override
    {
        switch (spellInfo->Id)
        {
            case SPELL_SACRIFICE:
                DoCastSpellIfCan(m_creature, SPELL_SUMMON_DEMONCHAINS, CAST_TRIGGERED);
                DoBroadcastText(urand(0, 1) ? SAY_SACRIFICE1 : SAY_SACRIFICE2, m_creature);
                m_sacrificeGuid = target->GetObjectGuid();
                break;
        }
    }

    void HandleSummonPortal()
    {
        if (DoCastSpellIfCan(nullptr, SPELL_FIENDISH_PORTAL) == CAST_OK)
            DoBroadcastText(urand(0, 1) ? SAY_SUMMON1 : SAY_SUMMON2, m_creature);
        DisableCombatAction(ILLHOOF_ACTION_SUMMON);
    }

    void HandleSummonKilrek()
    {
        DoCastSpellIfCan(m_creature, SPELL_SUMMON_IMP);
        DisableCombatAction(ILLHOOF_ACTION_SUMMON_KILREK);
    }

    void HandleBerserk()
    {
        DoCastSpellIfCan(nullptr, SPELL_BERSERK);
        DisableCombatAction(ILLHOOF_ACTION_BERSERK);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoBroadcastText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_TERESTIAN, IN_PROGRESS);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        Creature* kilrek = m_creature->GetMap()->GetAnyTypeCreature(m_kilrekGuid);
        if (!kilrek || !kilrek->IsAlive())
            DoCastSpellIfCan(m_creature, SPELL_SUMMON_IMP);

        if (m_instance)
            m_instance->SetData(TYPE_TERESTIAN, FAIL);
    }

    void JustRespawned() override
    {
        DoCastSpellIfCan(m_creature, SPELL_SUMMON_IMP);
        Reset();
    }
    
    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        switch (summoned->GetEntry())
        {
            case NPC_PORTAL:
                if (m_creature->IsInCombat())
                    summoned->SetInCombatWithZone();
                if (!m_bSummonedPortals)
                {
                    m_bSummonedPortals = true;
                    DoCastSpellIfCan(m_creature, SPELL_FIENDISH_PORTAL_1, CAST_TRIGGERED);
                }
                break;
            case NPC_KILREK:
                m_kilrekGuid = summoned->GetObjectGuid();
                if (m_creature->IsInCombat())
                {
                    summoned->SetInCombatWithZone();
                    summoned->AI()->AttackClosestEnemy();
                }
                m_creature->RemoveAurasDueToSpell(SPELL_BROKEN_PACT);
                break;
            case NPC_DEMONCHAINS:
                m_chainsGuid = summoned->GetObjectGuid();
                summoned->AI()->SetReactState(REACT_PASSIVE);
                summoned->CastSpell(summoned, SPELL_DEMON_CHAINS, TRIGGERED_NONE);
                break;
        }
    }

    // 召唤生物死亡回调
    void SummonedCreatureJustDied(Creature* summoned) override
    {
        switch (summoned->GetEntry())
        {
            case NPC_KILREK:
                DoBroadcastText(SAY_KILREK_DEATH, summoned, summoned);
                summoned->CastSpell(m_creature, SPELL_BROKEN_PACT, TRIGGERED_OLD_TRIGGERED);
                ResetTimer(ILLHOOF_ACTION_SUMMON_KILREK, GetSubsequentActionTimer(ILLHOOF_ACTION_SUMMON_KILREK));
                break;
            case NPC_DEMONCHAINS:
                if (Player* pPlayer = m_creature->GetMap()->GetPlayer(m_sacrificeGuid))
                    pPlayer->RemoveAurasDueToSpell(SPELL_SACRIFICE);
                break;
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_TERESTIAN, DONE);
    }
};

struct Sacrifice : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply)
        {
            if (Unit* caster = aura->GetCaster())
            {
                if (boss_terestianAI* ai = dynamic_cast<boss_terestianAI*>(caster->AI()))
                {
                    if (Creature* chains = caster->GetMap()->GetCreature(ai->m_chainsGuid))
                    {
                        chains->ForcedDespawn();
                    }
                }
            }
        }
    }
};

// 注册并载入脚本模块：boss_terestian_illhoof
void AddSC_boss_terestian_illhoof()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_terestian_illhoof";
    pNewScript->GetAI = &GetNewAIInstance<boss_terestianAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<Sacrifice>("spell_sacrifice");
}
