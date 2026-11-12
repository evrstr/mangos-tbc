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
SDName: Boss_Priestess_Delrissa
SD%Complete: 100
SDComment:
SDCategory: Magister's Terrace
EndScriptData */

// 女祭司德莉希亚（Priestess Delrissa）3号首领战 AI（竞技场风格队伍，四名随机随从）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "magisters_terrace.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum Delrissa
{
    SAY_AGGRO                   = -1585012,                 // 开战 台词广播 ID
    SAY_DEATH                   = -1585022,                 // 死亡 台词广播 ID

    SPELL_HEALING_POTION        = 15503,                    // 治疗potion 法术 ID
    SPELL_DISPEL_MAGIC          = 27609,                    // dispelmagic 法术 ID
    SPELL_MEDALLION             = 46227,                    // medallion 法术 ID
    SPELL_FLASH_HEAL            = 17843,                    // flash治疗 法术 ID
    SPELL_SHADOW_WORD_PAIN      = 14032,                    // 暗影wordpain 法术 ID
    SPELL_SHADOW_WORD_PAIN_H    = 15654,                    // 暗影wordpainh 法术 ID
    SPELL_SCREAM                = 27610,                    // scream 法术 ID
    SPELL_SHIELD                = 44175,                    // 护盾 法术 ID
    SPELL_SHIELD_H              = 46193,                    // 护盾h 法术 ID
    SPELL_RENEW                 = 44174,                    // renew 法术 ID
    SPELL_RENEW_H               = 46192,                    // renewh 法术 ID

    SPELL_PERMANENT_FEIGN_DEATH = 29266,                    // permanentfeign死亡 法术 ID
    SPELL_SUICIDE               = 3617,                     // 自杀 法术 ID

    MAX_COMPANIONS              = 8,                        // companions 标识 ID
};

static const int32 aPlayerDeath[] = { -1585017, -1585018, -1585019, -1585020, -1585021};
static const uint32 aDelrissaLackeys[MAX_COMPANIONS] = {NPC_KAGANI, NPC_ELLRYS, NPC_ERAMAS, NPC_YAZZAI, NPC_SALARIS, NPC_GARAXXAS, NPC_APOKO, NPC_ZELFAN};

static const float aLackeyLocations[MAX_DELRISSA_ADDS][4] =
{
    {123.77f,  17.6007f, -19.921f, 4.98f},
    {131.731f, 15.0827f, -19.921f, 4.98f},
    {121.563f, 15.6213f, -19.921f, 4.98f},
    {129.988f, 17.2355f, -19.921f, 4.98f},
};

// 枚举定义：法术技能、生物与环境事件标识
enum CommonActions
{
    COMMON_RETARGET,
    COMMON_MEDALLION,
    COMMON_ACTION_MAX,
};

struct priestess_commonAI : public CombatAI
{
    priestess_commonAI(Creature* creature, uint32 actions) : CombatAI(creature, actions),
        m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData())), m_isRegularMode(creature->GetMap()->IsRegularDifficulty())
    {
        AddCombatAction(COMMON_RETARGET, 5000u);
        if (!m_isRegularMode)
            AddCombatAction(COMMON_MEDALLION, 1000u);
    }

    ScriptedInstance* m_instance;
    bool m_isRegularMode;

    virtual void ExecuteActions() override
    {
        if (!m_creature->IsAlive() || m_creature->GetCombatManager().IsEvadingHome() || (m_unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED) && m_unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED)) ||
            m_unit->hasUnitState(UNIT_STAT_PROPELLED | UNIT_STAT_RETREATING) || m_unit->IsNonMeleeSpellCasted(false))
            return; // custom condition due to her medallion - TODO: resolve globally

        for (uint32 i = 0; i < GetCombatActionCount(); ++i)
        {
            // can be changed on any action - prevent all additional ones
            if (GetCombatScriptStatus())
                return;

            if (GetActionReadyStatus(i))
                ExecuteAction(i);
        }
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case COMMON_RETARGET:
            {
                if (m_creature->getThreatManager().getThreatList().size() <= 1)
                    break;

                std::vector<Unit*> units;
                DoResetThreat();
                for (auto& data : m_creature->getThreatManager().getThreatList())
                {
                    if (data->isValid())
                        if (data->getTarget()->IsPlayer())
                            units.push_back(data->getTarget());
                }
                std::shuffle(units.begin(), units.end(), *GetRandomGenerator());
                uint32 i = 0;
                for (Unit* unit : units)
                    m_creature->AddThreat(unit, urand(1, (++i) * 100000));
                ResetCombatAction(action, 10000);
                break;
            }
            case COMMON_MEDALLION:
                if (m_creature->isFrozen() || m_creature->IsCrowdControlled())
                    if (DoCastSpellIfCan(nullptr, SPELL_MEDALLION) == CAST_OK)
                        ResetCombatAction(action, 60000);
                break;
        }
    }
};

/*######
## boss_priestess_delrissa
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum DelrissaActions
{
    DELRISSA_HEAL = COMMON_ACTION_MAX,                      // 治疗 标识 ID
    DELRISSA_RENEW,
    DELRISSA_SHIELD,
    DELRISSA_SHADOW_WORD_PAIN,
    DELRISSA_DISPEL,
    DELRISSA_SCREAM,
    DELRISSA_ACTION_MAX,
};

// 首领战 AI：德莉希亚（priestess_delrissa）
struct boss_priestess_delrissaAI : public priestess_commonAI
{
    boss_priestess_delrissaAI(Creature* creature) : priestess_commonAI(creature, DELRISSA_ACTION_MAX)
    {
        AddCombatAction(DELRISSA_HEAL, 0u);
        AddCombatAction(DELRISSA_RENEW, 0u);
        AddCombatAction(DELRISSA_SHIELD, 0u);
        AddCombatAction(DELRISSA_SHADOW_WORD_PAIN, 1000, 2000);
        AddCombatAction(DELRISSA_DISPEL, 0, 1000);
        AddCombatAction(DELRISSA_SCREAM, 30000, 32000);
    }

    std::vector<uint32> m_vuiLackeyEnties;

    uint8 m_playersKilled;
    uint32 m_summonsKilled;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();
        m_playersKilled = 0;
        m_summonsKilled = 0;

        DoInitializeCompanions();

        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);

        SetDeathPrevention(true);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_DELRISSA, FAIL);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_DELRISSA, IN_PROGRESS);
    }

    void JustPreventedDeath(Unit* /*attacker*/) override
    {
        DoFakeDeath(SPELL_PERMANENT_FEIGN_DEATH);
        if (m_summonsKilled >= 4)
            m_creature->CastSpell(nullptr, SPELL_SUICIDE, TRIGGERED_OLD_TRIGGERED);
    }

    // Summon four random adds to help during the fight (召唤 four random adds to help during the fight)
    void DoInitializeCompanions()
    {
        // can be called if creature are dead, so avoid
        if (!m_creature->IsAlive())
            return;

        // it's empty, so first time (容器为空，初次初始化)
        if (m_vuiLackeyEnties.empty())
        {
            // pre-allocate size for speed (预分配容器大小以提高效率)
            m_vuiLackeyEnties.resize(MAX_COMPANIONS);

            // fill vector array with entries from creature array (使用生物条目填充数组)
            for (uint8 i = 0; i < MAX_COMPANIONS; ++i)
                m_vuiLackeyEnties[i] = aDelrissaLackeys[i];

            std::shuffle(m_vuiLackeyEnties.begin(), m_vuiLackeyEnties.end(), *GetRandomGenerator());

            // Summon the 4 entries (召唤这 4个指定条目)
            for (uint8 i = 0; i < MAX_DELRISSA_ADDS; ++i)
                m_creature->SummonCreature(m_vuiLackeyEnties[i], aLackeyLocations[i][0], aLackeyLocations[i][1], aLackeyLocations[i][2], aLackeyLocations[i][3], TEMPSPAWN_CORPSE_TIMED_DESPAWN, 7000);
        }
        // Resummon the killed adds (重新召唤已死亡的小怪)
        else
        {
            if (!m_instance)
                return;

            for (uint8 i = 0; i < MAX_DELRISSA_ADDS; ++i)
            {
                // If we already have the creature on the map, then don't summon it (若场景中已存在该生物则不再重复召唤)
                if (Creature* creature = m_instance->GetSingleCreatureFromStorage(m_vuiLackeyEnties[i], true))
                    if (creature->IsAlive())
                        continue;

                m_creature->SummonCreature(m_vuiLackeyEnties[i], aLackeyLocations[i][0], aLackeyLocations[i][1], aLackeyLocations[i][2], aLackeyLocations[i][3], TEMPSPAWN_CORPSE_TIMED_DESPAWN, 7000);
            }
        }
    }

    // 击杀目标回调：播放击杀台词
    void KilledUnit(Unit* victim) override
    {
        if (victim->GetTypeId() != TYPEID_PLAYER)
            return;

        DoScriptText(aPlayerDeath[m_playersKilled], m_creature);
        ++m_playersKilled;

        // reset counter (重置counter)
        if (m_playersKilled == 5)
            m_playersKilled = 0;
    }

    void SummonedCreatureJustDied(Creature* /*summoned*/) override
    {
        ++m_summonsKilled;
        if (!m_creature->HasAura(SPELL_PERMANENT_FEIGN_DEATH))
            DoScriptText(aDelrissaAddDeath[m_summonsKilled - 1], m_creature);
        if (m_summonsKilled >= 4 && m_creature->HasAura(SPELL_PERMANENT_FEIGN_DEATH))
            m_creature->CastSpell(nullptr, SPELL_SUICIDE, TRIGGERED_OLD_TRIGGERED);
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);

        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);

        if (m_instance)
            m_instance->SetData(TYPE_DELRISSA, DONE);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case DELRISSA_HEAL:
                if (Unit* target = DoSelectLowestHpFriendly(40.f, 25.0f, true))
                    if (DoCastSpellIfCan(target, SPELL_FLASH_HEAL) == CAST_OK)
                        ResetCombatAction(action, urand(2000, 4000));
                break;
            case DELRISSA_RENEW:
                if (Unit* target = DoSelectLowestHpFriendly(40.f, 5.0f, true))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_RENEW : SPELL_RENEW_H) == CAST_OK)
                        ResetCombatAction(action, urand(15000, 17000));
                break;
            case DELRISSA_SHIELD:
                if (Unit* target = DoSelectLowestHpFriendly(40.f, 1.f, true))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_SHIELD : SPELL_SHIELD_H) == CAST_OK)
                        ResetCombatAction(action, urand(15000, 19000));
                break;
            case DELRISSA_DISPEL:
            {
                std::vector<Unit*> unitVector;
                m_creature->SelectAttackingTargets(unitVector, ATTACKING_TARGET_ALL_SUITABLE, 0, SPELL_DISPEL_MAGIC, SELECT_FLAG_PLAYER);
                for (Unit* unit : unitVector) // TODO: Code dispel detection
                {
                    if (DoCastSpellIfCan(unit, SPELL_DISPEL_MAGIC) == CAST_OK)
                        ResetCombatAction(action, urand(2000, 4000));
                    break;
                }
                break;
            }
            case DELRISSA_SHADOW_WORD_PAIN:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, m_isRegularMode ? SPELL_SHADOW_WORD_PAIN : SPELL_SHADOW_WORD_PAIN_H, m_isRegularMode ? SPELL_SHADOW_WORD_PAIN : SPELL_SHADOW_WORD_PAIN_H, (SELECT_FLAG_PLAYER | SELECT_FLAG_NOT_AURA)))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_SHADOW_WORD_PAIN : SPELL_SHADOW_WORD_PAIN_H) == CAST_OK)
                        ResetCombatAction(action, urand(5000, 14000));
                break;
            case DELRISSA_SCREAM:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_NEAREST_BY, 0, SPELL_SCREAM, (SELECT_FLAG_IN_LOS | SELECT_FLAG_PLAYER | SELECT_FLAG_RANGE_AOE_RANGE)))
                    if (DoCastSpellIfCan(nullptr, SPELL_SCREAM) == CAST_OK)
                        ResetCombatAction(action, urand(30000, 32000));
                break;
        }
        priestess_commonAI::ExecuteAction(action);
    }
};

/*######
## priestess_companion_common
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum CompanionActions
{
    COMPANION_POTION = COMMON_ACTION_MAX,                   // potion 标识 ID
    COMPANION_ACTION_MAX,
};

struct priestess_companion_commonAI : public priestess_commonAI
{
    priestess_companion_commonAI(Creature* creature, uint32 actions) : priestess_commonAI(creature, actions)
    {
        AddTimerlessCombatAction(COMPANION_POTION, true);
    }

    // 击杀目标回调：播放击杀台词
    void KilledUnit(Unit* victim) override
    {
        if (!m_instance)
            return;

        if (Creature* delrissa = m_instance->GetSingleCreatureFromStorage(NPC_DELRISSA))
            delrissa->AI()->KilledUnit(victim);
    }

    // 进入脱战闪避模式
    void EnterEvadeMode() override
    {
        priestess_commonAI::EnterEvadeMode();
        if (Creature* delrissa = m_instance->GetSingleCreatureFromStorage(NPC_DELRISSA))
            if (delrissa->IsAlive() && delrissa->IsInCombat())
                delrissa->AI()->EnterEvadeMode();
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case COMPANION_POTION:
                if (m_creature->GetHealthPercent() > 20.0f)
                    return;
                if (DoCastSpellIfCan(nullptr, SPELL_HEALING_POTION) == CAST_OK)
                    SetActionReadyStatus(action, false);
                break;
        }
        priestess_commonAI::ExecuteAction(action);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum Kagani
{
    SPELL_KIDNEY_SHOT       = 27615,                        // kidneyshot 法术 ID
    SPELL_GOUGE             = 12540,                        // gouge 法术 ID
    SPELL_KICK              = 27613,                        // kick 法术 ID
    SPELL_VANISH            = 44290,                        // vanish 法术 ID
    SPELL_BACKSTAB          = 15657,                        // backstab 法术 ID
    SPELL_BACKSTAB_H        = 15582,                        // backstabh 法术 ID
    SPELL_EVISCERATE        = 27611,                        // eviscerate 法术 ID
    SPELL_EVISCERATE_H      = 46189,                        // eviscerateh 法术 ID
};

/*######
## npc_kagani_nightstrike - Rogue
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum KaganiActions
{
    KAGANI_GOUGE = COMPANION_ACTION_MAX,                    // gouge 标识 ID
    KAGANI_KICK,
    KAGANI_VANISH,
    KAGANI_EVISCERATE,
    KAGANI_BACKSTAB,
    KAGANI_KIDNEY_SHOT,
    KAGANI_ACTION_MAX,
    KAGANI_VANISH_END,
};

// 随从/NPC AI：nightstrike（kagani_nightstrike）
struct npc_kagani_nightstrikeAI : public priestess_companion_commonAI
{
    npc_kagani_nightstrikeAI(Creature* creature) : priestess_companion_commonAI(creature, KAGANI_ACTION_MAX)
    {
        AddCombatAction(KAGANI_GOUGE, 0, 1000);
        AddCombatAction(KAGANI_KICK, 0, 1000);
        AddCombatAction(KAGANI_VANISH, 0u);
        AddCombatAction(KAGANI_EVISCERATE, 12000, 13000);
        AddCombatAction(KAGANI_BACKSTAB, 1000, 2000);
        AddCombatAction(KAGANI_KIDNEY_SHOT, 0, 1000);
        AddCustomAction(KAGANI_VANISH_END, true, [&]()
        {
            SetMeleeEnabled(true);
            SetCombatScriptStatus(false);
        });
    }

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        priestess_companion_commonAI::Reset();
        SetCombatScriptStatus(false);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case KAGANI_GOUGE:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_GOUGE) == CAST_OK)
                    ResetCombatAction(action, urand(14000, 15000));
                break;
            case KAGANI_KICK:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_KICK) == CAST_OK)
                    ResetCombatAction(action, urand(15000, 19000));
                break;
            case KAGANI_VANISH:
                if (m_creature->GetHealthPercent() > 50.f)
                    break;

                if (DoCastSpellIfCan(nullptr, SPELL_VANISH) == CAST_OK)
                {
                    // Prefer targets with mana
                    if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, uint32(0), SELECT_FLAG_POWER_MANA | SELECT_FLAG_PLAYER))
                    {
                        DoResetThreat();
                        AttackStart(target);
                        SetCombatScriptStatus(true);
                        SetMeleeEnabled(false);
                    }

                    ResetCombatAction(action, 300000);
                    ResetTimer(KAGANI_VANISH_END, 10000);
                }
                break;
            case KAGANI_EVISCERATE:
                if (DoCastSpellIfCan(m_creature->GetVictim(), m_isRegularMode ? SPELL_EVISCERATE : SPELL_EVISCERATE_H) == CAST_OK)
                    ResetCombatAction(action, urand(11000, 15000));
                break;
            case KAGANI_BACKSTAB:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_BACKSTAB) == CAST_OK)
                    ResetCombatAction(action, urand(6000, 8000));
                break;
            case KAGANI_KIDNEY_SHOT:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_KIDNEY_SHOT) == CAST_OK)
                    ResetCombatAction(action, 23000);
                break;
        }
        priestess_companion_commonAI::ExecuteAction(action);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum Ellrys
{
    SPELL_IMMOLATE              = 44267,                    // 献祭 法术 ID
    SPELL_IMMOLATE_H            = 46191,                    // 献祭h 法术 ID
    SPELL_SHADOW_BOLT           = 12471,                    // 暗影箭 法术 ID
    SPELL_SHADOW_BOLT_H         = 15232,                    // 暗影箭h 法术 ID
    SPELL_SEED_OF_CORRUPTION    = 44141,                    // seedof腐蚀 法术 ID
    SPELL_CURSE_OF_AGONY        = 14875,                    // 诅咒ofagony 法术 ID
    SPELL_CURSE_OF_AGONY_H      = 46190,                    // 诅咒ofagonyh 法术 ID
    SPELL_FEAR                  = 38595,                    // 恐惧 法术 ID
    SPELL_DEATH_COIL            = 44142,                    // 死亡coil 法术 ID
    SPELL_SUMMON_IMP            = 44163,                    // 召唤imp 法术 ID

    NPC_FIZZLE                  = 24656,                    // fizzle Entry ID
};

/*######
## npc_ellrys_duskhallow - Warlock
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum EllrysActions
{
    ELLRYS_IMMOLATE = COMPANION_ACTION_MAX,                 // 献祭 标识 ID
    ELLRYS_SHADOWBOLT,
    ELLRYS_SEED_OF_CORRUPTION,
    ELLRYS_CURSE_OF_AGONY,
    ELLRYS_FEAR,
    ELLRYS_DEATH_COIL,
    ELLRYS_ACTION_MAX,
};

// 随从/NPC AI：duskhallow（ellrys_duskhallow）
struct npc_ellrys_duskhallowAI : public priestess_companion_commonAI
{
    npc_ellrys_duskhallowAI(Creature* creature) : priestess_companion_commonAI(creature, ELLRYS_ACTION_MAX)
    {
        AddCombatAction(ELLRYS_IMMOLATE, 8000, 21000);
        AddCombatAction(ELLRYS_SHADOWBOLT, 3000u);
        AddCombatAction(ELLRYS_SEED_OF_CORRUPTION, 1000, 10000);
        AddCombatAction(ELLRYS_CURSE_OF_AGONY, 1000, 12000);
        AddCombatAction(ELLRYS_FEAR, 8000, 23000);
        AddCombatAction(ELLRYS_DEATH_COIL, 8000, 16000);
        SetRangedMode(true, 25.f, TYPE_PROXIMITY);
        AddMainSpell(m_isRegularMode ? SPELL_SHADOW_BOLT : SPELL_SHADOW_BOLT_H);
    }

    GuidVector m_spawns;

    void JustRespawned() override
    {
        priestess_companion_commonAI::JustRespawned();
        DoCastSpellIfCan(nullptr, SPELL_SUMMON_IMP);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        priestess_companion_commonAI::JustReachedHome();
        Pet* imp = m_creature->FindGuardianWithEntry(NPC_FIZZLE);
        if (!imp || !imp->IsAlive())
            DoCastSpellIfCan(nullptr, SPELL_SUMMON_IMP);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        m_spawns.push_back(summoned->GetObjectGuid());
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case ELLRYS_IMMOLATE:
                if (DoCastSpellIfCan(m_creature->GetVictim(), m_isRegularMode ? SPELL_IMMOLATE : SPELL_IMMOLATE_H) == CAST_OK)
                    ResetCombatAction(action, urand(16000, 30000));
                break;
            case ELLRYS_SHADOWBOLT:
                if (DoCastSpellIfCan(m_creature->GetVictim(), m_isRegularMode ? SPELL_SHADOW_BOLT : SPELL_SHADOW_BOLT_H) == CAST_OK)
                    ResetCombatAction(action, urand(2400, 3600));
                break;
            case ELLRYS_SEED_OF_CORRUPTION:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_SEED_OF_CORRUPTION, SELECT_FLAG_PLAYER | SELECT_FLAG_NOT_AURA))
                    if (DoCastSpellIfCan(target, SPELL_SEED_OF_CORRUPTION) == CAST_OK)
                        ResetCombatAction(action, urand(16000, 30000));
                break;
            case ELLRYS_CURSE_OF_AGONY:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, m_isRegularMode ? SPELL_CURSE_OF_AGONY : SPELL_CURSE_OF_AGONY_H, SELECT_FLAG_PLAYER | SELECT_FLAG_NOT_AURA))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_CURSE_OF_AGONY : SPELL_CURSE_OF_AGONY_H) == CAST_OK)
                        ResetCombatAction(action, urand(9000, 20000));
                break;
            case ELLRYS_FEAR:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER | SELECT_FLAG_SKIP_TANK))
                    if (DoCastSpellIfCan(target, SPELL_FEAR) == CAST_OK)
                        ResetCombatAction(action, urand(18000, 49000));
                break;
            case ELLRYS_DEATH_COIL:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, SPELL_DEATH_COIL) == CAST_OK)
                        ResetCombatAction(action, urand(10000, 20000));
                break;
        }
        priestess_companion_commonAI::ExecuteAction(action);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum Eramas
{
    SPELL_KNOCKDOWN             = 11428,                    // 击倒 法术 ID
    SPELL_KNOCKDOWN_H           = 46183,                    // 击倒h 法术 ID
    SPELL_SNAP_KICK             = 46182,                    // snapkick 法术 ID
    SPELL_FISTS_OF_ARCANE_FURY  = 44118,                    // fistsof奥术fury 法术 ID

    SPELL_DUAL_WEILD            = 42459,                    // dualweild 法术 ID
};

/*######
## npc_eramas_brightblaze - Monk
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum EramasActions
{
    ERAMAS_KNOCKDOWN = COMPANION_ACTION_MAX,                // 击倒 标识 ID
    ERAMAS_KICK,
    ERAMAS_ACTION_MAX,
};

// 随从/NPC AI：brightblaze（eramas_brightblaze）
struct npc_eramas_brightblazeAI : public priestess_companion_commonAI
{
    npc_eramas_brightblazeAI(Creature* creature) : priestess_companion_commonAI(creature, ERAMAS_ACTION_MAX)
    {
        AddCombatAction(ERAMAS_KNOCKDOWN, 15000, 16000);
        AddCombatAction(ERAMAS_KICK, 0, 1000);
    }

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        priestess_companion_commonAI::Reset();
        DoCastSpellIfCan(nullptr, SPELL_DUAL_WEILD, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
        DoCastSpellIfCan(nullptr, SPELL_FISTS_OF_ARCANE_FURY, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case ERAMAS_KNOCKDOWN:
                if (DoCastSpellIfCan(m_creature->GetVictim(), m_isRegularMode ? SPELL_KNOCKDOWN : SPELL_KNOCKDOWN_H) == CAST_OK)
                    ResetCombatAction(action, urand(14000, 18000));
                break;
            case ERAMAS_KICK:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_SNAP_KICK) == CAST_OK)
                    ResetCombatAction(action, urand(12000, 15000));
                break;
        }
        priestess_companion_commonAI::ExecuteAction(action);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum Yazzai
{
    SPELL_POLYMORPH         = 13323,                        // polymorph 法术 ID
    SPELL_ICE_BLOCK         = 27619,                        // iceblock 法术 ID
    SPELL_BLIZZARD          = 44178,                        // 暴风雪 法术 ID
    SPELL_BLIZZARD_H        = 46195,                        // 暴风雪h 法术 ID
    SPELL_ICE_LANCE         = 44176,                        // icelance 法术 ID
    SPELL_ICE_LANCE_H       = 46194,                        // icelanceh 法术 ID
    SPELL_CONE_OF_COLD      = 12611,                        // coneofcold 法术 ID
    SPELL_CONE_OF_COLD_H    = 38384,                        // coneofcoldh 法术 ID
    SPELL_FROSTBOLT         = 15043,                        // frostbolt 法术 ID
    SPELL_FROSTBOLT_H       = 15530,                        // frostbolth 法术 ID
    SPELL_BLINK             = 14514,                        // blink 法术 ID
    SPELL_FROST_NOVA        = 44177,                        // 冰霜新星 法术 ID
};

/*######
## npc_yazzai - Mage
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum YazzaiActions
{
    YAZZAI_POLYMORPH = COMPANION_ACTION_MAX,                // polymorph 标识 ID
    YAZZAI_FROST_NOVA,
    YAZZAI_ICE_BLOCK,
    YAZZAI_BLIZZARD,
    YAZZAI_ICE_LANCE,
    YAZZAI_CONE_OF_COLD,
    YAZZAI_FROSTBOLT,
    YAZZAI_BLINK,
    YAZZAI_ACTION_MAX,
};

// 随从/NPC AI：标识 ID（yazzai）
struct npc_yazzaiAI : public priestess_companion_commonAI
{
    npc_yazzaiAI(Creature* creature) : priestess_companion_commonAI(creature, YAZZAI_ACTION_MAX)
    {
        AddCombatAction(YAZZAI_POLYMORPH, 12000, 22000);
        AddCombatAction(YAZZAI_FROST_NOVA, 4000, 28000);
        AddCombatAction(YAZZAI_ICE_BLOCK, true);
        AddCombatAction(YAZZAI_BLIZZARD, 7000, 8000);
        AddCombatAction(YAZZAI_ICE_LANCE, true);
        AddCombatAction(YAZZAI_CONE_OF_COLD, 1000, 11000);
        AddCombatAction(YAZZAI_FROSTBOLT, 0u);
        AddCombatAction(YAZZAI_BLINK, 8000, 24000);
        SetRangedMode(true, 25.f, TYPE_PROXIMITY);
        AddDistanceSpell(SPELL_FROST_NOVA);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case YAZZAI_POLYMORPH:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER | SELECT_FLAG_SKIP_TANK))
                    if (DoCastSpellIfCan(target, SPELL_POLYMORPH) == CAST_OK)
                        ResetCombatAction(action, urand(28000, 44000));
                break;
            case YAZZAI_FROST_NOVA:
                if (m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_FROST_NOVA, SELECT_FLAG_PLAYER | SELECT_FLAG_USE_EFFECT_RADIUS))
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_FROST_NOVA) == CAST_OK)
                    {
                        DistanceYourself();
                        ResetCombatAction(action, urand(26000, 34000));
                        ResetCombatAction(YAZZAI_ICE_LANCE, 3000);
                    }
                }
                break;
            case YAZZAI_ICE_BLOCK:
                if (m_creature->GetHealthPercent() < 50.0f)
                    if (DoCastSpellIfCan(nullptr, SPELL_ICE_BLOCK) == CAST_OK)
                        ResetCombatAction(action, 300000);
                break;
            case YAZZAI_BLIZZARD:
                if (m_creature->getThreatManager().getThreatList().size() < 3)
                    break;

                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_BLIZZARD : SPELL_BLIZZARD_H) == CAST_OK)
                        ResetCombatAction(action, urand(13000, 16000));
                break;
            case YAZZAI_ICE_LANCE:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_ICE_LANCE : SPELL_ICE_LANCE_H) == CAST_OK)
                        DisableCombatAction(action);
                break;
            case YAZZAI_CONE_OF_COLD:
                if (m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, m_isRegularMode ? SPELL_CONE_OF_COLD : SPELL_CONE_OF_COLD_H, SELECT_FLAG_PLAYER | SELECT_FLAG_USE_EFFECT_RADIUS))
                    if (DoCastSpellIfCan(nullptr, m_isRegularMode ? SPELL_CONE_OF_COLD : SPELL_CONE_OF_COLD_H) == CAST_OK)
                        ResetCombatAction(action, urand(11000, 17000));
                break;
            case YAZZAI_FROSTBOLT:
                if (DoCastSpellIfCan(m_creature->GetVictim(), m_isRegularMode ? SPELL_FROSTBOLT : SPELL_FROSTBOLT_H) == CAST_OK)
                    ResetCombatAction(action, urand(2400, 3600));
                break;
            case YAZZAI_BLINK:
                // if anybody is in melee range than escape by blink
                if (m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, uint32(0), SELECT_FLAG_IN_MELEE_RANGE))
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_BLINK) == CAST_OK)
                        ResetCombatAction(action, urand(26000, 40000));
                }
                else
                    ResetCombatAction(action, 2000);
                break;
        }
        priestess_companion_commonAI::ExecuteAction(action);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum Salaris
{
    SPELL_INTERCEPT_STUN        = 27577,                    // intercept击晕 法术 ID
    SPELL_DISARM                = 27581,                    // 缴械 法术 ID
    SPELL_PIERCING_HOWL         = 23600,                    // piercinghowl 法术 ID
    SPELL_FRIGHTENING_SHOUT     = 19134,                    // frighteningshout 法术 ID
    SPELL_HAMSTRING             = 27584,                    // hamstring 法术 ID
    SPELL_BATTLE_SHOUT          = 27578,                    // battleshout 法术 ID
    SPELL_MORTAL_STRIKE         = 44268                     // mortal打击 法术 ID
};

/*######
## npc_warlord_salaris - Warrior
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum SalarisActions
{
    SALARIS_INTERCEPT_STUN = COMPANION_ACTION_MAX,          // intercept击晕 标识 ID
    SALARIS_DISARM,
    SALARIS_PIERCING_HOWL,
    SALARIS_FRIGHTENING_SHOUT,
    SALARIS_HAMSTRING,
    SALARIS_BATTLE_SHOUT,
    SALARIS_MORTAL_STRIKE,
    SALARIS_ACTION_MAX,
};

// 随从/NPC AI：salaris（warlord_salaris）
struct npc_warlord_salarisAI : public priestess_companion_commonAI
{
    npc_warlord_salarisAI(Creature* creature) : priestess_companion_commonAI(creature, SALARIS_ACTION_MAX)
    {
        AddCombatAction(SALARIS_INTERCEPT_STUN, 500u);
        AddCombatAction(SALARIS_DISARM, 4000, 10000);
        AddCombatAction(SALARIS_PIERCING_HOWL, 1000, 5000);
        AddCombatAction(SALARIS_FRIGHTENING_SHOUT, 6000, 50000);
        AddCombatAction(SALARIS_HAMSTRING, 3000, 10000);
        AddCombatAction(SALARIS_MORTAL_STRIKE, 3000, 9000);
        AddCombatAction(SALARIS_BATTLE_SHOUT, 0u);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case SALARIS_INTERCEPT_STUN:
                // if nobody is in melee range than try to use Intercept
                if (!m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, uint32(0), SELECT_FLAG_IN_MELEE_RANGE))
                {
                    if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_INTERCEPT_STUN, (SELECT_FLAG_PLAYER | SELECT_FLAG_NOT_IN_MELEE_RANGE | SELECT_FLAG_IN_LOS)))
                        if (DoCastSpellIfCan(target, SPELL_INTERCEPT_STUN) == CAST_OK)
                            ResetCombatAction(action, urand(20000, 22000));
                }
                else
                    ResetCombatAction(action, 2000);
                break;
            case SALARIS_DISARM:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_DISARM) == CAST_OK)
                    ResetCombatAction(action, urand(60000, 65000));
                break;
            case SALARIS_PIERCING_HOWL:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_NEAREST_BY, 0, SPELL_PIERCING_HOWL, (SELECT_FLAG_IN_LOS | SELECT_FLAG_PLAYER | SELECT_FLAG_RANGE_AOE_RANGE)))
                    if (DoCastSpellIfCan(nullptr, SPELL_PIERCING_HOWL) == CAST_OK)
                        ResetCombatAction(action, urand(16000, 24000));
                break;
            case SALARIS_FRIGHTENING_SHOUT:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_FRIGHTENING_SHOUT) == CAST_OK)
                    ResetCombatAction(action, urand(120000, 125000));
                break;
            case SALARIS_HAMSTRING:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_HAMSTRING) == CAST_OK)
                    ResetCombatAction(action, urand(4000, 6000));
                break;
            case SALARIS_MORTAL_STRIKE:
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MORTAL_STRIKE) == CAST_OK)
                    ResetCombatAction(action, urand(6000, 10000));
                break;
            case SALARIS_BATTLE_SHOUT:
                if (DoCastSpellIfCan(nullptr, SPELL_BATTLE_SHOUT) == CAST_OK)
                    ResetCombatAction(action, urand(60000, 62000));
                break;
        }
        priestess_companion_commonAI::ExecuteAction(action);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum Garaxxas
{
    SPELL_AIMED_SHOT            = 44271,                    // aimedshot 法术 ID
    SPELL_SHOOT                 = 15620,                    // shoot 法术 ID
    SPELL_SHOOT_H               = 22907,                    // shooth 法术 ID
    SPELL_CONCUSSIVE_SHOT       = 27634,                    // concussiveshot 法术 ID
    SPELL_MULTI_SHOT            = 31942,                    // multishot 法术 ID
    SPELL_MULTI_SHOT_H          = 44285,                    // multishoth 法术 ID
    SPELL_WING_CLIP             = 44286,                    // 翼击clip 法术 ID
    SPELL_FREEZING_TRAP         = 44136,                    // freezingtrap 法术 ID
};

/*######
## npc_garaxxas - Hunter
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum GaraxxasActions
{
    GARAXXAS_AIMED_SHOT = COMPANION_ACTION_MAX,             // aimedshot 标识 ID
    GARAXXAS_SHOOT,
    GARAXXAS_CONCUSSIVE_SHOT,
    GARAXXAS_MULTI_SHOT,
    GARAXXAS_WING_CLIP,
    GARAXXAS_FREEZING_TRAP,
    GARAXXAS_ACTION_MAX,
};

// 随从/NPC AI：标识 ID（garaxxas）
struct npc_garaxxasAI : public priestess_companion_commonAI
{
    npc_garaxxasAI(Creature* creature) : priestess_companion_commonAI(creature, GARAXXAS_ACTION_MAX)
    {
        AddCombatAction(GARAXXAS_AIMED_SHOT, 1000, 2000);
        AddCombatAction(GARAXXAS_SHOOT, 2500u);
        AddCombatAction(GARAXXAS_CONCUSSIVE_SHOT, 6000, 8000);
        AddCombatAction(GARAXXAS_MULTI_SHOT, 15000, 30000);
        AddCombatAction(GARAXXAS_WING_CLIP, 1000, 2000);
        AddCombatAction(GARAXXAS_FREEZING_TRAP, 15000u);
        SetRangedMode(true, 25.0f, TYPE_PROXIMITY);
        AddDistanceSpell(SPELL_FREEZING_TRAP);
        JustReachedHome();
    }

    GuidVector m_traps;

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        priestess_companion_commonAI::Reset();

        // Check if the pet was killed
        Creature* sliver = m_instance->GetSingleCreatureFromStorage(NPC_SLIVER);
        if (!sliver || !sliver->IsAlive())
            m_creature->SummonCreature(NPC_SLIVER, 0, 0, 0, 0, TEMPSPAWN_CORPSE_DESPAWN, 0);
    }

    void JustSummoned(GameObject* summoned) override
    {
        m_traps.push_back(summoned->GetObjectGuid());
    }

    // 死亡回调：更新遭遇战状态与开启门禁
    void JustDied(Unit* killer) override
    {
        priestess_companion_commonAI::JustDied(killer);
        DespawnGuids(m_traps);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case GARAXXAS_AIMED_SHOT:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, SPELL_AIMED_SHOT) == CAST_OK)
                        ResetCombatAction(action, urand(15000, 17000));
                break;
            case GARAXXAS_SHOOT:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_SHOOT : SPELL_SHOOT_H) == CAST_OK)
                        ResetCombatAction(action, 2500);
                break;
            case GARAXXAS_CONCUSSIVE_SHOT:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, SPELL_CONCUSSIVE_SHOT) == CAST_OK)
                        ResetCombatAction(action, urand(14000, 16000));
                break;
            case GARAXXAS_MULTI_SHOT:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_MULTI_SHOT : SPELL_MULTI_SHOT_H) == CAST_OK)
                        ResetCombatAction(action, urand(13000, 17000));
                break;
            case GARAXXAS_WING_CLIP: // melee
                if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_WING_CLIP) == CAST_OK)
                {
                    DistanceYourself();
                    ResetCombatAction(action, urand(11000, 12000));
                }
                break;
            case GARAXXAS_FREEZING_TRAP: // melee
                if (!GetCurrentRangedMode())
                    if (DoCastSpellIfCan(nullptr, SPELL_FREEZING_TRAP) == CAST_OK)
                    {
                        DistanceYourself();
                        ResetCombatAction(action, urand(15000, 30000));
                    }
                break;
        }
        priestess_companion_commonAI::ExecuteAction(action);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum Apoko
{
    SPELL_WAR_STOMP             = 46026,                    // warstomp 法术 ID
    SPELL_PURGE                 = 27626,                    // purge 法术 ID
    SPELL_LESSER_HEALING_WAVE   = 44256,                    // lesser治疗wave 法术 ID
    SPELL_LESSER_HEALING_WAVE_H = 46181,                    // lesser治疗waveh 法术 ID
    SPELL_FROST_SHOCK           = 21401,                    // 冰霜震击 法术 ID
    SPELL_FROST_SHOCK_H         = 46180,                    // 冰霜震击h 法术 ID
    SPELL_WINDFURY_TOTEM        = 27621,                    // windfury图腾 法术 ID
    SPELL_FIRE_NOVA_TOTEM       = 44257,                    // 火焰新星图腾 法术 ID
    SPELL_EARTHBIND_TOTEM       = 15786,                    // earthbind图腾 法术 ID
};

/*######
## npc_apoko - Shaman
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum ApokoActions
{
    APOKO_EARTHBIND_TOTEM = COMPANION_ACTION_MAX,           // earthbind图腾 标识 ID
    APOKO_FIRE_NOVA_TOTEM,
    APOKO_WINDFURY_TOTEM,
    APOKO_WAR_STOMP,
    APOKO_PURGE,
    APOKO_HEALING_WAVE,
    APOKO_FROST_SHOCK,
    APOKO_ACTION_MAX,
};

// 随从/NPC AI：标识 ID（apoko）
struct npc_apokoAI : public priestess_companion_commonAI
{
    npc_apokoAI(Creature* creature) : priestess_companion_commonAI(creature, APOKO_ACTION_MAX)
    {
        AddCombatAction(APOKO_EARTHBIND_TOTEM, 1000, 2000);
        AddCombatAction(APOKO_FIRE_NOVA_TOTEM, 10000, 20000);
        AddCombatAction(APOKO_WINDFURY_TOTEM, 8000, 17000);
        AddCombatAction(APOKO_WAR_STOMP, 16000, 19000);
        AddCombatAction(APOKO_PURGE, 1000, 2000);
        AddCombatAction(APOKO_HEALING_WAVE, 0u);
        AddCombatAction(APOKO_FROST_SHOCK, 2000, 3000);
    }

    GuidVector m_spawns;

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        DespawnGuids(m_spawns);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        m_spawns.push_back(summoned->GetObjectGuid());
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case APOKO_EARTHBIND_TOTEM:
                if (DoCastSpellIfCan(nullptr, SPELL_EARTHBIND_TOTEM) == CAST_OK)
                    ResetCombatAction(action, urand(32000, 37000));
                break;
            case APOKO_FIRE_NOVA_TOTEM:
                if (DoCastSpellIfCan(nullptr, SPELL_FIRE_NOVA_TOTEM) == CAST_OK)
                    ResetCombatAction(action, urand(20000, 30000));
                break;
            case APOKO_WINDFURY_TOTEM:
                if (DoCastSpellIfCan(nullptr, SPELL_WINDFURY_TOTEM) == CAST_OK)
                    ResetCombatAction(action, urand(32000, 37000));
                break;
            case APOKO_WAR_STOMP:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_NEAREST_BY, 0, SPELL_WAR_STOMP, (SELECT_FLAG_IN_LOS | SELECT_FLAG_PLAYER | SELECT_FLAG_RANGE_AOE_RANGE)))
                    if (DoCastSpellIfCan(nullptr, SPELL_WAR_STOMP) == CAST_OK)
                        ResetCombatAction(action, urand(120000, 125000));
                break;
            case APOKO_PURGE:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER)) // TODO: Code dispel detection
                    if (DoCastSpellIfCan(target, SPELL_PURGE) == CAST_OK)
                        ResetCombatAction(action, urand(7000, 11000));
                break;
            case APOKO_HEALING_WAVE:
                if (Unit* target = DoSelectLowestHpFriendly(40.f, 25.0f, true))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_LESSER_HEALING_WAVE : SPELL_LESSER_HEALING_WAVE_H) == CAST_OK)
                        ResetCombatAction(action, urand(4000, 6000));
                break;
            case APOKO_FROST_SHOCK:
                if (DoCastSpellIfCan(m_creature->GetVictim(), m_isRegularMode ? SPELL_FROST_SHOCK : SPELL_FROST_SHOCK_H) == CAST_OK)
                    ResetCombatAction(action, urand(6000, 10000));
                break;
        }
        priestess_companion_commonAI::ExecuteAction(action);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum Zelfan
{
    SPELL_GOBLIN_DRAGON_GUN         = 44272,                // goblin巨龙gun 法术 ID
    SPELL_GOBLIN_DRAGON_GUN_CONE    = 44273,                // goblin巨龙guncone 法术 ID
    SPELL_GOBLIN_DRAGON_GUN_H       = 46185,                // goblin巨龙gunh 法术 ID
    SPELL_GOBLIN_DRAGON_GUN_CONE_H  = 46186,                // goblin巨龙gunconeh 法术 ID
    SPELL_ROCKET_LAUNCH             = 44137,                // 火箭发射 法术 ID
    SPELL_ROCKET_LAUNCH_H           = 46187,                // 火箭发射h 法术 ID
    SPELL_RECOMBOBULATE             = 44274,                // recombobulate 法术 ID
    SPELL_HIGH_EXPLOSIVE_SHEEP      = 44276,                // highexplosivesheep 法术 ID
    SPELL_FEL_IRON_BOMB             = 46024,                // 魔能黑铁炸弹 法术 ID
    SPELL_FEL_IRON_BOMB_H           = 46184,                // 魔能黑铁炸弹h 法术 ID

    // sheep
    SPELL_SUICIDE_TIMER         = 8327,                     // 自杀计时器 法术 ID
};

/*######
## npc_zelfan - Engineer
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum ZelfanActions
{
    ZELFAN_GOBLIN_DRAGON_GUN = COMPANION_ACTION_MAX,        // goblin巨龙gun 标识 ID
    ZELFAN_ROCKET_LAUNCH,
    ZELFAN_RECOMBOBULATE,
    ZELFAN_HIGH_EXPLOSIVE_SHEEP,
    ZELFAN_FEL_IRON_BOMB,
    ZELFAN_ACTIONS_MAX,
};

// 随从/NPC AI：标识 ID（zelfan）
struct npc_zelfanAI : public priestess_companion_commonAI
{
    npc_zelfanAI(Creature* creature) : priestess_companion_commonAI(creature, ZELFAN_ACTIONS_MAX)
    {
        AddCombatAction(ZELFAN_GOBLIN_DRAGON_GUN, 5000, 6000);
        AddCombatAction(ZELFAN_ROCKET_LAUNCH, 14000, 17000);
        AddCombatAction(ZELFAN_RECOMBOBULATE, 3000, 10000);
        AddCombatAction(ZELFAN_HIGH_EXPLOSIVE_SHEEP, 1000, 2000);
        AddCombatAction(ZELFAN_FEL_IRON_BOMB, 11000, 33000);
    }

    GuidVector m_spawns;

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        DespawnGuids(m_spawns);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        m_spawns.push_back(summoned->GetObjectGuid());
        summoned->AI()->DoCastSpellIfCan(nullptr, SPELL_SUICIDE_TIMER);
        if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_NEAREST_BY, 0, nullptr, SELECT_FLAG_PLAYER))
            summoned->AI()->AttackStart(target);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case ZELFAN_GOBLIN_DRAGON_GUN:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, m_isRegularMode ? SPELL_GOBLIN_DRAGON_GUN_CONE : SPELL_GOBLIN_DRAGON_GUN_CONE_H, SELECT_FLAG_PLAYER | SELECT_FLAG_USE_EFFECT_RADIUS))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_GOBLIN_DRAGON_GUN : SPELL_GOBLIN_DRAGON_GUN_H) == CAST_OK)
                        ResetCombatAction(action, urand(5000, 7000));
                break;
            case ZELFAN_ROCKET_LAUNCH:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_ROCKET_LAUNCH : SPELL_ROCKET_LAUNCH_H) == CAST_OK)
                        ResetCombatAction(action, urand(43000, 54000));
                break;
            case ZELFAN_RECOMBOBULATE:
            {
                std::list<Creature*> ccedFriends = DoFindFriendlyEligibleDispel(30.0f);
                for (Creature* creature : ccedFriends)
                {
                    if (creature->IsPolymorphed())
                    {
                        if (DoCastSpellIfCan(creature, SPELL_RECOMBOBULATE) == CAST_OK)
                            ResetCombatAction(action, urand(22000, 49000));
                        break;
                    }
                }
                break;
            }
            case ZELFAN_HIGH_EXPLOSIVE_SHEEP:
                if (DoCastSpellIfCan(nullptr, SPELL_HIGH_EXPLOSIVE_SHEEP) == CAST_OK)
                    ResetCombatAction(action, urand(17000, 30000));
                break;
            case ZELFAN_FEL_IRON_BOMB:
                if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0, nullptr, SELECT_FLAG_PLAYER))
                    if (DoCastSpellIfCan(target, m_isRegularMode ? SPELL_FEL_IRON_BOMB : SPELL_FEL_IRON_BOMB_H) == CAST_OK)
                        ResetCombatAction(action, urand(15000, 48000));
                break;
        }
        priestess_companion_commonAI::ExecuteAction(action);
    }
};

// 注册并载入脚本模块：boss_priestess_delrissa
void AddSC_boss_priestess_delrissa()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_priestess_delrissa";
    pNewScript->GetAI = &GetNewAIInstance<boss_priestess_delrissaAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_kagani_nightstrike";
    pNewScript->GetAI = &GetNewAIInstance<npc_kagani_nightstrikeAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_ellrys_duskhallow";
    pNewScript->GetAI = &GetNewAIInstance<npc_ellrys_duskhallowAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_eramas_brightblaze";
    pNewScript->GetAI = &GetNewAIInstance<npc_eramas_brightblazeAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_yazzai";
    pNewScript->GetAI = &GetNewAIInstance<npc_yazzaiAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_warlord_salaris";
    pNewScript->GetAI = &GetNewAIInstance<npc_warlord_salarisAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_garaxxas";
    pNewScript->GetAI = &GetNewAIInstance<npc_garaxxasAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_apoko";
    pNewScript->GetAI = &GetNewAIInstance<npc_apokoAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_zelfan";
    pNewScript->GetAI = &GetNewAIInstance<npc_zelfanAI>;
    pNewScript->RegisterSelf();
}
