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
SDName: Burning_Steppes
SD%Complete: 100
SDComment: Quest support: 4121, 4122
SDCategory: Burning Steppes
EndScriptData */

// 燃烧平原（Burning Steppes）区域杂项与护送任务脚本

/* ContentData
npc_grark_lorkrub
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

/*######
## npc_grark_lorkrub
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_START                   = -1000873,                 // 开始 台词广播 ID
    SAY_PAY                     = -1000874,                 // pay 台词广播 ID
    SAY_FIRST_AMBUSH_START      = -1000875,                 // firstambush开始 台词广播 ID
    SAY_FIRST_AMBUSH_END        = -1000876,                 // firstambushend 台词广播 ID
    SAY_SEC_AMBUSH_START        = -1000877,                 // secambush开始 台词广播 ID
    SAY_SEC_AMBUSH_END          = -1000878,                 // secambushend 台词广播 ID
    SAY_THIRD_AMBUSH_START      = -1000879,                 // thirdambush开始 台词广播 ID
    SAY_THIRD_AMBUSH_END        = -1000880,                 // thirdambushend 台词广播 ID
    EMOTE_LAUGH                 = -1000881,                 // laugh 表情动作 ID
    SAY_LAST_STAND              = -1000882,                 // laststand 台词广播 ID
    SAY_LEXLORT_1               = -1000883,                 // lexlort1 台词广播 ID
    SAY_LEXLORT_2               = -1000884,                 // lexlort2 台词广播 ID
    EMOTE_RAISE_AXE             = -1000885,                 // raiseaxe 表情动作 ID
    EMOTE_LOWER_HAND            = -1000886,                 // lowerhand 表情动作 ID
    SAY_LEXLORT_3               = -1000887,                 // lexlort3 台词广播 ID
    SAY_LEXLORT_4               = -1000888,                 // lexlort4 台词广播 ID

    EMOTE_SUBMIT                = -1000889,                 // submit 表情动作 ID
    SAY_AGGRO                   = -1000890,                 // 开战 台词广播 ID

    SPELL_CAPTURE_GRARK             = 14250,                // 捕获grark 法术 ID

    NPC_BLACKROCK_AMBUSHER          = 9522,                 // blackrockambusher Entry ID
    NPC_BLACKROCK_RAIDER            = 9605,                 // blackrockraider Entry ID
    NPC_FLAMESCALE_DRAGONSPAWN      = 7042,                 // flamescaledragonspawn Entry ID
    NPC_SEARSCALE_DRAKE             = 7046,                 // searscale幼龙 Entry ID

    NPC_GRARK_LORKRUB               = 9520,                 // grarklorkrub Entry ID
    NPC_HIGH_EXECUTIONER_NUZARK     = 9538,                 // highexecutionernuzark Entry ID
    NPC_SHADOW_OF_LEXLORT           = 9539,                 // 暗影oflexlort Entry ID

    FACTION_GRARK_FRIENDLY          = 29,                   // grarkfriendly 阵营 ID

    QUEST_ID_PRECARIOUS_PREDICAMENT = 4121                  // IDprecariouspredicament 任务 ID
};

static const DialogueEntry aOutroDialogue[] =
{
    {SAY_LAST_STAND,    NPC_GRARK_LORKRUB,              5000},
    {SAY_LEXLORT_1,     NPC_SHADOW_OF_LEXLORT,          3000},
    {SAY_LEXLORT_2,     NPC_SHADOW_OF_LEXLORT,          5000},
    {EMOTE_RAISE_AXE,   NPC_HIGH_EXECUTIONER_NUZARK,    4000},
    {EMOTE_LOWER_HAND,  NPC_SHADOW_OF_LEXLORT,          3000},
    {SAY_LEXLORT_3,     NPC_SHADOW_OF_LEXLORT,          3000},
    {NPC_GRARK_LORKRUB, 0,                              5000},
    {SAY_LEXLORT_4,     NPC_SHADOW_OF_LEXLORT,          0},
    {0, 0, 0},
};

// 随从/NPC AI：lorkrub（grark_lorkrub）
struct npc_grark_lorkrubAI : public npc_escortAI, private DialogueHelper
{
    npc_grark_lorkrubAI(Creature* pCreature) : npc_escortAI(pCreature), DialogueHelper(aOutroDialogue), m_shackled(false)
    {
        Reset();
    }

    ObjectGuid m_nuzarkGuid;                                // nuzark 实体 GUID
    ObjectGuid m_lexlortGuid;                               // lexlort 实体 GUID

    GuidList m_lSearscaleGuidList;

    uint8 m_uiKilledCreatures;
    bool m_bIsFirstSearScale;                               // IsFirstSearScale 状态标记
    bool m_shackled;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        if (!HasEscortState(STATE_ESCORT_ESCORTING))
        {
            m_uiKilledCreatures = 0;
            m_bIsFirstSearScale = true;

            m_lSearscaleGuidList.clear();

            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
        }
        SetDeathPrevention(false);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (!HasEscortState(STATE_ESCORT_ESCORTING))
            DoScriptText(SAY_AGGRO, m_creature);
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        // No combat during escort
        if (HasEscortState(STATE_ESCORT_ESCORTING))
            return;

        npc_escortAI::MoveInLineOfSight(pWho);
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*pSender*/, Unit* /*pInvoker*/, uint32 /*uiMiscValue*/) override
    {
        // rejoin head on request (响应请求头部回归身体)
        if (eventType == AI_EVENT_CUSTOM_A)
        {
            m_shackled = true;
            SetDeathPrevention(true);
        }
    }

    void HandleLowHp()
    {
        DoScriptText(EMOTE_SUBMIT, m_creature);
        static_cast<Creature*>(m_creature)->SetFactionTemporary(FACTION_GRARK_FRIENDLY, TEMPFACTION_RESTORE_RESPAWN);
        EnterEvadeMode();
        SetDeathPrevention(false);
        m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 1:
                DoScriptText(SAY_START, m_creature);
                break;
            case 7:
                DoScriptText(SAY_PAY, m_creature);
                break;
            case 12:
                DoScriptText(SAY_FIRST_AMBUSH_START, m_creature);
                SetEscortPaused(true);

                m_creature->SummonCreature(NPC_BLACKROCK_AMBUSHER, -7844.3f, -1521.6f, 139.2f, 0.0f, TEMPSPAWN_TIMED_OOC_DESPAWN, 20000);
                m_creature->SummonCreature(NPC_BLACKROCK_AMBUSHER, -7860.4f, -1507.8f, 141.0f, 6.0f, TEMPSPAWN_TIMED_OOC_DESPAWN, 20000);
                m_creature->SummonCreature(NPC_BLACKROCK_RAIDER,   -7845.6f, -1508.1f, 138.8f, 6.1f, TEMPSPAWN_TIMED_OOC_DESPAWN, 20000);
                m_creature->SummonCreature(NPC_BLACKROCK_RAIDER,   -7859.8f, -1521.8f, 139.2f, 6.2f, TEMPSPAWN_TIMED_OOC_DESPAWN, 20000);
                break;
            case 24:
                DoScriptText(SAY_SEC_AMBUSH_START, m_creature);
                SetEscortPaused(true);

                m_creature->SummonCreature(NPC_BLACKROCK_AMBUSHER,     -8035.3f, -1222.2f, 135.5f, 5.1f, TEMPSPAWN_TIMED_OOC_DESPAWN, 20000);
                m_creature->SummonCreature(NPC_FLAMESCALE_DRAGONSPAWN, -8037.5f, -1216.9f, 135.8f, 5.1f, TEMPSPAWN_TIMED_OOC_DESPAWN, 20000);
                m_creature->SummonCreature(NPC_BLACKROCK_AMBUSHER,     -8009.5f, -1222.1f, 139.2f, 3.9f, TEMPSPAWN_TIMED_OOC_DESPAWN, 20000);
                m_creature->SummonCreature(NPC_FLAMESCALE_DRAGONSPAWN, -8007.1f, -1219.4f, 140.1f, 3.9f, TEMPSPAWN_TIMED_OOC_DESPAWN, 20000);
                break;
            case 28:
                m_creature->SummonCreature(NPC_SEARSCALE_DRAKE, -7897.8f, -1123.1f, 233.4f, 3.0f, TEMPSPAWN_TIMED_OOC_DESPAWN, 60000);
                m_creature->SummonCreature(NPC_SEARSCALE_DRAKE, -7898.8f, -1125.1f, 193.9f, 3.0f, TEMPSPAWN_TIMED_OOC_DESPAWN, 60000);
                m_creature->SummonCreature(NPC_SEARSCALE_DRAKE, -7895.6f, -1119.5f, 194.5f, 3.1f, TEMPSPAWN_TIMED_OOC_DESPAWN, 60000);
                break;
            case 30:
            {
                SetEscortPaused(true);
                DoScriptText(SAY_THIRD_AMBUSH_START, m_creature);

                Player* pPlayer = GetPlayerForEscort();
                if (!pPlayer)
                    return;

                // Set all the dragons in combat
                for (GuidList::const_iterator itr = m_lSearscaleGuidList.begin(); itr != m_lSearscaleGuidList.end(); ++itr)
                {
                    if (Creature* pTemp = m_creature->GetMap()->GetCreature(*itr))
                        pTemp->AI()->AttackStart(pPlayer);
                }
                break;
            }
            case 36:
                DoScriptText(EMOTE_LAUGH, m_creature);
                break;
            case 45:
                StartNextDialogueText(SAY_LAST_STAND);
                SetEscortPaused(true);

                m_creature->SummonCreature(NPC_HIGH_EXECUTIONER_NUZARK, -7532.3f, -1029.4f, 258.0f, 2.7f, TEMPSPAWN_TIMED_DESPAWN, 40000);
                m_creature->SummonCreature(NPC_SHADOW_OF_LEXLORT,       -7532.8f, -1032.9f, 258.2f, 2.5f, TEMPSPAWN_TIMED_DESPAWN, 40000);
                break;
        }
    }

    void JustDidDialogueStep(int32 iEntry) override
    {
        switch (iEntry)
        {
            case SAY_LEXLORT_1:
                m_creature->SetStandState(UNIT_STAND_STATE_KNEEL);
                break;
            case SAY_LEXLORT_3:
                // Note: this part isn't very clear. Should he just simply attack him, or charge him?
                if (Creature* pNuzark = m_creature->GetMap()->GetCreature(m_nuzarkGuid))
                    pNuzark->HandleEmote(EMOTE_ONESHOT_ATTACK2HTIGHT);
                break;
            case NPC_GRARK_LORKRUB:
                // Fake death creature when the axe is lowered. This will allow us to finish the event
                m_creature->InterruptNonMeleeSpells(true);
                m_creature->StopMoving();
                m_creature->ClearComboPointHolders();
                m_creature->RemoveAllAurasOnDeath();
                m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);
                m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, false);
                m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
                m_creature->ClearAllReactives();
                m_creature->GetMotionMaster()->Clear();
                m_creature->GetMotionMaster()->MoveIdle();
                m_creature->SetStandState(UNIT_STAND_STATE_DEAD);
                break;
            case SAY_LEXLORT_4:
                // Finish the quest
                if (Player* pPlayer = GetPlayerForEscort())
                    pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_ID_PRECARIOUS_PREDICAMENT, m_creature);
                // Kill self (击杀self)
                m_creature->Suicide();
                break;
        }
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* pSummoned) override
    {
        switch (pSummoned->GetEntry())
        {
            case NPC_HIGH_EXECUTIONER_NUZARK: m_nuzarkGuid  = pSummoned->GetObjectGuid(); break;
            case NPC_SHADOW_OF_LEXLORT:       m_lexlortGuid = pSummoned->GetObjectGuid(); break;
            case NPC_SEARSCALE_DRAKE:
                // If it's the flying drake allow him to move in circles
                if (m_bIsFirstSearScale)
                {
                    m_bIsFirstSearScale = false;

                    pSummoned->SetLevitate(true);
                    // ToDo: this guy should fly in circles above the creature
                }
                m_lSearscaleGuidList.push_back(pSummoned->GetObjectGuid());
                break;

            default:
                // The hostile mobs should attack the player only
                if (Player* pPlayer = GetPlayerForEscort())
                    pSummoned->AI()->AttackStart(pPlayer);
                break;
        }
    }

    void SummonedCreatureJustDied(Creature* /*pSummoned*/) override
    {
        ++m_uiKilledCreatures;

        switch (m_uiKilledCreatures)
        {
            case 4:
                DoScriptText(SAY_FIRST_AMBUSH_END, m_creature);
                SetEscortPaused(false);
                break;
            case 8:
                DoScriptText(SAY_SEC_AMBUSH_END, m_creature);
                SetEscortPaused(false);
                break;
            case 11:
                DoScriptText(SAY_THIRD_AMBUSH_END, m_creature);
                SetEscortPaused(false);
                break;
        }
    }

    Creature* GetSpeakerByEntry(uint32 uiEntry) override
    {
        switch (uiEntry)
        {
            case NPC_GRARK_LORKRUB:           return m_creature;
            case NPC_HIGH_EXECUTIONER_NUZARK: return m_creature->GetMap()->GetCreature(m_nuzarkGuid);
            case NPC_SHADOW_OF_LEXLORT:       return m_creature->GetMap()->GetCreature(m_lexlortGuid);

            default:
                return nullptr;
        }
    }

    void UpdateEscortAI(const uint32 diff) override
    {
        DialogueUpdate(diff);
        CreatureAI::UpdateAI(diff);
        if (m_shackled && m_creature->GetHealthPercent() <= 25.f)
            HandleLowHp();
    }
};

// 创建并返回 AI 实例工厂函数：npc_grark_lorkrub
UnitAI* GetAI_npc_grark_lorkrub(Creature* pCreature)
{
    return new npc_grark_lorkrubAI(pCreature);
}

// 任务接取回调函数：npc_grark_lorkrub
bool QuestAccept_npc_grark_lorkrub(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_ID_PRECARIOUS_PREDICAMENT)
    {
        if (npc_grark_lorkrubAI* pEscortAI = dynamic_cast<npc_grark_lorkrubAI*>(pCreature->AI()))
            pEscortAI->Start(false, pPlayer, pQuest);

        return true;
    }

    return false;
}

// 14250 - Capture Grark (14250 - 捕获 Grark)
struct CaptureGrark : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* target = spell->GetUnitTarget();
        target->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, spell->GetCaster(), target);
    }
};

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SPELL_FOOLS_PLIGHT          = 23504,                    // foolsplight 法术 ID

    SPELL_DEMONIC_FRENZY        = 23257,                    // demonic狂乱 法术 ID
    SPELL_ENTROPIC_STING        = 23260,                    // entropic钉刺 法术 ID

    EMOTE_FRENZY                = -1000001,                 // 狂乱 表情动作 ID

    NPC_NELSON_THE_NICE         = 14529,                    // nelsonthenice Entry ID
    NPC_KLINFRAN_THE_CRAZED     = 14534,                    // klinfranthecrazed Entry ID
    NPC_THE_CLEANER             = 14503,                    // thecleaner Entry ID

    QUEST_STAVE_OF_THE_ANCIENTS = 7636                      // staveoftheancients 任务 ID
};

#define GOSSIP_ITEM                 "Show me your real face, demon."

/*######
## npc_franklin_the_friendly
######*/

/*######
## npc_klinfran_the_crazed
######*/

// 随从/NPC AI：标识 ID（klinfran）
struct npc_klinfranAI : public ScriptedAI
{
    npc_klinfranAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_bTransform = false;
        m_uiDespawn_Timer = 0;
        Reset();
    }

    uint32 m_uiTransform_Timer;                             // Transform_ 技能计时器
    uint32 m_uiTransformEmote_Timer;                        // TransformEmote_ 技能计时器
    bool m_bTransform;                                      // Transform 状态标记

    ObjectGuid m_hunterGuid;                                // hunter 实体 GUID
    uint32 m_uiDemonic_Frenzy_Timer;                        // Demonic_Frenzy_ 技能计时器
    uint32 m_uiDespawn_Timer;                               // Despawn_ 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        switch (m_creature->GetEntry())
        {
            case NPC_NELSON_THE_NICE:
                m_creature->SetRespawnDelay(35 * MINUTE);
                m_creature->SetRespawnTime(35 * MINUTE);
                m_creature->NearTeleportTo(-8318.19f, -993.662f, 176.956f, 5.65024f);
                if (m_creature->GetMotionMaster()->GetCurrentMovementGeneratorType() != WAYPOINT_MOTION_TYPE)
                {
                    m_creature->SetDefaultMovementType(WAYPOINT_MOTION_TYPE);
                    m_creature->GetMotionMaster()->Initialize();
                }

                m_creature->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

                m_uiTransform_Timer = 10000;
                m_uiTransformEmote_Timer = 5000;
                m_bTransform = false;
                m_uiDespawn_Timer = 0;
                break;
            case NPC_KLINFRAN_THE_CRAZED:
                if (!m_uiDespawn_Timer)
                    m_uiDespawn_Timer = 20 * MINUTE*IN_MILLISECONDS;

                m_hunterGuid.Clear();
                m_uiDemonic_Frenzy_Timer = 5000;
                break;
        }
    }

    /** Franklin the Friendly */
    void Transform()
    {
        m_creature->UpdateEntry(NPC_KLINFRAN_THE_CRAZED);
        Reset();
    }

    void BeginEvent(ObjectGuid playerGuid)
    {
        m_hunterGuid = playerGuid;
        m_creature->GetMotionMaster()->Clear(false);
        m_creature->GetMotionMaster()->MoveIdle();
        m_creature->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
        m_bTransform = true;
    }

    /** Klinfran the Crazed */
    // 进入战斗回调：记录开战状态并启动技能循环
    void Aggro(Unit* pWho) override
    {
        if (pWho->getClass() == CLASS_HUNTER && (m_hunterGuid.IsEmpty() || m_hunterGuid == pWho->GetObjectGuid()))
        {
            m_hunterGuid = pWho->GetObjectGuid();
        }
        else
            DemonDespawn();
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        uint32 m_respawn_delay_Timer = 2 * HOUR;
        m_creature->SetRespawnDelay(m_respawn_delay_Timer);
        m_creature->SetRespawnTime(m_respawn_delay_Timer);
        m_creature->SaveRespawnTime();
    }

    void DemonDespawn(bool triggered = true)
    {
        m_creature->SetRespawnDelay(15 * MINUTE);
        m_creature->SetRespawnTime(15 * MINUTE);
        m_creature->SaveRespawnTime();

        if (triggered)
        {
            Creature* pCleaner = m_creature->SummonCreature(NPC_THE_CLEANER, m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), m_creature->GetAngle(m_creature), TEMPSPAWN_DEAD_DESPAWN, 20 * MINUTE*IN_MILLISECONDS);
            if (pCleaner)
            {
                ThreatList const& tList = m_creature->getThreatManager().getThreatList();

                for (auto itr : tList)
                {
                    if (Unit* pUnit = m_creature->GetMap()->GetUnit(itr->getUnitGuid()))
                    {
                        if (pUnit->IsAlive())
                            pCleaner->AI()->AttackStart(pUnit);
                    }
                }
            }
        }

        m_creature->ForcedDespawn();
    }

    void SpellHit(Unit* /*pCaster*/, const SpellEntry* pSpell) override
    {
        if (pSpell && pSpell->Id == 14277)   // Scorpid Sting (Rank 4)
        {
            m_creature->RemoveAurasDueToSpell(SPELL_DEMONIC_FRENZY);
            DoCastSpellIfCan(m_creature, SPELL_ENTROPIC_STING, CAST_TRIGGERED);
        }
    }

    // 战斗 AI 逻辑轮询更新
    void UpdateAI(const uint32 uiDiff) override
    {
        /** Franklin the Friendly */
        if (m_bTransform)
        {
            if (m_uiTransformEmote_Timer)
            {
                if (m_uiTransformEmote_Timer <= uiDiff)
                {
                    m_creature->HandleEmote(EMOTE_ONESHOT_POINT);
                    m_uiTransformEmote_Timer = 0;
                }
                else
                    m_uiTransformEmote_Timer -= uiDiff;
            }

            if (m_uiTransform_Timer < uiDiff)
            {
                m_bTransform = false;
                Transform();
            }
            else
                m_uiTransform_Timer -= uiDiff;
        }

        /** Klinfran the Crazed */
        if (m_uiDespawn_Timer)
        {
            if (m_uiDespawn_Timer <= uiDiff)
            {
                if (m_creature->IsAlive() && !m_creature->IsInCombat())
                    DemonDespawn(false);
            }
            else
                m_uiDespawn_Timer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        if (m_creature->getThreatManager().getThreatList().size() > 1 /*|| pHunter->IsDead()*/)
            DemonDespawn();

        if (m_uiDemonic_Frenzy_Timer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_DEMONIC_FRENZY) == CAST_OK)
            {
                DoScriptText(EMOTE_FRENZY, m_creature);
                m_uiDemonic_Frenzy_Timer = 15000;
            }
        }
        else
            m_uiDemonic_Frenzy_Timer -= uiDiff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// NPC 对话交互入口函数：npc_klinfran
bool GossipHello_npc_klinfran(Player* pPlayer, Creature* pCreature)
{
    if (pPlayer->GetQuestStatus(QUEST_STAVE_OF_THE_ANCIENTS) == QUEST_STATUS_INCOMPLETE)
        pPlayer->ADD_GOSSIP_ITEM(0, GOSSIP_ITEM, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);

    pPlayer->SEND_GOSSIP_MENU(pPlayer->GetGossipTextId(pCreature), pCreature->GetObjectGuid());
    return true;
}

bool GossipSelect_npc_klinfran(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 /*uiAction*/)
{
    pPlayer->CLOSE_GOSSIP_MENU();
    ((npc_klinfranAI*)pCreature->AI())->BeginEvent(pPlayer->GetObjectGuid());
    return true;
}

// 创建并返回 AI 实例工厂函数：npc_klinfran
UnitAI* GetAI_npc_klinfran(Creature* pCreature)
{
    return new npc_klinfranAI(pCreature);
}

// 注册并载入脚本模块：burning_steppes
void AddSC_burning_steppes()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_grark_lorkrub";
    pNewScript->GetAI = &GetAI_npc_grark_lorkrub;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_grark_lorkrub;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_klinfran";
    pNewScript->GetAI = &GetAI_npc_klinfran;
    pNewScript->pGossipHello = &GossipHello_npc_klinfran;
    pNewScript->pGossipSelect = &GossipSelect_npc_klinfran;
    pNewScript->RegisterSelf();

    RegisterSpellScript<CaptureGrark>("spell_capture_grark");
}
