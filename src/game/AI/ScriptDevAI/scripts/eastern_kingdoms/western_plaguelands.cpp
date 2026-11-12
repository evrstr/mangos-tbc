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
SDName: Western_Plaguelands
SD%Complete: 90
SDComment: Quest support: 5216, 5219, 5222, 5225, 5229, 5231, 5233, 5235, 5862, 5944, 9446.
SDCategory: Western Plaguelands
EndScriptData */

// 西瘟疫之地（Western Plaguelands）安多哈尔与四大瘟疫大锅任务脚本

/* ContentData
npc_the_scourge_cauldron
npc_anchorite_truuen
npc_taelan_fordring
npc_isillien
npc_tirion_fordring
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"
#include "Spells/Scripts/SpellScript.h"

/*######
## npc_the_scourge_cauldron
######*/

// 随从/NPC AI：scourgecauldron（the_scourge_cauldron）
struct npc_the_scourge_cauldronAI : public ScriptedAI
{
    npc_the_scourge_cauldronAI(Creature* pCreature) : ScriptedAI(pCreature) {Reset();}

    // 重置战斗状态与技能计时器
    void Reset() override {}

    void DoDie()
    {
        // summoner dies here - TODO: Check suicide spell
        m_creature->Suicide();
        // override any database `spawntimesecs` to prevent duplicated summons
        uint32 rTime = m_creature->GetRespawnDelay();
        if (rTime < 600)
            m_creature->SetRespawnDelay(600);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!who || who->GetTypeId() != TYPEID_PLAYER)
            return;

        if (who->GetTypeId() == TYPEID_PLAYER)
        {
            switch (m_creature->GetAreaId())
            {
                case 199:                                   // felstone
                    if (((Player*)who)->GetQuestStatus(5216) == QUEST_STATUS_INCOMPLETE ||
                            ((Player*)who)->GetQuestStatus(5229) == QUEST_STATUS_INCOMPLETE)
                    {
                        m_creature->SummonCreature(11075, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 600000);
                        DoDie();
                    }
                    break;
                case 200:                                   // dalson
                    if (((Player*)who)->GetQuestStatus(5219) == QUEST_STATUS_INCOMPLETE ||
                            ((Player*)who)->GetQuestStatus(5231) == QUEST_STATUS_INCOMPLETE)
                    {
                        m_creature->SummonCreature(11077, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 600000);
                        DoDie();
                    }
                    break;
                case 201:                                   // gahrron
                    if (((Player*)who)->GetQuestStatus(5225) == QUEST_STATUS_INCOMPLETE ||
                            ((Player*)who)->GetQuestStatus(5235) == QUEST_STATUS_INCOMPLETE)
                    {
                        m_creature->SummonCreature(11078, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 600000);
                        DoDie();
                    }
                    break;
                case 202:                                   // writhing
                    if (((Player*)who)->GetQuestStatus(5222) == QUEST_STATUS_INCOMPLETE ||
                            ((Player*)who)->GetQuestStatus(5233) == QUEST_STATUS_INCOMPLETE)
                    {
                        m_creature->SummonCreature(11076, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 600000);
                        DoDie();
                    }
                    break;
            }
        }
    }
};

// 创建并返回 AI 实例工厂函数：npc_the_scourge_cauldron
UnitAI* GetAI_npc_the_scourge_cauldron(Creature* creature)
{
    return new npc_the_scourge_cauldronAI(creature);
}

/*######
## npc_anchorite_truuen
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_BEGIN                   = -1000910,                 // begin 台词广播 ID
    SAY_FIRST_STOP              = -1000911,                 // firststop 台词广播 ID
    SAY_CONTINUE                = -1000912,                 // continue 台词广播 ID
    SAY_FIRST_ATTACK            = -1000913,                 // first攻击 台词广播 ID
    SAY_PURITY                  = -1000914,                 // purity 台词广播 ID
    SAY_SECOND_ATTACK           = -1000915,                 // second攻击 台词广播 ID
    SAY_CLEANSE                 = -1000916,                 // cleanse 台词广播 ID
    SAY_WELCOME                 = -1000917,                 // welcome 台词广播 ID
    SAY_EPILOGUE_1              = -1000918,                 // epilogue1 台词广播 ID
    SAY_EPILOGUE_2              = -1000919,                 // epilogue2 台词广播 ID

    NPC_PRIEST_THELDANIS        = 1854,                     // 牧师theldanis Entry ID
    NPC_HUNGERING_WRAITH        = 1802,                     // hungeringwraith Entry ID
    NPC_HAUNDING_VISION         = 4472,                     // haundingvision Entry ID
    NPC_BLIGHTED_ZOMBIE         = 4475,                     // blighted僵尸 Entry ID
    NPC_GHOST_OF_UTHER          = 17233,                    // ghostofuther Entry ID

    QUEST_ID_TOMB_LIGHTBRINGER  = 9446,                     // ID墓穴lightbringer 任务 ID
};

// 随从/NPC AI：truuen（anchorite_truuen）
struct npc_anchorite_truuenAI: public npc_escortAI
{
    npc_anchorite_truuenAI(Creature* pCreature): npc_escortAI(pCreature) { Reset(); }

    ObjectGuid m_utherGhostGuid;                            // utherGhost 实体 GUID

    // 重置战斗状态与技能计时器
    void Reset() override { }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*pSender*/, Unit* pInvoker, uint32 uiMiscValue) override
    {
        if (eventType == AI_EVENT_START_ESCORT && pInvoker->GetTypeId() == TYPEID_PLAYER)
        {
            DoScriptText(SAY_BEGIN, m_creature);
            Start(false, (Player*)pInvoker, GetQuestTemplateStore(uiMiscValue));
        }
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 5:
                DoScriptText(SAY_FIRST_STOP, m_creature);
                break;
            case 6:
                DoScriptText(SAY_CONTINUE, m_creature);
                break;
            case 11:
                DoScriptText(SAY_FIRST_ATTACK, m_creature);
                // spawn first attacker wave
                m_creature->SummonCreature(NPC_HAUNDING_VISION, 1045.26f, -1576.50f, 62.42f, 2.82f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 30000);
                m_creature->SummonCreature(NPC_HUNGERING_WRAITH, 1021.74f, -1547.49f, 63.44f, 5.24f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 30000);
                break;
            case 12:
                DoScriptText(SAY_PURITY, m_creature);
                break;
            case 22:
                DoScriptText(SAY_SECOND_ATTACK, m_creature);
                // spawn second attacker wave
                m_creature->SummonCreature(NPC_BLIGHTED_ZOMBIE, 1123.08f, -1738.70f, 61.65f, 3.63f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 30000);
                m_creature->SummonCreature(NPC_BLIGHTED_ZOMBIE, 1117.07f, -1763.47f, 62.72f, 1.83f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 30000);
                m_creature->SummonCreature(NPC_BLIGHTED_ZOMBIE, 1096.79f, -1719.14f, 62.69f, 4.88f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 30000);
                m_creature->SummonCreature(NPC_BLIGHTED_ZOMBIE, 1068.92f, -1739.68f, 62.23f, 6.21f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 30000);
                break;
            case 23:
                DoScriptText(SAY_CLEANSE, m_creature);
                break;
            case 36:
                if (Creature* pPriest = GetClosestCreatureWithEntry(m_creature, NPC_PRIEST_THELDANIS, 60.0f))
                    DoScriptText(SAY_WELCOME, pPriest);
                break;
            case 39:
                m_creature->SetStandState(UNIT_STAND_STATE_KNEEL);
                m_creature->SummonCreature(NPC_GHOST_OF_UTHER, 972.96f, -1824.82f, 82.54f, 0.27f, TEMPSPAWN_TIMED_DESPAWN, 45000);
                // complete the quest - the event continues with the dialogue
                if (Player* pPlayer = GetPlayerForEscort())
                    pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_ID_TOMB_LIGHTBRINGER, m_creature);
                break;
            case 40:
                if (Creature* pUther = m_creature->GetMap()->GetCreature(m_utherGhostGuid))
                {
                    pUther->SetFacingToObject(m_creature);
                    DoScriptText(SAY_EPILOGUE_1, pUther);
                }
                break;
            case 41:
                if (Creature* pUther = m_creature->GetMap()->GetCreature(m_utherGhostGuid))
                    DoScriptText(SAY_EPILOGUE_2, pUther);
                break;
            case 42:
                m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                break;
        }
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() != NPC_GHOST_OF_UTHER)
            pSummoned->AI()->AttackStart(m_creature);
        else
            m_utherGhostGuid = pSummoned->GetObjectGuid();
    }
};

// 创建并返回 AI 实例工厂函数：npc_anchorite_truuen
UnitAI* GetAI_npc_anchorite_truuen(Creature* pCreature)
{
    return new npc_anchorite_truuenAI(pCreature);
}

// 任务接取回调函数：npc_anchorite_truuen
bool QuestAccept_npc_anchorite_truuen(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_ID_TOMB_LIGHTBRINGER)
        pCreature->AI()->SendAIEvent(AI_EVENT_START_ESCORT, pPlayer, pCreature, pQuest->GetQuestId());

    return true;
}

/*######
## npc_taelan_fordring
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    // scarlet end quest texts
    SAY_SCARLET_COMPLETE_1          = -1001076,             // scarletcomplete1 台词广播 ID
    SAY_SCARLET_COMPLETE_2          = -1001077,             // scarletcomplete2 台词广播 ID

    // escort text
    SAY_ESCORT_START                = -1001078,             // escort开始 台词广播 ID
    SAY_EXIT_KEEP                   = -1001079,             // exitkeep 台词广播 ID
    EMOTE_MOUNT                     = -1001080,             // 坐骑 表情动作 ID
    SAY_REACH_TOWER                 = -1001081,             // reachtower 台词广播 ID
    SAY_ISILLIEN_1                  = -1001082,             // isillien1 台词广播 ID
    SAY_ISILLIEN_2                  = -1001083,             // isillien2 台词广播 ID
    SAY_ISILLIEN_3                  = -1001084,             // isillien3 台词广播 ID
    SAY_ISILLIEN_4                  = -1001085,             // isillien4 台词广播 ID
    SAY_ISILLIEN_5                  = -1001086,             // isillien5 台词广播 ID
    SAY_ISILLIEN_6                  = -1001087,             // isillien6 台词广播 ID
    EMOTE_ISILLIEN_ATTACK           = -1001088,             // isillien攻击 表情动作 ID
    SAY_ISILLIEN_ATTACK             = -1001089,             // isillien攻击 台词广播 ID
    SAY_KILL_TAELAN_1               = -1001090,             // 击杀泰兰1 台词广播 ID
    EMOTE_ISILLIEN_LAUGH            = -1001091,             // isillienlaugh 表情动作 ID
    SAY_KILL_TAELAN_2               = -1001092,             // 击杀泰兰2 台词广播 ID
    EMOTE_ATTACK_PLAYER             = -1001093,             // 攻击player 表情动作 ID
    SAY_TIRION_1                    = -1001094,             // tirion1 台词广播 ID
    SAY_TIRION_2                    = -1001095,             // tirion2 台词广播 ID
    SAY_TIRION_3                    = -1001096,             // tirion3 台词广播 ID
    SAY_TIRION_4                    = -1001097,             // tirion4 台词广播 ID
    SAY_TIRION_5                    = -1001098,             // tirion5 台词广播 ID
    SAY_EPILOG_1                    = -1001099,             // epilog1 台词广播 ID
    EMOTE_KNEEL                     = -1001100,             // kneel 表情动作 ID
    SAY_EPILOG_2                    = -1001101,             // epilog2 台词广播 ID
    EMOTE_HOLD_TAELAN               = -1001102,             // hold泰兰 表情动作 ID
    SAY_EPILOG_3                    = -1001103,             // epilog3 台词广播 ID
    SAY_EPILOG_4                    = -1001104,             // epilog4 台词广播 ID
    SAY_EPILOG_5                    = -1001105,             // epilog5 台词广播 ID

    // spells (技能法术)
    SPELL_DEVOTION_AURA             = 17232,                // devotion光环 法术 ID
    SPELL_CRUSADER_STRIKE           = 14518,                // crusader打击 法术 ID
    SPELL_HOLY_STRIKE               = 17143,                // 神圣打击 法术 ID
    SPELL_HOLY_CLEAVE               = 18819,                // 神圣顺劈斩 法术 ID
    SPELL_HOLY_LIGHT                = 15493,                // 神圣light 法术 ID
    SPELL_LAY_ON_HANDS              = 17233,                // layonhands 法术 ID
    SPELL_TAELAN_SUFFERING          = 18810,                // 泰兰suffering 法术 ID

    // isillen spells
    SPELL_DOMINATE_MIND             = 14515,                // dominatemind 法术 ID
    SPELL_FLASH_HEAL                = 10917,                // flash治疗 法术 ID
    SPELL_GREATER_HEAL              = 10965,                // greater治疗 法术 ID
    SPELL_MANA_BURN                 = 15800,                // 法力burn 法术 ID
    SPELL_MIND_BLAST                = 17194,                // mind冲击 法术 ID
    SPELL_MIND_FLAY                 = 17165,                // mind鞭笞 法术 ID
    SPELL_TAELAN_DEATH              = 18969,                // 泰兰死亡 法术 ID

    // npcs (相关 NPC)
    NPC_TAELAN_FORDRING             = 1842,                 // 泰兰fordring Entry ID
    NPC_SCARLET_CAVALIER            = 1836,                 // scarletcavalier Entry ID
    NPC_ISILLIEN                    = 1840,                 // isillien Entry ID
    NPC_TIRION_FORDRING             = 12126,                // tirionfordring Entry ID
    NPC_CRIMSON_ELITE               = 12128,                // crimsonelite Entry ID

    MODEL_TAELAN_MOUNT              = 239,                  // 泰兰坐骑 标识 ID

    // quests
    QUEST_ID_SCARLET_SUBTERFUGE     = 5862,                 // IDscarletsubterfuge 任务 ID
    QUEST_ID_IN_DREAMS              = 5944,                 // IDindreams 任务 ID

    TIRION_KNEELS_1                 = 1,                    // kneels1 标识 ID
};

static const int32 aCavalierYells[] = { -1001072, -1001073, -1001074, -1001075 };

static const DialogueEntry aScarletDialogue[] =
{
    // Scarlet Subterfuge ending dialogue
    {NPC_SCARLET_CAVALIER,          0,                          3000},
    {QUEST_ID_SCARLET_SUBTERFUGE,   0,                          7000},
    {SAY_SCARLET_COMPLETE_1,        NPC_TAELAN_FORDRING,        2000},
    {QUEST_ID_IN_DREAMS,            0,                          0},
    {SPELL_DEVOTION_AURA,           0,                          3000},
    {SAY_SCARLET_COMPLETE_2,        NPC_TAELAN_FORDRING,        0},
    // In Dreams event dialogue
    {SAY_EXIT_KEEP,                 NPC_TAELAN_FORDRING,        6000},
    {EMOTE_MOUNT,                   NPC_TAELAN_FORDRING,        4000},
    {MODEL_TAELAN_MOUNT,            0,                          0},
    {SAY_REACH_TOWER,               NPC_TAELAN_FORDRING,        2000},
    {SAY_ISILLIEN_1,                NPC_ISILLIEN,               5000},
    {SAY_ISILLIEN_2,                NPC_TAELAN_FORDRING,        4000},
    {SAY_ISILLIEN_3,                NPC_TAELAN_FORDRING,        10000},
    {SAY_ISILLIEN_4,                NPC_ISILLIEN,               7000},
    {SAY_ISILLIEN_5,                NPC_ISILLIEN,               5000},
    {SAY_ISILLIEN_6,                NPC_ISILLIEN,               6000},
    {EMOTE_ISILLIEN_ATTACK,         NPC_ISILLIEN,               3000},
    {SAY_ISILLIEN_ATTACK,           NPC_ISILLIEN,               3000},
    {SPELL_CRUSADER_STRIKE,         0,                          0},
    {SAY_KILL_TAELAN_1,             NPC_ISILLIEN,               1000},
    {EMOTE_ISILLIEN_LAUGH,          NPC_ISILLIEN,               4000},
    {SAY_KILL_TAELAN_2,             NPC_ISILLIEN,               10000},
    {EMOTE_ATTACK_PLAYER,           NPC_ISILLIEN,               0},
    {NPC_TIRION_FORDRING,           0,                          5000},
    {NPC_ISILLIEN,                  0,                          10000},
    {SAY_TIRION_1,                  NPC_TIRION_FORDRING,        4000},
    {SAY_TIRION_2,                  NPC_ISILLIEN,               6000},
    {SAY_TIRION_3,                  NPC_TIRION_FORDRING,        4000},
    {SAY_TIRION_4,                  NPC_TIRION_FORDRING,        3000},
    {SAY_TIRION_5,                  NPC_ISILLIEN,               0},
    {SAY_EPILOG_1,                  NPC_TIRION_FORDRING,        3000},
    {TIRION_KNEELS_1,               0,                          3000},
    {EMOTE_KNEEL,                   NPC_TIRION_FORDRING,        4000},
    {SAY_EPILOG_2,                  NPC_TIRION_FORDRING,        5000},
    {EMOTE_HOLD_TAELAN,             NPC_TIRION_FORDRING,        5000},
    {SAY_EPILOG_3,                  NPC_TIRION_FORDRING,        6000},
    {SAY_EPILOG_4,                  NPC_TIRION_FORDRING,        7000},
    {SAY_EPILOG_5,                  NPC_TIRION_FORDRING,        0},
    {0, 0, 0},
};

// 随从/NPC AI：fordring（taelan_fordring）
struct npc_taelan_fordringAI: public npc_escortAI, private DialogueHelper
{
    npc_taelan_fordringAI(Creature* creature): npc_escortAI(creature), DialogueHelper(aScarletDialogue)
    {
        m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);

        m_scarletComplete = false;
        m_fightStarted = false;
        m_taelanDead = false;
        m_hasMount = false;
        Reset();
    }

    bool m_scarletComplete;
    bool m_fightStarted;
    bool m_hasMount;
    bool m_taelanDead;

    ObjectGuid m_isillenGuid;                               // isillen 实体 GUID
    ObjectGuid m_tirionGuid;                                // tirion 实体 GUID
    GuidList m_lCavalierGuids;

    uint32 m_holyCleaveTimer;
    uint32 m_holyStrikeTimer;
    uint32 m_crusaderStrike;
    uint32 m_holyLightTimer;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_holyCleaveTimer = urand(11000, 15000);
        m_holyStrikeTimer = urand(6000, 8000);
        m_crusaderStrike  = urand(1000, 5000);
        m_holyLightTimer  = 0;
    }

    void Aggro(Unit* /*who*/) override
    {
        if (m_hasMount)
            m_creature->Unmount();

        DoCastSpellIfCan(m_creature, SPELL_DEVOTION_AURA);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (m_taelanDead)
            return;

        npc_escortAI::MoveInLineOfSight(who);
    }

    // 进入脱战闪避模式
    void EnterEvadeMode() override
    {
        if (m_taelanDead)
        {
            m_creature->RemoveAllAurasOnEvade();
            m_creature->CombatStop(true);
            m_creature->SetLootRecipient(nullptr);

            m_creature->InterruptNonMeleeSpells(true);
            m_creature->SetHealth(0);
            m_creature->StopMoving();
            m_creature->ClearComboPointHolders();
            m_creature->RemoveAllAurasOnDeath();
            m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);
            m_creature->ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, false);
            m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNINTERACTIBLE);
            m_creature->ClearAllReactives();
            m_creature->SetImmobilizedState(true, true);
            m_creature->SetStandState(UNIT_STAND_STATE_DEAD);

            Reset();
        }
        else
        {
            if (m_hasMount)
                m_creature->Mount(MODEL_TAELAN_MOUNT);

            npc_escortAI::EnterEvadeMode();
        }
    }

    // 脱战归位回调：重置遭遇战状态
    void JustReachedHome() override
    {
        if (m_scarletComplete)
        {
            StartNextDialogueText(SPELL_DEVOTION_AURA);
            m_scarletComplete = false;
        }
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* /*sender*/, Unit* invoker, uint32 miscValue) override
    {
        if (eventType == AI_EVENT_START_ESCORT && invoker->GetTypeId() == TYPEID_PLAYER)
        {
            Start(false, (Player*)invoker, GetQuestTemplateStore(miscValue));
            DoScriptText(SAY_ESCORT_START, m_creature);
            m_creature->SetFactionTemporary(FACTION_ESCORT_N_FRIEND_PASSIVE, TEMPFACTION_RESTORE_RESPAWN);
        }
        else if (eventType == AI_EVENT_CUSTOM_A && invoker->GetTypeId() == TYPEID_PLAYER && miscValue == QUEST_ID_SCARLET_SUBTERFUGE)
            StartNextDialogueText(NPC_SCARLET_CAVALIER);
        else if (eventType == AI_EVENT_CUSTOM_B && invoker->GetEntry() == NPC_ISILLIEN)
        {
            StartNextDialogueText(NPC_TIRION_FORDRING);
            if (Creature* tirion = m_creature->SummonCreature(NPC_TIRION_FORDRING, 2620.273f, -1920.917f, 74.25f, 0, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 15 * MINUTE * IN_MILLISECONDS))
            {
                tirion->SetWalk(false);
                tirion->SetImmuneToNPC(true);  // Temporary make Tirion immune to nearby Scarlet NPCs so the script can run smoothly
            }
        }
    }

    void WaypointReached(uint32 pointId) override
    {
        switch (pointId)
        {
            case 26:
                SetEscortPaused(true);
                StartNextDialogueText(SAY_EXIT_KEEP);
                break;
            case 56:
                SetEscortPaused(true);
                m_creature->GetMotionMaster()->Clear();
                m_creature->GetMotionMaster()->MoveIdle();
                StartNextDialogueText(SAY_REACH_TOWER);
                break;
        }
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* summoned) override
    {
        switch (summoned->GetEntry())
        {
            case NPC_ISILLIEN:
                SendAIEvent(AI_EVENT_START_ESCORT, m_creature, summoned);
                m_isillenGuid = summoned->GetObjectGuid();

                // summon additional crimson elites (召唤 additional crimson elites)
                float fX, fY, fZ;
                summoned->GetNearPoint(summoned, fX, fY, fZ, 0, 5.0f, M_PI_F * 1.25f);
                summoned->SummonCreature(NPC_CRIMSON_ELITE, fX, fY, fZ, summoned->GetOrientation(), TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 15 * MINUTE * IN_MILLISECONDS);
                summoned->GetNearPoint(summoned, fX, fY, fZ, 0, 5.0f, 0);
                summoned->SummonCreature(NPC_CRIMSON_ELITE, fX, fY, fZ, summoned->GetOrientation(), TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 15 * MINUTE * IN_MILLISECONDS);
                break;
            case NPC_TIRION_FORDRING:
                m_tirionGuid = summoned->GetObjectGuid();
                SendAIEvent(AI_EVENT_START_ESCORT, m_creature, summoned);
                break;
        }
    }

    void SummonedMovementInform(Creature* summoned, uint32 /*motionType*/, uint32 pointId) override
    {
        if (summoned->GetEntry() != NPC_TIRION_FORDRING)
            return;

        if (pointId == 100)
        {
            StartNextDialogueText(NPC_ISILLIEN);
            summoned->SetFacingToObject(m_creature);
            summoned->SetStandState(UNIT_STAND_STATE_KNEEL);
        }
        else if (pointId == 200)
            StartNextDialogueText(SAY_EPILOG_1);
    }

    void JustDidDialogueStep(int32 entry) override
    {
        switch (entry)
        {
            case NPC_SCARLET_CAVALIER:
            {
                // kneel and make everyone worried
                m_creature->SetStandState(UNIT_STAND_STATE_KNEEL);
                m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);

                CreatureList lCavaliersInRange;
                GetCreatureListWithEntryInGrid(lCavaliersInRange, m_creature, NPC_SCARLET_CAVALIER, 10.0f);

                uint8 index = 0;
                for (auto* cavalier : lCavaliersInRange)
                {
                    if (cavalier->IsAlive() && !cavalier->IsInCombat() && cavalier->GetPositionZ() > 164.0f) // Prevent picking NPC from another floor
                    {
                        m_lCavalierGuids.push_back(cavalier->GetObjectGuid());
                        cavalier->SetFacingToObject(m_creature);
                        DoScriptText(aCavalierYells[index], cavalier);
                        ++index;
                    }
                }
                break;
            }
            case QUEST_ID_SCARLET_SUBTERFUGE:
            {
                float fX, fY, fZ;
                for (GuidList::const_iterator itr = m_lCavalierGuids.begin(); itr != m_lCavalierGuids.end(); ++itr)
                {
                    if (Creature* cavalier = m_creature->GetMap()->GetCreature(*itr))
                    {
                        m_creature->GetContactPoint(cavalier, fX, fY, fZ);
                        cavalier->GetMotionMaster()->MovePoint(0, fX, fY, fZ);
                    }
                }
                break;
            }
            case SAY_SCARLET_COMPLETE_1:
                // stand up and knock down effect
                m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                DoCastSpellIfCan(m_creature, SPELL_TAELAN_SUFFERING);
                break;
            case QUEST_ID_IN_DREAMS:
                // force attack (force攻击)
                m_creature->SetFactionTemporary(FACTION_ESCORT_N_FRIEND_PASSIVE, TEMPFACTION_RESTORE_RESPAWN);
                for (GuidList::const_iterator itr = m_lCavalierGuids.begin(); itr != m_lCavalierGuids.end(); ++itr)
                {
                    if (Creature* cavalier = m_creature->GetMap()->GetCreature(*itr))
                        cavalier->AI()->AttackStart(m_creature);
                }
                m_scarletComplete = true;
                break;
            case SAY_SCARLET_COMPLETE_2:
                m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                break;
            case MODEL_TAELAN_MOUNT:
                // mount when outside
                m_hasMount = true;
                SetEscortPaused(false);
                m_creature->Mount(MODEL_TAELAN_MOUNT);
                break;
            case SAY_REACH_TOWER:
                // start fight event (开始fight事件)
                if (Player* player = GetPlayerForEscort())
                    m_creature->SetFacingToObject(player);
                m_creature->SummonCreature(NPC_ISILLIEN, 2693.12f, -1943.04f, 72.04f, 2.11f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 15 * MINUTE * IN_MILLISECONDS);
                break;
            case SAY_ISILLIEN_2:
                if (Creature* isillien = m_creature->GetMap()->GetCreature(m_isillenGuid))
                    m_creature->SetFacingToObject(isillien);
                break;
            case SAY_ISILLIEN_3:
                m_creature->Unmount();
                break;
            case SPELL_CRUSADER_STRIKE:
            {
                float fX, fY, fZ;
                // spawn 3 additional elites
                if (Creature* elite = m_creature->SummonCreature(NPC_CRIMSON_ELITE, 2711.32f, -1882.67f, 67.89f, 3.2f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 15 * MINUTE * IN_MILLISECONDS))
                {
                    elite->SetWalk(false);
                    m_creature->GetContactPoint(elite, fX, fY, fZ);
                    elite->GetMotionMaster()->MovePoint(0, fX, fY, fZ);
                }
                if (Creature* elite = m_creature->SummonCreature(NPC_CRIMSON_ELITE, 2710.93f, -1878.90f, 67.97f, 3.2f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 15 * MINUTE * IN_MILLISECONDS))
                {
                    elite->SetWalk(false);
                    m_creature->GetContactPoint(elite, fX, fY, fZ);
                    elite->GetMotionMaster()->MovePoint(0, fX, fY, fZ);
                }
                if (Creature* elite = m_creature->SummonCreature(NPC_CRIMSON_ELITE, 2710.53f, -1875.28f, 67.90f, 3.2f, TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, 15 * MINUTE * IN_MILLISECONDS))
                {
                    elite->SetWalk(false);
                    m_creature->GetContactPoint(elite, fX, fY, fZ);
                    elite->GetMotionMaster()->MovePoint(0, fX, fY, fZ);
                }

                // Isillien only attacks Taelan (Isillien仅attacks泰兰)
                if (Creature* isillien = m_creature->GetMap()->GetCreature(m_isillenGuid))
                {
                    isillien->AI()->AttackStart(m_creature);
                    AttackStart(isillien);
                }

                m_fightStarted = true;
                break;
            }
            case SAY_KILL_TAELAN_1:
                // Kill Taelan and attack players (击杀泰兰and攻击players)
                if (Creature* isillien = m_creature->GetMap()->GetCreature(m_isillenGuid))
                    SendAIEvent(AI_EVENT_CUSTOM_A, m_creature, isillien);
                break;
            case EMOTE_ATTACK_PLAYER:
                // Attack players (攻击players)
                if (Creature* isillien = m_creature->GetMap()->GetCreature(m_isillenGuid))
                {
                    if (Player* player = GetPlayerForEscort())
                        isillien->AI()->AttackStart(player);
                }
                break;
            // Tirion event (Tirion事件)
            case SAY_TIRION_1:
                if (Creature* tirion = m_creature->GetMap()->GetCreature(m_tirionGuid))
                {
                    tirion->SetStandState(UNIT_STAND_STATE_STAND);
                    if (Creature* isillien = m_creature->GetMap()->GetCreature(m_isillenGuid))
                    {
                        tirion->SetFacingToObject(isillien);
                        isillien->CombatStop();
                    }
                }
                break;
            case SAY_TIRION_5:
                if (Creature* isillien = m_creature->GetMap()->GetCreature(m_isillenGuid))
                {
                    if (Creature* tirion = m_creature->GetMap()->GetCreature(m_tirionGuid))
                    {
                        tirion->SetImmuneToNPC(false);
                        tirion->AI()->AttackStart(isillien);
                        isillien->AI()->AttackStart(tirion);
                    }
                }
                break;
            // Epilogue dialogue
            case SAY_EPILOG_1:
                if (Creature* isillien = m_creature->GetMap()->GetCreature(m_isillenGuid))
                {
                    if (Creature* tirion = m_creature->GetMap()->GetCreature(m_tirionGuid))
                        tirion->SetFacingToObject(isillien);
                }
                break;
            case TIRION_KNEELS_1:
                if (Creature* tirion = m_creature->GetMap()->GetCreature(m_tirionGuid))
                    tirion->SetFacingToObject(m_creature);
                break;
            case EMOTE_HOLD_TAELAN:
                if (Creature* tirion = m_creature->GetMap()->GetCreature(m_tirionGuid))
                    tirion->SetStandState(UNIT_STAND_STATE_KNEEL);
                break;
            case SAY_EPILOG_4:
                if (Creature* tirion = m_creature->GetMap()->GetCreature(m_tirionGuid))
                    tirion->SetStandState(UNIT_STAND_STATE_STAND);
                break;
            case SAY_EPILOG_5:
                if (Creature* tirion = m_creature->GetMap()->GetCreature(m_tirionGuid))
                {
                    if (Player* player = GetPlayerForEscort())
                        player->RewardPlayerAndGroupAtEventExplored(QUEST_ID_IN_DREAMS, m_creature);

                    tirion->ForcedDespawn(3 * MINUTE * IN_MILLISECONDS);
                    tirion->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                }
                m_creature->ForcedDespawn(3 * MINUTE * IN_MILLISECONDS);
                break;
        }
    }

    Creature* GetSpeakerByEntry(uint32 entry) override
    {
        switch (entry)
        {
            case NPC_TAELAN_FORDRING:   return m_creature;
            case NPC_ISILLIEN:          return m_creature->GetMap()->GetCreature(m_isillenGuid);
            case NPC_TIRION_FORDRING:   return m_creature->GetMap()->GetCreature(m_tirionGuid);

            default:
                return nullptr;
        }
    }

    void UpdateEscortAI(const uint32 diff) override
    {
        DialogueUpdate(diff);

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        if (m_taelanDead)
            return;

        // If Taelan is in fight with Isillien and drops below 50% make him "dead"
        if (!m_taelanDead && m_fightStarted && m_creature->GetHealthPercent() < 50.0f)
        {
            StartNextDialogueText(SAY_KILL_TAELAN_1);
            m_taelanDead = true;
        }

        if (m_holyCleaveTimer < diff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_HOLY_CLEAVE) == CAST_OK)
                m_holyCleaveTimer = urand(11000, 13000);
        }
        else
            m_holyCleaveTimer -= diff;

        if (m_holyStrikeTimer < diff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_HOLY_STRIKE) == CAST_OK)
                m_holyStrikeTimer = urand(9000, 14000);
        }
        else
            m_holyStrikeTimer -= diff;

        if (m_crusaderStrike < diff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_CRUSADER_STRIKE) == CAST_OK)
                m_crusaderStrike = urand(7000, 12000);
        }
        else
            m_crusaderStrike -= diff;

        if (m_creature->GetHealthPercent() < 75.0f)
        {
            if (m_holyLightTimer < diff)
            {
                if (Unit* target = DoSelectLowestHpFriendly(50.0f))
                {
                    if (DoCastSpellIfCan(target, SPELL_HOLY_LIGHT) == CAST_OK)
                        m_holyLightTimer = urand(10000, 15000);
                }
            }
            else
                m_holyLightTimer -= diff;
        }

        if (!m_fightStarted && m_creature->GetHealthPercent() < 15.0f)
            DoCastSpellIfCan(m_creature, SPELL_LAY_ON_HANDS);

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：npc_taelan_fordring
UnitAI* GetAI_npc_taelan_fordring(Creature* creature)
{
    return new npc_taelan_fordringAI(creature);
}

// 任务接取回调函数：npc_taelan_fordring
bool QuestAccept_npc_taelan_fordring(Player* player, Creature* creature, const Quest* quest)
{
    if (quest->GetQuestId() == QUEST_ID_IN_DREAMS)
        creature->AI()->SendAIEvent(AI_EVENT_START_ESCORT, player, creature, quest->GetQuestId());

    return true;
}

bool QuestRewarded_npc_taelan_fordring(Player* player, Creature* creature, const Quest* quest)
{
    if (quest->GetQuestId() == QUEST_ID_SCARLET_SUBTERFUGE)
        creature->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, player, creature, quest->GetQuestId());

    return true;
}

// 18969 - Taelan Death (18969 - 泰兰 死亡)
struct TaelanDeath : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* caster = spell->GetCaster();
        Unit* target = spell->GetUnitTarget();
        if (target->AI())
            target->AI()->EnterEvadeMode();
        caster->SetFacingToObject(target);
        if (caster->AI())
            caster->AI()->EnterEvadeMode();
    }
};

/*######
## npc_isillien
######*/

// 随从/NPC AI：标识 ID（isillien）
struct npc_isillienAI: public npc_escortAI
{
    npc_isillienAI(Creature* creature): npc_escortAI(creature)
    {
        m_tirionSpawned = false;
        m_taelanDead = false;
        m_summonTirionTimer = 0;
        Reset();
    }

    bool m_tirionSpawned;
    bool m_taelanDead;

    ObjectGuid m_taelanGuid;                                // taelan 实体 GUID

    uint32 m_summonTirionTimer;
    uint32 m_manaBurnTimer;
    uint32 m_flashHealTimer;
    uint32 m_greaterHealTimer;
    uint32 m_holyLightTimer;
    uint32 m_dominateTimer;
    uint32 m_mindBlastTimer;
    uint32 m_mindFlayTimer;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_manaBurnTimer   = urand(7000, 12000);
        m_flashHealTimer   = urand(10000, 15000);
        m_dominateTimer   = urand(10000, 14000);
        m_mindBlastTimer  = urand(0, 1000);
        m_mindFlayTimer   = urand(3000, 7000);
        m_greaterHealTimer = 0;
    }

    void MoveInLineOfSight(Unit* /*who*/) override
    {
        // attack only on request (攻击仅onrequest)
    }

    // 进入脱战闪避模式
    void EnterEvadeMode() override
    {
        // evade and keep the same posiiton
        if (m_taelanDead)
        {
            m_creature->RemoveAllAurasOnEvade();
            m_creature->CombatStop(true);
            m_creature->SetLootRecipient(nullptr);

            m_creature->GetMotionMaster()->MoveIdle();

            Reset();
        }
        else
            npc_escortAI::EnterEvadeMode();
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* sender, Unit* invoker, uint32 /*miscValue*/) override
    {
        if (sender->GetEntry() != NPC_TAELAN_FORDRING)
            return;

        // move outside the tower
        if (eventType == AI_EVENT_START_ESCORT)
            Start(false);
        else if (eventType == AI_EVENT_CUSTOM_A)
        {
            // kill Taelan (击杀泰兰)
            DoCastSpellIfCan(invoker, SPELL_TAELAN_DEATH, CAST_INTERRUPT_PREVIOUS);
            m_taelanDead = true;
            m_summonTirionTimer = 120000;      // 2 minutes wait before Tirion arrives
            m_taelanGuid = invoker->GetObjectGuid();
        }
    }

    void WaypointReached(uint32 pointId) override
    {
        switch (pointId)
        {
            case 3:
                SetEscortPaused(true);
                break;
        }
    }

    void UpdateEscortAI(const uint32 diff) override
    {
        // Start event epilogue (开始事件epilogue)
        if (!m_tirionSpawned && ((m_summonTirionTimer != 0 && m_summonTirionTimer < diff) || m_creature->GetHealthPercent() < 20.0f))
        {
            if (Creature* taelan = m_creature->GetMap()->GetCreature(m_taelanGuid))
                SendAIEvent(AI_EVENT_CUSTOM_B, m_creature, taelan);
            m_tirionSpawned = true;
        }
        else if (m_summonTirionTimer)
            m_summonTirionTimer -= diff;

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // Combat spells (战斗技能法术)
        if (m_mindBlastTimer < diff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MIND_BLAST) == CAST_OK)
                m_mindBlastTimer = urand(3000, 5000);
        }
        else
            m_mindBlastTimer -= diff;

        if (m_mindFlayTimer < diff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MIND_FLAY) == CAST_OK)
                m_mindFlayTimer = urand(9000, 15000);
        }
        else
            m_mindFlayTimer -= diff;

        if (m_manaBurnTimer < diff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_MANA_BURN) == CAST_OK)
                m_manaBurnTimer = urand(8000, 12000);
        }
        else
            m_manaBurnTimer -= diff;

        if (m_dominateTimer < diff)
        {
            if (Unit* target = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1, SPELL_DOMINATE_MIND, SELECT_FLAG_PLAYER))
            {
                if (DoCastSpellIfCan(target, SPELL_DOMINATE_MIND) == CAST_OK)
                    m_dominateTimer = urand(25000, 30000);
            }
        }
        else
            m_dominateTimer -= diff;

        if (m_flashHealTimer < diff)
        {
            if (Unit* target = DoSelectLowestHpFriendly(50.0f))
            {
                if (DoCastSpellIfCan(target, SPELL_FLASH_HEAL) == CAST_OK)
                    m_flashHealTimer = urand(10000, 15000);
            }
        }
        else
            m_flashHealTimer -= diff;

        if (m_creature->GetHealthPercent() < 50.0f)
        {
            if (m_greaterHealTimer < diff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_GREATER_HEAL) == CAST_OK)
                    m_greaterHealTimer = urand(15000, 20000);
            }
            else
                m_greaterHealTimer -= diff;
        }

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：npc_isillien
UnitAI* GetAI_npc_isillien(Creature* creature)
{
    return new npc_isillienAI(creature);
}

/*######
## npc_tirion_fordring
######*/

// 随从/NPC AI：fordring（tirion_fordring）
struct npc_tirion_fordringAI: public npc_escortAI
{
    npc_tirion_fordringAI(Creature* creature): npc_escortAI(creature) { Reset(); }

    ObjectGuid m_taelanGuid;                                // taelan 实体 GUID

    uint32 m_holyCleaveTimer;
    uint32 m_holyStrikeTimer;
    uint32 m_crusaderStrike;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_holyCleaveTimer = urand(11000, 15000);
        m_holyStrikeTimer = urand(6000, 8000);
        m_crusaderStrike  = urand(1000, 5000);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoCastSpellIfCan(m_creature, SPELL_DEVOTION_AURA);
    }

    void MoveInLineOfSight(Unit* /*who*/) override
    {
        // attack only on request (攻击仅onrequest)
    }

    // 进入脱战闪避模式
    void EnterEvadeMode() override
    {
        m_creature->RemoveAllAurasOnEvade();
        m_creature->CombatStop(true);
        m_creature->SetLootRecipient(nullptr);

        // on evade go to Taelan
        if (Creature* taelan = m_creature->GetMap()->GetCreature(m_taelanGuid))
        {
            float fX, fY, fZ;
            taelan->GetContactPoint(m_creature, fX, fY, fZ);
            m_creature->GetMotionMaster()->MovePoint(200, fX, fY, fZ);
        }

        Reset();
    }

    void ReceiveAIEvent(AIEventType eventType, Unit* sender, Unit* invoker, uint32 /*miscValue*/) override
    {
        if (sender->GetEntry() != NPC_TAELAN_FORDRING)
            return;

        if (eventType == AI_EVENT_START_ESCORT)
        {
            Start(true);
            m_taelanGuid = invoker->GetObjectGuid();
        }
    }

    void WaypointReached(uint32 pointId) override
    {
        switch (pointId)
        {
            case 3:
                SetEscortPaused(true);

                // unmount and go to Taelan
                m_creature->Unmount();
                m_creature->GetMotionMaster()->Clear();
                m_creature->GetMotionMaster()->MoveIdle();
                if (Creature* taelan = m_creature->GetMap()->GetCreature(m_taelanGuid))
                {
                    float fX, fY, fZ;
                    taelan->GetContactPoint(m_creature, fX, fY, fZ);
                    m_creature->GetMotionMaster()->MovePoint(100, fX, fY, fZ);
                }
                break;
        }
    }

    // 移动路径点到达回调
    void MovementInform(uint32 moveType, uint32 pointId) override
    {
        // custom points; ignore in escort AI
        if (pointId == 100 || pointId == 200)
            return;

        npc_escortAI::MovementInform(moveType, pointId);
    }

    void UpdateEscortAI(const uint32 diff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        // combat spells (战斗spells)
        if (m_holyCleaveTimer < diff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_HOLY_CLEAVE) == CAST_OK)
                m_holyCleaveTimer = urand(12000, 15000);
        }
        else
            m_holyCleaveTimer -= diff;

        if (m_holyStrikeTimer < diff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_HOLY_STRIKE) == CAST_OK)
                m_holyStrikeTimer = urand(8000, 11000);
        }
        else
            m_holyStrikeTimer -= diff;

        if (m_crusaderStrike < diff)
        {
            if (DoCastSpellIfCan(m_creature->GetVictim(), SPELL_CRUSADER_STRIKE) == CAST_OK)
                m_crusaderStrike = urand(7000, 9000);
        }
        else
            m_crusaderStrike -= diff;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：npc_tirion_fordring
UnitAI* GetAI_npc_tirion_fordring(Creature* creature)
{
    return new npc_tirion_fordringAI(creature);
}

/*######
## spell_placing_beacon_torch
######*/

// 法术脚本逻辑实现：placingbeacontorch 法术 ID（spell_placing_beacon_torch）
struct spell_placing_beacon_torch : public SpellScript
{
    void OnRadiusCalculate(Spell* /*spell*/, SpellEffectIndex effIdx, bool /*targetB*/, float& radius) const override
    {
        if (effIdx == EFFECT_INDEX_0)
            radius = 0.f;
        if (effIdx == EFFECT_INDEX_1)
            radius = 10.f;
    }
};


// 注册并载入脚本模块：western_plaguelands
void AddSC_western_plaguelands()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_the_scourge_cauldron";
    pNewScript->GetAI = &GetAI_npc_the_scourge_cauldron;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_anchorite_truuen";
    pNewScript->GetAI = &GetAI_npc_anchorite_truuen;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_anchorite_truuen;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_taelan_fordring";
    pNewScript->GetAI = &GetAI_npc_taelan_fordring;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_taelan_fordring;
    pNewScript->pQuestRewardedNPC = &QuestRewarded_npc_taelan_fordring;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_isillien";
    pNewScript->GetAI = &GetAI_npc_isillien;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_tirion_fordring";
    pNewScript->GetAI = &GetAI_npc_tirion_fordring;
    pNewScript->RegisterSelf();

    RegisterSpellScript<TaelanDeath>("spell_taelan_death");
    RegisterSpellScript<spell_placing_beacon_torch>("spell_placing_beacon_torch");
}
