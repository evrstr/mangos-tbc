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
SDName: Isle_of_Queldanas
SD%Complete: 100
SDComment: Quest support: 11524, 11525
SDCategory: Isle Of Quel'Danas
EndScriptData */

// 奎尔丹纳斯岛（Isle of Quel'Danas）破碎残阳日常进攻剧情与事件脚本

/* ContentData
npc_converted_sentry
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

/*######
## npc_converted_sentry
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_CONVERTED_1             = -1000188,                 // converted1 台词广播 ID
    SAY_CONVERTED_2             = -1000189,                 // converted2 台词广播 ID

    SPELL_CONVERT_CREDIT        = 45009,                    // convertcredit 法术 ID
    TIME_PET_DURATION           = 7500                      // petduration 标识 ID
};

// 随从/NPC AI：sentry（converted_sentry）
struct npc_converted_sentryAI : public ScriptedAI
{
    npc_converted_sentryAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 m_uiCreditTimer;                                 // Credit 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_uiCreditTimer = 2500;
    }

    void MoveInLineOfSight(Unit* /*pWho*/) override {}

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        if (m_uiCreditTimer)
        {
            if (m_uiCreditTimer <= uiDiff)
            {
                DoScriptText(urand(0, 1) ? SAY_CONVERTED_1 : SAY_CONVERTED_2, m_creature);

                DoCastSpellIfCan(m_creature, SPELL_CONVERT_CREDIT);
                ((Pet*)m_creature)->SetDuration(TIME_PET_DURATION);
                ((Pet*)m_creature)->GetMotionMaster()->MoveRandomAroundPoint(m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), 15.0f);
                m_uiCreditTimer = 0;
            }
            else
                m_uiCreditTimer -= uiDiff;
        }
    }
};

// 创建并返回 AI 实例工厂函数：npc_converted_sentry
UnitAI* GetAI_npc_converted_sentry(Creature* pCreature)
{
    return new npc_converted_sentryAI(pCreature);
}

/*######
# npc_shattered_sun_fighter
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    ZONEID_ISLE_OF_QUELDANAS                = 4080,         // isleofqueldanas 标识 ID
    NPC_SHATTERED_SUN_WARRIOR               = 25115,        // shatteredsun战士 Entry ID
    NPC_SHATTERED_SUN_MARKSMAN              = 24938,        // shatteredsunmarksman Entry ID
    // relay dbscripts - each of which cast 1 of 4 transformation spells based on current Sun's Reach game event information
    SCRIPT_TRANSFORM_ARCHER_BE_MALE         = 2493801,      // 变形archerbemale 标识 ID
    SCRIPT_TRANSFORM_ARCHER_BE_FEMALE       = 2493802,      // 变形archerbefemale 标识 ID
    SCRIPT_TRANSFORM_ARCHER_DRAENEI_MALE    = 2493803,      // 变形archerdraeneimale 标识 ID
    SCRIPT_TRANSFORM_ARCHER_DRAENEI_FEMALE  = 2493804,      // 变形archerdraeneifemale 标识 ID
    SCRIPT_TRANSFORM_WARRIOR_BE_MALE        = 2511502,      // 变形战士bemale 标识 ID
    SCRIPT_TRANSFORM_WARRIOR_BE_FEMALE      = 2511501,      // 变形战士befemale 标识 ID
    SCRIPT_TRANSFORM_WARRIOR_DRAENEI_MALE   = 2511504,      // 变形战士draeneimale 标识 ID
    SCRIPT_TRANSFORM_WARRIOR_DRAENEI_FEMALE = 2511503,      // 变形战士draeneifemale 标识 ID
};

// 随从/NPC AI：sunfighter（shattered_sun_fighter）
struct npc_shattered_sun_fighterAI : public ScriptedAI
{
    npc_shattered_sun_fighterAI(Creature* creature) : ScriptedAI(creature)
    {
        Reset();
    }

    uint32 m_uiMarksmanRace;

    void JustRespawned() override
    {
        if (m_creature->GetZoneId() == ZONEID_ISLE_OF_QUELDANAS) // let the spawns in Shattrath be handled via movement dbscript
        {
            uint32 transformScriptId = 0;
            if (m_creature->GetEntry() == NPC_SHATTERED_SUN_MARKSMAN)
            {
                SetCombatMovement(false);
                SetMeleeEnabled(false);
                switch (urand(0, 3))
                {
                    case 0:
                        transformScriptId = SCRIPT_TRANSFORM_ARCHER_BE_MALE;
                        m_uiMarksmanRace = RACE_BLOODELF;
                        break;
                    case 1:
                        transformScriptId = SCRIPT_TRANSFORM_ARCHER_BE_FEMALE;
                        m_uiMarksmanRace = RACE_BLOODELF;
                        break;
                    case 2:
                        transformScriptId = SCRIPT_TRANSFORM_ARCHER_DRAENEI_MALE;
                        m_uiMarksmanRace = RACE_DRAENEI;
                        break;
                    case 3:
                        transformScriptId = SCRIPT_TRANSFORM_ARCHER_DRAENEI_FEMALE;
                        m_uiMarksmanRace = RACE_DRAENEI;
                        break;
                }
            }
            else if (m_creature->GetEntry() == NPC_SHATTERED_SUN_WARRIOR)
            {
                switch (urand(0, 3))
                {
                    case 0: transformScriptId = SCRIPT_TRANSFORM_WARRIOR_BE_MALE; break;
                    case 1: transformScriptId = SCRIPT_TRANSFORM_WARRIOR_BE_FEMALE; break;
                    case 2: transformScriptId = SCRIPT_TRANSFORM_WARRIOR_DRAENEI_MALE; break;
                    case 3: transformScriptId = SCRIPT_TRANSFORM_WARRIOR_DRAENEI_FEMALE; break;
                }
            }
            if (transformScriptId)
                m_creature->GetMap()->ScriptsStart(SCRIPT_TYPE_RELAY, transformScriptId, m_creature, m_creature);
        }
    }

    // 重置战斗状态与技能计时器
    void Reset() override {}
    void UpdateAI(const uint32 /*diff*/) override {}
};

// 随从/NPC AI：sun战士（shattered_sun_warrior）
struct npc_shattered_sun_warriorAI : public npc_shattered_sun_fighterAI
{
    npc_shattered_sun_warriorAI(Creature* creature) : npc_shattered_sun_fighterAI(creature) { Reset(); }
    // 重置战斗状态与技能计时器
    void Reset() override {}
    void UpdateAI(const uint32 /*diff*/) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SPELL_SHOOT_BLOODELF_NW = 45219,                        // shootbloodelfnw 法术 ID
    SPELL_SHOOT_DRAENEI_NW  = 45223,                        // shootdraeneinw 法术 ID
    SPELL_SHOOT_BLOODELF_SE = 45229,                        // shootbloodelfse 法术 ID
    SPELL_SHOOT_DRAENEI_SE  = 45233,                        // shootdraeneise 法术 ID
};

// 随从/NPC AI：sunmarksman（shattered_sun_marksman）
struct npc_shattered_sun_marksmanAI : public npc_shattered_sun_fighterAI
{
    npc_shattered_sun_marksmanAI(Creature* creature) : npc_shattered_sun_fighterAI(creature), m_uiShootTimer(0), m_uiShootSpell(0) {}

    uint32 m_uiShootTimer;                                  // Shoot 技能计时器
    uint32 m_uiShootSpell;

    void ReceiveAIEvent(AIEventType eventType, Unit* sender, Unit* /*invoker*/, uint32 /*miscValue*/) override
    {
        if (sender->GetObjectGuid() != m_creature->GetObjectGuid())
            return;
        // Facing North-West
        if (eventType == AI_EVENT_CUSTOM_A)
            m_uiShootSpell = m_uiMarksmanRace == RACE_BLOODELF ? SPELL_SHOOT_BLOODELF_NW : SPELL_SHOOT_DRAENEI_NW;
        // Facing South-East
        else if (eventType == AI_EVENT_CUSTOM_B)
            m_uiShootSpell = m_uiMarksmanRace == RACE_BLOODELF ? SPELL_SHOOT_BLOODELF_SE : SPELL_SHOOT_DRAENEI_SE;
        if (eventType == AI_EVENT_CUSTOM_A || eventType == AI_EVENT_CUSTOM_B)
            m_uiShootTimer = urand(1000, 10000);
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 diff) override
    {
        if (m_uiShootTimer)
        {
            if (m_uiShootTimer <= diff)
            {
                DoCastSpellIfCan(m_creature, m_uiShootSpell);
                m_uiShootTimer = urand(8000, 14000);
            }
            else
                m_uiShootTimer -= diff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 法术脚本逻辑实现：标识 ID（ShatteredSunMarksmanShoot）
struct ShatteredSunMarksmanShoot : public SpellScript
{
    void OnRadiusCalculate(Spell* /*spell*/, SpellEffectIndex /*effIdx*/, bool targetB, float& radius) const override
    {
        if (targetB)
            radius = INTERACTION_DISTANCE;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        if (Unit* caster = spell->GetCaster())
        {
            float angle = caster->GetAngle(spell->m_targets.m_destPos.x, spell->m_targets.m_destPos.y);
            caster->SetFacingTo(angle);
            caster->SetOrientation(angle);
        }
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    NPC_DAWNBLADE_BLOOD_KNIGHT  = 24976,                    // dawnblade鲜血骑士 Entry ID

    SPELL_SEAL_OF_WRATH         = 45095,                    // sealofwrath 法术 ID
    SPELL_JOUST                 = 45105,                    // joust 法术 ID
    SPELL_JUDGEMENT_OF_WRATH    = 45337,                    // judgementofwrath 法术 ID
    SPELL_HOLY_LIGHT            = 13952,                    // 神圣light 法术 ID

    SPELL_FACTION_SPAR          = 45091,                    // factionspar 法术 ID
    SPELL_FACTION_SPAR_BUDDY    = 45092,                    // factionsparbuddy 法术 ID

    AREA_ID_DAWNSTAR_VILLAGE    = 4089,                     // IDdawnstarvillage 标识 ID
    FACTION_SPARRING_DAWNBLADE  = 1965,                     // sparringdawnblade 阵营 ID
    FACTION_SPAR_BUDDY          = 1814,                     // sparbuddy 阵营 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum DawnbladeBloodKnightActions
{
    DAWNBLADE_BLOOD_KNIGHT_STOP_EVENT,
    DAWNBLADE_BLOOD_KNIGHT_SEAL_OF_WRATH,
    DAWNBLADE_BLOOD_KNIGHT_JUDGEMENT_OF_WRATH,
    DAWNBLADE_BLOOD_KNIGHT_HOLY_LIGHT,
    DAWNBLADE_BLOOD_KNIGHT_ACTION_MAX,
    DAWNBLADE_BLOOD_KNIGHT_START_EVENT,
};

struct npc_dawnblade_blood_knight : public CombatAI
{
    npc_dawnblade_blood_knight(Creature* creature) : CombatAI(creature, DAWNBLADE_BLOOD_KNIGHT_ACTION_MAX)
    {
        AddCombatAction(DAWNBLADE_BLOOD_KNIGHT_HOLY_LIGHT, 0, 0);
        AddCombatAction(DAWNBLADE_BLOOD_KNIGHT_SEAL_OF_WRATH, 5000, 10000);
        AddCombatAction(DAWNBLADE_BLOOD_KNIGHT_JUDGEMENT_OF_WRATH, 10000, 20000);
        AddCustomAction(DAWNBLADE_BLOOD_KNIGHT_START_EVENT, true, [&]() { StartDuel(); });
        AddTimerlessCombatAction(DAWNBLADE_BLOOD_KNIGHT_STOP_EVENT, false);
    }

    ObjectGuid m_sparringPartner;

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case DAWNBLADE_BLOOD_KNIGHT_STOP_EVENT:
            {
                if (m_creature->IsInCombat())
                {
                    if (m_sparringPartner)
                    {
                        if (Unit* attacking = m_creature->GetVictim())
                            if (attacking->GetObjectGuid() != m_sparringPartner || attacking->GetHealthPercent() <= 15.f)
                                StopDuel();
                    }
                }
                break;
            }
            case DAWNBLADE_BLOOD_KNIGHT_SEAL_OF_WRATH:
            {
                if (DoCastSpellIfCan(nullptr, SPELL_SEAL_OF_WRATH) == CAST_OK)
                    ResetCombatAction(action, urand(15000, 25000));
                break;
            }
            case DAWNBLADE_BLOOD_KNIGHT_JUDGEMENT_OF_WRATH:
            {
                if (m_creature->HasAura(SPELL_SEAL_OF_WRATH))
                    if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_JUDGEMENT_OF_WRATH) == CAST_OK)
                    {
                        m_creature->RemoveAurasDueToSpell(SPELL_SEAL_OF_WRATH);
                        ResetCombatAction(action, urand(25000, 30000));
                    }
                break;
            }
            case DAWNBLADE_BLOOD_KNIGHT_HOLY_LIGHT:
            {
                if (Unit* target = DoSelectLowestHpFriendly(40.0f, 50.0, true, true))
                    if (target->GetObjectGuid() == m_creature->GetObjectGuid() || (target->GetFaction() != FACTION_SPARRING_DAWNBLADE && target->GetFaction() != FACTION_SPAR_BUDDY))
                        if (DoCastSpellIfCan(target, SPELL_HOLY_LIGHT) == CAST_OK)
                            ResetCombatAction(action, urand(16000, 24000));
                break;
            }
        }
    }

    void JustRespawned() override
    {
        CombatAI::JustRespawned();
        if (m_creature->GetAreaId() == AREA_ID_DAWNSTAR_VILLAGE)
        {
            if (Creature* partner = GetClosestCreatureWithEntry(m_creature, m_creature->GetEntry(), 5.f, true, false, true))
            {
                m_sparringPartner = partner->GetObjectGuid();
                ResetTimer(DAWNBLADE_BLOOD_KNIGHT_START_EVENT, 5000u);
                SetActionReadyStatus(DAWNBLADE_BLOOD_KNIGHT_STOP_EVENT, true);
            }
        }
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* sender, Unit* invoker, uint32 /*miscValue*/) override
    {
        if (sender->GetEntry() != m_creature->GetEntry())
            return;

        if (eventType == AI_EVENT_CUSTOM_A) // start duel (开始duel)
        {
            m_sparringPartner = invoker->GetObjectGuid();
            SetReactState(REACT_DEFENSIVE);
            AttackStart(invoker);
        }
        else if (eventType == AI_EVENT_CUSTOM_B) // end duel
        {
            SetReactState(REACT_AGGRESSIVE);
            if (Creature* partner = m_creature->GetMap()->GetCreature(m_sparringPartner))
            {
                m_creature->getHostileRefManager().deleteReference(partner);
                if (m_creature->GetVictim() == partner)
                    m_creature->AttackStop();
            }
        }
    }

    void StartDuel()
    {
        if (Creature* partner = m_creature->GetMap()->GetCreature(m_sparringPartner))
        {
            m_creature->CastSpell(nullptr, SPELL_FACTION_SPAR, TRIGGERED_NONE);
            partner->CastSpell(nullptr, SPELL_FACTION_SPAR_BUDDY, TRIGGERED_NONE);
            SendAIEvent(AI_EVENT_CUSTOM_A, partner, m_creature);
            SendAIEvent(AI_EVENT_CUSTOM_A, m_creature, partner);
        }
    }

    void StopDuel()
    {
        SendAIEvent(AI_EVENT_CUSTOM_B, m_creature, m_creature);
        if (Creature* partner = m_creature->GetMap()->GetCreature(m_sparringPartner))
        {
            SendAIEvent(AI_EVENT_CUSTOM_B, m_creature, partner);
            m_creature->RestoreOriginalFaction();
            partner->RestoreOriginalFaction();
        }
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        CombatAI::JustReachedHome();
        if (m_sparringPartner)
            ResetTimer(DAWNBLADE_BLOOD_KNIGHT_START_EVENT, 30000u);
    }

    // 进入战斗回调：记录开战状态并启动技能循环
    void Aggro(Unit* who) override
    {
        if (m_creature->IsMounted())
            DoCastSpellIfCan(who, SPELL_JOUST);
        CombatAI::Aggro(who);
    }
};

struct SparAuras : public AuraScript
{
    bool OnCheckProc(Aura* /*aura*/, ProcExecutionData& data) const override
    {
        if (data.attacker && !data.attacker->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
            return false;
        return true;
    }

    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply)
        {
            if (npc_dawnblade_blood_knight* dawnbladeAI = dynamic_cast<npc_dawnblade_blood_knight*>(aura->GetTarget()->AI()))
            {
                dawnbladeAI->StopDuel();
            }
        }
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    // Quest "Disrupt the Greengill Coast" (11541)
    SPELL_ORB_OF_MURLOC_CONTROL         = 45109,            // orbofmurloccontrol 法术 ID
    SPELL_GREENGILL_SLAVE_FREED         = 45110,            // greengillslavefreed 法术 ID
    SPELL_ENRAGE                        = 45111,            // 狂暴 法术 ID
    NPC_FREED_GREENGILL_SLAVE           = 25085,            // freedgreengillslave Entry ID
    NPC_DARKSPINE_MYRMIDON              = 25060,            // darkspinemyrmidon Entry ID
    NPC_DARKSPINE_SIREN                 = 25073,            // darkspinesiren Entry ID
};

// 45111 - Enrage (45111 - 狂暴)
struct EnrageDisruptTheGreenCoast : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply || !aura->GetTarget()->IsCreature())
            return;

        Unit* target = aura->GetTarget();

        if (Creature* creature = GetClosestCreatureWithEntry(target, NPC_DARKSPINE_MYRMIDON, 50.0f))
        {
            target->AI()->AttackStart(creature);
            return;
        }

        if (Creature* creature = GetClosestCreatureWithEntry(target, NPC_DARKSPINE_SIREN, 50.0f))
        {
            target->AI()->AttackStart(creature);
            return;
        }
    }
};

// 45109 - Orb of Murloc Control
struct OrbOfMurlocControl : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* caster = spell->GetCaster();
        Unit* target = spell->GetUnitTarget();
        if (!target->IsCreature())
            return;

        target->CastSpell(caster, SPELL_GREENGILL_SLAVE_FREED, TRIGGERED_OLD_TRIGGERED);

        // Freed Greengill Slave
        static_cast<Creature*>(target)->UpdateEntry(NPC_FREED_GREENGILL_SLAVE);

        target->CastSpell(target, SPELL_ENRAGE, TRIGGERED_OLD_TRIGGERED);
    }
};

// 注册并载入脚本模块：isle_of_queldanas
void AddSC_isle_of_queldanas()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_converted_sentry";
    pNewScript->GetAI = &GetAI_npc_converted_sentry;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_shattered_sun_marksman";
    pNewScript->GetAI = &GetNewAIInstance<npc_shattered_sun_marksmanAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_shattered_sun_warrior";
    pNewScript->GetAI = &GetNewAIInstance<npc_shattered_sun_warriorAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_dawnblade_blood_knight";
    pNewScript->GetAI = &GetNewAIInstance<npc_dawnblade_blood_knight>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<ShatteredSunMarksmanShoot>("spell_shattered_sun_marksman_shoot");
    RegisterSpellScript<SparAuras>("spell_spar_auras");
    RegisterSpellScript<EnrageDisruptTheGreenCoast>("spell_enrage_disrupt_the_green_coast");
    RegisterSpellScript<OrbOfMurlocControl>("spell_orb_of_murloc_control");
}
