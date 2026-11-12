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
SDName: Boss_Jeklik
SD%Complete: 85
SDComment: Some minor improvements are required; Bat rider movement not implemented
SDCategory: Zul'Gurub
EndScriptData */

// 高阶祭司耶克里克（High Priestess Jeklik）蝙蝠祭司首领战 AI（音波爆震群体沉默、暗影箭雨与鲜血拟态）

#include "AI/ScriptDevAI/include/sc_common.h"
#include "zulgurub.h"
#include "AI/ScriptDevAI/base/CombatAI.h"

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_AGGRO                   = -1309002,                 // 开战 台词广播 ID
    SAY_RAIN_FIRE               = -1309003,                 // rain火焰 台词广播 ID
    SAY_SHRIEK                  = -1309026,                 // shriek 台词广播 ID
    SAY_HEAL                    = -1309027,                 // 治疗 台词广播 ID
    SAY_DEATH                   = -1309004,                 // 死亡 台词广播 ID
    SAY_SELF_DETONATE           = -1309028,                 // selfdetonate 台词广播 ID

    // Bat spells
    SPELL_CHARGE                = 22911,                    // 冲锋 法术 ID
    SPELL_SONIC_BURST           = 23918,                    // sonicburst 法术 ID
    SPELL_SWOOP                 = 23919,                    // swoop 法术 ID
    SPELL_ROOT_SELF             = 23973,                    // 缠绕self 法术 ID
    SPELL_SUMMON_FRENZIED_BATS  = 23974,                    // 召唤frenziedbats 法术 ID
    SPELL_BLOOD_LEECH           = 22644,                    // 鲜血leech 法术 ID
    SPELL_PIERCE_ARMOR          = 12097,                    // piercearmor 法术 ID

    // Troll form spells
    SPELL_SHADOW_WORD_PAIN      = 23952,                    // 暗影wordpain 法术 ID
    SPELL_MIND_FLAY             = 23953,                    // mind鞭笞 法术 ID
    SPELL_GREATER_HEAL          = 23954,                    // greater治疗 法术 ID

    // SPELL_PSYHIC_SCREAM      = 22884,                   // spell not confirmed - needs research
    // curse of blood also unconfirmed

    // Common spells
    SPELL_GREEN_CHANNELING      = 13540,                    // visual for idle mode
    SPELL_BAT_FORM              = 23966,                    // batform 法术 ID
    SPELL_TRANSFORM_VISUAL      = 24085,                    // 变形视觉特效 法术 ID

    // Batriders Spell (Batriders法术)
    SPELL_LIQUID_FIRE           = 23968,                    // script effect - triggers 23971,
    SPELL_UNSTABLE_CONCOCTION   = 24024,                    // unstableconcoction 法术 ID
    SPELL_THRASH                = 8876,                     // thrash 法术 ID
    SPELL_DEMORALIZING_SHOUT    = 23511,                    // demoralizingshout 法术 ID
    SPELL_BATTLE_COMMAND        = 5115,                     // battlecommand 法术 ID
    SPELL_INFECTED_BITE         = 16128,                    // infectedbite 法术 ID

    // npcs (相关 NPC)
    NPC_FRENZIED_BAT            = 14965,                    // frenziedbat Entry ID
    NPC_BAT_RIDER               = 14750,                    // batrider Entry ID

    SPELL_LIST_PHASE_1 = 1451701,                           // list阶段1 法术 ID
    SPELL_LIST_PHASE_2 = 1451702,                           // list阶段2 法术 ID
};

// 枚举定义：法术技能、生物与环境事件标识
enum JeklikActions
{
    JEKLIK_PHASE_2,
    JEKLIK_PHASE_BATS,
    JEKLIK_ACTION_MAX,
    JEKLIK_DELAYED_BAT,
};

// 首领战 AI：标识 ID（jeklik）
struct boss_jeklikAI : public CombatAI
{
    boss_jeklikAI(Creature* creature) : CombatAI(creature, JEKLIK_ACTION_MAX), m_instance(static_cast<instance_zulgurub*>(creature->GetInstanceData()))
    {
        AddTimerlessCombatAction(JEKLIK_PHASE_2, true);
        AddTimerlessCombatAction(JEKLIK_PHASE_BATS, true);
        AddCustomAction(JEKLIK_DELAYED_BAT, true, [&]()
        {
            if (Creature* bat = m_creature->GetMap()->GetCreature(m_delayedBat))
            {
                bat->CastSpell(nullptr, SPELL_LIQUID_FIRE, TRIGGERED_OLD_TRIGGERED);
                bat->GetMotionMaster()->MovePath(1);
            }
        });
    }

    ScriptedInstance* m_instance;

    GuidList m_lBombRiderGuidsList;

    ObjectGuid m_delayedBat;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();

        m_creature->SetSpellList(SPELL_LIST_PHASE_1);

        DoCastSpellIfCan(m_creature, SPELL_GREEN_CHANNELING);
        SetCombatMovement(false);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);

        // Note: on aggro the bats from the cave behind the boss should fly outside!
        if (DoCastSpellIfCan(m_creature, SPELL_BAT_FORM) == CAST_OK)
        {
            m_creature->SetLevitate(true);
            // override MMaps, by allowing the boss to fly up from the ledge
            m_creature->SetWalk(false);
            m_creature->GetMotionMaster()->MovePoint(1, -12281.58f, -1392.84f, 146.1f);
        }
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        DoScriptText(SAY_DEATH, m_creature);
        DoDespawnBombRiders();

        if (m_instance)
            m_instance->SetData(TYPE_JEKLIK, DONE);
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        DoDespawnBombRiders();

        if (m_instance)
            m_instance->SetData(TYPE_JEKLIK, FAIL);
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        summoned->SetLevitate(true);
        summoned->SetHover(true);
        if (summoned->GetEntry() == NPC_FRENZIED_BAT)
        {
            summoned->CastSpell(summoned, SPELL_ROOT_SELF, TRIGGERED_NONE);
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                summoned->AI()->AttackStart(pTarget);
        }
        else if (summoned->GetEntry() == NPC_BAT_RIDER)
        {
            summoned->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
            m_lBombRiderGuidsList.push_back(summoned->GetObjectGuid());
        }
    }

    // 进入脱战闪避模式
    void EnterEvadeMode() override
    {
        // Override MMaps, and teleport to original position
        float fX, fY, fZ, fO;
        m_creature->GetRespawnCoord(fX, fY, fZ, &fO);
        m_creature->NearTeleportTo(fX, fY, fZ, fO);

        ScriptedAI::EnterEvadeMode();
    }

    // 移动路径点到达回调
    void MovementInform(uint32 uiMoveType, uint32 uiPointId) override
    {
        if (uiMoveType != POINT_MOTION_TYPE || !uiPointId)
            return;

        SetCombatMovement(true);
        DoStartMovement(m_creature->GetVictim());
    }

    // Wrapper to despawn the bomb riders on evade / death
    void DoDespawnBombRiders()
    {
        if (m_lBombRiderGuidsList.empty())
            return;

        for (GuidList::const_iterator itr = m_lBombRiderGuidsList.begin(); itr != m_lBombRiderGuidsList.end(); ++itr)
        {
            if (Creature* pRider = m_creature->GetMap()->GetCreature(*itr))
                pRider->ForcedDespawn();
        }
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case JEKLIK_PHASE_2:
                // Phase Switch at 50%
                if (m_creature->GetHealthPercent() < 50.0f)
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_TRANSFORM_VISUAL) == CAST_OK)
                    {
                        m_creature->RemoveAurasDueToSpell(SPELL_BAT_FORM);
                        m_creature->SetLevitate(false);
                        DoResetThreat();
                        DisableCombatAction(action);
                        m_creature->SetSpellList(SPELL_LIST_PHASE_2);
                    }                    
                }
                break;
            case JEKLIK_PHASE_BATS:
                if (m_creature->GetHealthPercent() < 35.0f)
                {
                    // TODO: check if more positions exist
                    const std::vector<Position> positions =
                    {
                        {-12298.03f, -1368.506f, 145.3976f, 4.799655f}, // waits 15 sec
                        {-12301.69f, -1371.292f, 145.0924f, 4.747295f},
                    };

                    Position const& pos1 = positions[0];
                    Creature* bat = m_creature->SummonCreature(NPC_BAT_RIDER, pos1.x, pos1.y, pos1.z, 0, TEMPSPAWN_DEAD_DESPAWN, 0);
                    bat->GetMotionMaster()->MoveIdle();
                    m_delayedBat = bat->GetObjectGuid();
                    ResetTimer(JEKLIK_DELAYED_BAT, 15000);

                    Position const& pos2 = positions[1];
                    bat = m_creature->SummonCreature(NPC_BAT_RIDER, pos2.x, pos2.y, pos2.z, 0, TEMPSPAWN_DEAD_DESPAWN, 0);
                    bat->CastSpell(nullptr, SPELL_LIQUID_FIRE, TRIGGERED_OLD_TRIGGERED);
                    bat->GetMotionMaster()->MovePath(1);

                    DoScriptText(SAY_RAIN_FIRE, m_creature);
                    DisableCombatAction(action);
                }
                break;
        }
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        if (spellInfo->Id == SPELL_SUMMON_FRENZIED_BATS)
            DoScriptText(SAY_SHRIEK, m_creature);
        else if (spellInfo->Id == SPELL_GREATER_HEAL)
            DoScriptText(SAY_HEAL, m_creature);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum BatRiderActions
{
    BAT_RIDER_UNSTABLE_CONCOCTION,
    BAT_RIDER_ACTION_MAX,
};

// 随从/NPC AI：batrider（gurubashi_bat_rider）
struct npc_gurubashi_bat_riderAI : public CombatAI
{
    npc_gurubashi_bat_riderAI(Creature* pCreature) : CombatAI(pCreature, BAT_RIDER_ACTION_MAX)
    {
        m_bIsSummon = m_creature->IsTemporarySummon();
        if (m_bIsSummon)
            SetReactState(REACT_PASSIVE);
        AddTimerlessCombatAction(BAT_RIDER_UNSTABLE_CONCOCTION, true);
    }

    bool m_bIsSummon;                                       // IsSummon 状态标记
    bool m_bHasDoneConcoction;                              // HasDoneConcoction 状态标记

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        CombatAI::Reset();

        m_bHasDoneConcoction = false;

        DoCastSpellIfCan(nullptr, SPELL_THRASH, CAST_TRIGGERED | CAST_AURA_NOT_PRESENT);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        // Don't attack if is summoned by Jeklik - the npc gets aggro because of the Liquid Fire
        if (m_bIsSummon)
            return;

        DoCastSpellIfCan(m_creature, SPELL_DEMORALIZING_SHOUT);
        // For normal mobs flag needs to be removed
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
    }

    void ExecuteAction(uint32 action) override
    {
        switch (action)
        {
            case BAT_RIDER_UNSTABLE_CONCOCTION:
                if (m_creature->GetHealthPercent() < 40.0f)
                {
                    if (DoCastSpellIfCan(nullptr, SPELL_UNSTABLE_CONCOCTION) == CAST_OK)
                    {
                        DoScriptText(SAY_SELF_DETONATE, m_creature);
                        DisableCombatAction(action);
                    }
                }
                break;
        }
    }
};

// 注册并载入脚本模块：boss_jeklik
void AddSC_boss_jeklik()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_jeklik";
    pNewScript->GetAI = &GetNewAIInstance<boss_jeklikAI>;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_gurubashi_bat_rider";
    pNewScript->GetAI = &GetNewAIInstance<npc_gurubashi_bat_riderAI>;
    pNewScript->RegisterSelf();
}
