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
SDName: gnomeregan
SD%Complete: 90
SDComment:  Grubbis Encounter, quest 2904 (A fine mess)
SDCategory: Gnomeregan
EndScriptData */

// 诺莫瑞根（Gnomeregan）辐射报警红灯与杂项生物 AI

/* ContentData
npc_blastmaster_emi_shortfuse
npc_kernobee
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "gnomeregan.h"
#include "AI/ScriptDevAI/base/escort_ai.h"
#include "AI/ScriptDevAI/base/follower_ai.h"

/*######
## npc_blastmaster_emi_shortfuse
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    SAY_START                   = -1090000,                 // 开始 台词广播 ID
    SAY_INTRO_1                 = -1090001,                 // 开场1 台词广播 ID
    SAY_INTRO_2                 = -1090002,                 // 开场2 台词广播 ID
    SAY_INTRO_3                 = -1090003,                 // 开场3 台词广播 ID
    SAY_INTRO_4                 = -1090004,                 // 开场4 台词广播 ID
    SAY_LOOK_1                  = -1090005,                 // look1 台词广播 ID
    SAY_HEAR_1                  = -1090006,                 // hear1 台词广播 ID
    SAY_AGGRO_1                 = -1090007,                 // 开战1 台词广播 ID
    SAY_CHARGE_1                = -1090008,                 // 冲锋1 台词广播 ID
    SAY_CHARGE_2                = -1090009,                 // 冲锋2 台词广播 ID
    SAY_BLOW_1_10               = -1090010,                 // blow110 台词广播 ID
    SAY_BLOW_1_5                = -1090011,                 // blow15 台词广播 ID
    SAY_BLOW_1                  = -1090012,                 // blow1 台词广播 ID
    SAY_FINISH_1                = -1090013,                 // finish1 台词广播 ID
    SAY_LOOK_2                  = -1090014,                 // look2 台词广播 ID
    SAY_HEAR_2                  = -1090015,                 // hear2 台词广播 ID
    SAY_CHARGE_3                = -1090016,                 // 冲锋3 台词广播 ID
    SAY_CHARGE_4                = -1090017,                 // 冲锋4 台词广播 ID
    SAY_BLOW_2_10               = -1090018,                 // blow210 台词广播 ID
    SAY_BLOW_2_5                = -1090019,                 // blow25 台词广播 ID
    SAY_BLOW_SOON               = -1090020,                 // blowsoon 台词广播 ID
    SAY_BLOW_2                  = -1090021,                 // blow2 台词广播 ID
    SAY_FINISH_2                = -1090022,                 // finish2 台词广播 ID

    SAY_AGGRO_2                 = -1090028,                 // 开战2 台词广播 ID

    SAY_GRUBBIS_SPAWN           = -1090023,                 // grubbis生成 台词广播 ID

    GOSSIP_ITEM_START           = -3090000,                 // 物品开始 对话菜单选项 ID

    SPELL_EXPLOSION_NORTH       = 12158,                    // 爆炸north 法术 ID
    SPELL_EXPLOSION_SOUTH       = 12159,                    // 爆炸south 法术 ID
    SPELL_FIREWORKS_RED         = 11542,                    // fireworksred 法术 ID

    MAX_SUMMON_POSITIONS        = 33,                       // 召唤positions 标识 ID

    NPC_GRUBBIS                 = 7361,                     // grubbis Entry ID
    NPC_CHOMPER                 = 6215,                     // chomper Entry ID
    NPC_CAVERNDEEP_BURROWER     = 6206,                     // caverndeepburrower Entry ID
    NPC_CAVERNDEEP_AMBUSHER     = 6207                      // caverndeepambusher Entry ID
};

struct sSummonInformation
{
    uint32 uiPosition, uiEntry;
    float fX, fY, fZ, fO;
};

static const sSummonInformation asSummonInfo[MAX_SUMMON_POSITIONS] =
{
    // Entries must be sorted by pack
    // First Cave-In
    {1, NPC_CAVERNDEEP_AMBUSHER, -566.8114f, -111.7036f, -151.1891f, 5.986479f},
    {1, NPC_CAVERNDEEP_AMBUSHER, -568.5875f, -113.7559f, -151.1869f, 0.06981317f},
    {1, NPC_CAVERNDEEP_AMBUSHER, -570.2333f, -116.8126f, -151.2272f, 0.296706f},
    {1, NPC_CAVERNDEEP_AMBUSHER, -550.6331f, -108.7592f, -153.965f, 0.8901179f},
    {1, NPC_CAVERNDEEP_AMBUSHER, -558.9717f, -115.0669f, -151.8799f, 0.5235988f},
    {1, NPC_CAVERNDEEP_AMBUSHER, -556.6719f, -112.0526f, -152.8255f, 0.4886922f},
    {1, NPC_CAVERNDEEP_AMBUSHER, -552.6419f, -113.4385f, -153.0727f, 0.8028514f},
    {1, NPC_CAVERNDEEP_AMBUSHER, -549.1248f, -112.1469f, -153.7987f, 0.7504916f},
    {1, NPC_CAVERNDEEP_AMBUSHER, -546.7435f, -112.3051f, -154.2225f, 0.9250245f},
    {2, NPC_CAVERNDEEP_AMBUSHER, -571.4071f, -108.7721f, -150.6547f, 5.480334f},
    {2, NPC_CAVERNDEEP_AMBUSHER, -573.797f, -106.5265f, -150.4106f, 5.550147f},
    {2, NPC_CAVERNDEEP_AMBUSHER, -576.3784f, -108.0483f, -150.4227f, 5.585053f},
    {2, NPC_CAVERNDEEP_AMBUSHER, -576.697f, -111.7413f, -150.6484f, 5.759586f},
    {3, NPC_CAVERNDEEP_AMBUSHER, -571.3161f, -114.4412f, -151.0931f, 6.021386f},
    {3, NPC_CAVERNDEEP_AMBUSHER, -570.3127f, -111.7964f, -151.04f, 2.042035f},

    // Second Cave-In
    {4, NPC_CAVERNDEEP_AMBUSHER, -474.5954f, -104.074f, -146.0483f, 2.338741f},
    {4, NPC_CAVERNDEEP_AMBUSHER, -477.9396f, -108.6563f, -145.7394f, 1.553343f},
    {4, NPC_CAVERNDEEP_AMBUSHER, -475.6625f, -97.12168f, -146.5959f, 1.291544f},
    {4, NPC_CAVERNDEEP_AMBUSHER, -480.5233f, -88.40702f, -146.3772f, 3.001966f},
    {5, NPC_CAVERNDEEP_AMBUSHER, -474.2943f, -105.2212f, -145.9747f, 2.251475f},
    {5, NPC_CAVERNDEEP_AMBUSHER, -481.1831f, -101.4225f, -146.377f, 2.146755f},
    {5, NPC_CAVERNDEEP_BURROWER, -475.0871f, -100.016f, -146.4382f, 2.303835f},
    {5, NPC_CAVERNDEEP_AMBUSHER, -478.8562f, -106.9321f, -145.8533f, 1.658063f},
    {6, NPC_CAVERNDEEP_AMBUSHER, -473.8762f, -107.4022f, -145.838f, 2.024582f},
    {6, NPC_CAVERNDEEP_AMBUSHER, -490.5134f, -92.72843f, -148.0954f, 3.054326f},
    {6, NPC_CAVERNDEEP_AMBUSHER, -491.401f, -88.25341f, -148.0358f, 3.560472f},
    {6, NPC_CAVERNDEEP_AMBUSHER, -479.1431f, -106.227f, -145.9097f, 1.727876f},
    {6, NPC_CAVERNDEEP_AMBUSHER, -475.3185f, -101.4804f, -146.2717f, 2.234021f},
    {6, NPC_CAVERNDEEP_AMBUSHER, -485.1559f, -89.57419f, -146.9299f, 3.071779f},
    {6, NPC_CAVERNDEEP_AMBUSHER, -482.2516f, -96.80614f, -146.6596f, 2.303835f},
    {6, NPC_CAVERNDEEP_AMBUSHER, -477.9874f, -92.82047f, -146.6944f, 3.124139f},

    // Grubbis and add
    {7, NPC_GRUBBIS, -476.3761f, -108.1901f, -145.7763f, 1.919862f},
    {7, NPC_CHOMPER, -473.1326f, -103.0901f, -146.1155f, 2.042035f}
};

// 随从/NPC AI：emishortfuse（blastmaster_emi_shortfuse）
struct npc_blastmaster_emi_shortfuseAI : public npc_escortAI
{
    npc_blastmaster_emi_shortfuseAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        m_pInstance = (instance_gnomeregan*)pCreature->GetInstanceData();
        // Remove Gossip-Menu in reload case for DONE enounter
        if (m_pInstance && m_pInstance->GetData(TYPE_GRUBBIS) == DONE)
            pCreature->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
        Reset();
    }

    instance_gnomeregan* m_pInstance;

    uint8 m_uiPhase;
    uint32 m_uiPhaseTimer;                                  // Phase 技能计时器
    ObjectGuid m_playerGuid;                                // player 实体 GUID
    bool m_bDidAggroText, m_bSouthernCaveInOpened, m_bNorthernCaveInOpened;
    GuidList m_luiSummonedMobGUIDs;

    // 重置战斗状态与技能计时器
    void Reset() override
    {
        m_bDidAggroText = false;                            // Used for 'defend' text, is triggered when the npc is attacked

        if (!HasEscortState(STATE_ESCORT_ESCORTING))
        {
            m_uiPhase = 0;
            m_uiPhaseTimer = 0;
            m_bSouthernCaveInOpened = m_bNorthernCaveInOpened = false;
            m_luiSummonedMobGUIDs.clear();
        }
    }

    void DoSummonPack(uint8 uiIndex)
    {
        for (const auto& i : asSummonInfo)
        {
            // This requires order of the array
            if (i.uiPosition > uiIndex)
                break;
            if (i.uiPosition == uiIndex)
                m_creature->SummonCreature(i.uiEntry, i.fX, i.fY, i.fZ, i.fO, TEMPSPAWN_DEAD_DESPAWN, 0);
        }
    }

    // 召唤生物生成回调
    void JustSummoned(Creature* pSummoned) override
    {
        switch (pSummoned->GetEntry())
        {
            case NPC_CAVERNDEEP_BURROWER:
            case NPC_CAVERNDEEP_AMBUSHER:
            {
                if (GameObject* pDoor = m_pInstance->GetSingleGameObjectFromStorage(m_uiPhase > 20 ? GO_CAVE_IN_NORTH : GO_CAVE_IN_SOUTH))
                {
                    float fX, fY, fZ;
                    pDoor->GetNearPoint(pDoor, fX, fY, fZ, 0.0f, 2.0f, frand(0.0f, 2 * M_PI_F));
                    pSummoned->GetMotionMaster()->MovePoint(1, fX, fY, fZ);
                }
                break;
            }
            case NPC_GRUBBIS:
                // Movement of Grubbis and Add to be handled by DB waypoints
                DoScriptText(SAY_GRUBBIS_SPAWN, pSummoned);
                break;
        }
        m_luiSummonedMobGUIDs.push_back(pSummoned->GetObjectGuid());
    }

    // 召唤生物死亡回调
    void SummonedCreatureJustDied(Creature* pSummoned) override
    {
        if (pSummoned->GetEntry() == NPC_GRUBBIS)
        {
            if (m_pInstance)
                m_pInstance->SetData(TYPE_GRUBBIS, DONE);
            m_uiPhaseTimer = 1000;
        }
        m_luiSummonedMobGUIDs.remove(pSummoned->GetObjectGuid());
    }

    bool IsPreparingExplosiveCharge() const
    {
        return m_uiPhase == 11 || m_uiPhase == 13 || m_uiPhase == 26 || m_uiPhase == 28;
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        // In case we are preparing the explosive charges, we won't start attacking mobs
        if (IsPreparingExplosiveCharge())
            return;

        npc_escortAI::MoveInLineOfSight(pWho);
    }

    void AttackStart(Unit* pWho) override
    {
        // In case we are preparing the explosive charges, we won't start attacking mobs
        if (IsPreparingExplosiveCharge())
            return;

        npc_escortAI::AttackStart(pWho);
    }

    void AttackedBy(Unit* pAttacker) override
    {
        // Possibility for Aggro-Text only once per combat
        if (m_bDidAggroText)
            return;

        m_bDidAggroText = true;

        if (!urand(0, 2))
            DoScriptText(urand(0, 1) ? SAY_AGGRO_1 : SAY_AGGRO_2, m_creature, pAttacker);
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (!m_pInstance)
            return;

        m_pInstance->SetData(TYPE_GRUBBIS, FAIL);

        if (m_bSouthernCaveInOpened)                        // close southern cave-in door
            m_pInstance->DoUseDoorOrButton(GO_CAVE_IN_SOUTH);
        if (m_bNorthernCaveInOpened)                        // close northern cave-in door
            m_pInstance->DoUseDoorOrButton(GO_CAVE_IN_NORTH);

        for (GuidList::const_iterator itr = m_luiSummonedMobGUIDs.begin(); itr != m_luiSummonedMobGUIDs.end(); ++itr)
        {
            if (Creature* pSummoned = m_creature->GetMap()->GetCreature(*itr))
                pSummoned->ForcedDespawn();
        }
    }

    void StartEvent(Player* pPlayer)
    {
        if (!m_pInstance)
            return;

        m_pInstance->SetData(TYPE_GRUBBIS, IN_PROGRESS);

        m_uiPhase = 1;
        m_uiPhaseTimer = 1000;
        m_playerGuid = pPlayer->GetObjectGuid();
    }

    void WaypointStart(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 10:
                // Open Southern Cave-In
                if (m_pInstance && !m_bSouthernCaveInOpened)
                    m_pInstance->DoUseDoorOrButton(GO_CAVE_IN_SOUTH);
                m_bSouthernCaveInOpened = true;
                break;
            case 12:
                DoScriptText(SAY_CHARGE_1, m_creature);
                break;
            case 16:
                DoScriptText(SAY_CHARGE_3, m_creature);
                // Open Northern Cave-In
                if (m_pInstance && !m_bNorthernCaveInOpened)
                    m_pInstance->DoUseDoorOrButton(GO_CAVE_IN_NORTH);
                m_bNorthernCaveInOpened = true;
                break;
        }
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 4:
                m_uiPhaseTimer = 1000;
                break;
            case 9:
                m_uiPhaseTimer = 2000;
                break;
            case 11:
                m_creature->HandleEmote(EMOTE_STATE_USESTANDING);
                m_uiPhaseTimer = 15000;
                break;
            case 13:
                m_creature->HandleEmote(EMOTE_STATE_USESTANDING);
                m_uiPhaseTimer = 10000;
                break;
            case 15:
                SetEscortPaused(true);
                if (m_pInstance)
                {
                    if (GameObject* pDoor = m_pInstance->GetSingleGameObjectFromStorage(GO_CAVE_IN_SOUTH))
                        m_creature->SetFacingToObject(pDoor);
                }
                DoScriptText(SAY_BLOW_1_10, m_creature);
                m_uiPhaseTimer = 5000;
                break;
            case 16:
                m_creature->HandleEmote(EMOTE_STATE_USESTANDING);
                m_uiPhaseTimer = 15000;
                break;
            case 17:
                m_creature->HandleEmote(EMOTE_STATE_USESTANDING);
                m_uiPhaseTimer = 10000;
                break;
            case 19:
                m_uiPhaseTimer = 2000;
                SetEscortPaused(true);                      // And keep paused from now on!
                break;
        }
    }

    void UpdateEscortAI(uint32 const uiDiff) override
    {
        // the phases are handled OOC (keeps them in sync with the waypoints)
        if (m_uiPhaseTimer && !m_creature->GetVictim())
        {
            if (m_uiPhaseTimer <= uiDiff)
            {
                switch (m_uiPhase)
                {
                    case 1:
                        DoScriptText(SAY_START, m_creature);
                        m_creature->SetFactionTemporary(FACTION_ESCORT_N_NEUTRAL_PASSIVE, TEMPFACTION_RESTORE_RESPAWN);
                        m_creature->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
                        m_creature->SetUInt32Value(UNIT_DYNAMIC_FLAGS, 0);
                        m_uiPhaseTimer = 5000;
                        break;
                    case 2:
                        DoScriptText(SAY_INTRO_1, m_creature);
                        m_uiPhaseTimer = 3500;              // 6s delay, but 2500ms for escortstarting (6秒倒计时)
                        break;
                    case 3:
                        Start(false, m_creature->GetMap()->GetPlayer(m_playerGuid), nullptr, false, false);
                        m_uiPhaseTimer = 0;
                        break;

                    case 4:                                 // Shortly after reached WP 4
                        DoScriptText(SAY_INTRO_2, m_creature);
                        m_uiPhaseTimer = 0;
                        break;

                    case 5:                                 // Shortly after reached WP 9
                        DoScriptText(SAY_INTRO_3, m_creature);
                        m_uiPhaseTimer = 6000;
                        break;
                    case 6:
                        DoScriptText(SAY_INTRO_4, m_creature);
                        m_uiPhaseTimer = 9000;
                        break;
                    case 7:
                        if (m_pInstance)
                        {
                            if (GameObject* pDoor = m_pInstance->GetSingleGameObjectFromStorage(GO_CAVE_IN_SOUTH))
                                m_creature->SetFacingToObject(pDoor);
                        }
                        m_uiPhaseTimer = 2000;
                        break;
                    case 8:
                        DoScriptText(SAY_LOOK_1, m_creature);
                        m_uiPhaseTimer = 5000;
                        break;
                    case 9:
                        DoScriptText(SAY_HEAR_1, m_creature);
                        m_uiPhaseTimer = 2000;
                        break;
                    case 10:                                // Shortly shortly before starting WP 11
                        DoSummonPack(1);
                        m_uiPhaseTimer = 0;
                        break;

                    case 11:                                // 15s after reached WP 11 (15秒倒计时)
                        DoSummonPack(2);

                        // Summon first explosive charge (召唤 first explosive charge)
                        if (m_pInstance)
                            m_pInstance->SetData(TYPE_EXPLOSIVE_CHARGE, DATA_EXPLOSIVE_CHARGE_1);
                        // Remove EMOTE_STATE_USESTANDING state-emote (移除站立使用状态表情)
                        m_creature->HandleEmote(EMOTE_ONESHOT_NONE);

                        m_uiPhaseTimer = 1;
                        break;
                    case 12:                                // Empty Phase, used to store information about set charge (空阶段，用于存储冲锋信息)
                        m_uiPhaseTimer = 0;
                        break;

                    case 13:                                // 10s after reached WP 13 (10秒倒计时)
                        DoSummonPack(3);

                        // Summon second explosive charge (召唤 second explosive charge)
                        if (m_pInstance)
                            m_pInstance->SetData(TYPE_EXPLOSIVE_CHARGE, DATA_EXPLOSIVE_CHARGE_2);
                        // Remove EMOTE_STATE_USESTANDING state-emote (移除站立使用状态表情)
                        m_creature->HandleEmote(EMOTE_ONESHOT_NONE);

                        m_uiPhaseTimer = 11000;
                        break;
                    case 14:                                // Empty Phase, used to store information about set charge (空阶段，用于存储冲锋信息)
                        m_uiPhaseTimer = 1;
                        break;
                    case 15:                                // shortly before starting WP 14
                        if (Player* pPlayer = m_creature->GetMap()->GetPlayer(m_playerGuid))
                            m_creature->SetFacingToObject(pPlayer);
                        DoScriptText(SAY_CHARGE_2, m_creature);
                        m_uiPhaseTimer = 0;
                        break;

                    case 16:                                // 5s after reaching WP 15 (5秒倒计时)
                        DoScriptText(SAY_BLOW_1_5, m_creature);
                        m_uiPhaseTimer = 5000;
                        break;
                    case 17:
                        DoScriptText(SAY_BLOW_1, m_creature);
                        m_uiPhaseTimer = 1000;
                        break;
                    case 18:
                        DoCastSpellIfCan(m_creature, SPELL_EXPLOSION_SOUTH);
                        m_uiPhaseTimer = 500;
                        break;
                    case 19:
                        // Close southern cave-in and let charges explode
                        if (m_pInstance)
                        {
                            m_pInstance->DoUseDoorOrButton(GO_CAVE_IN_SOUTH);
                            m_bSouthernCaveInOpened = false;
                            m_pInstance->SetData(TYPE_EXPLOSIVE_CHARGE, DATA_EXPLOSIVE_CHARGE_USE);
                        }
                        m_uiPhaseTimer = 5000;
                        break;
                    case 20:
                        m_creature->HandleEmote(EMOTE_ONESHOT_CHEER);
                        m_uiPhaseTimer = 6000;
                        break;
                    case 21:
                        DoScriptText(SAY_FINISH_1, m_creature);
                        m_uiPhaseTimer = 6000;
                        break;
                    case 22:
                        DoScriptText(SAY_LOOK_2, m_creature);
                        m_uiPhaseTimer = 3000;
                        break;
                    case 23:
                        if (m_pInstance)
                        {
                            if (GameObject* pDoor = m_pInstance->GetSingleGameObjectFromStorage(GO_CAVE_IN_NORTH))
                                m_creature->SetFacingToObject(pDoor);
                        }
                        m_uiPhaseTimer = 3000;
                        break;
                    case 24:
                        DoScriptText(SAY_HEAR_2, m_creature);
                        m_uiPhaseTimer = 8000;
                        break;
                    case 25:                                // shortly before starting WP 16
                        SetEscortPaused(false);
                        DoSummonPack(4);
                        m_uiPhaseTimer = 0;
                        break;

                    case 26:                                // 15s after reaching WP 16 (15秒倒计时)
                        DoSummonPack(5);

                        // Summon third explosive charge (召唤 third explosive charge)
                        if (m_pInstance)
                            m_pInstance->SetData(TYPE_EXPLOSIVE_CHARGE, DATA_EXPLOSIVE_CHARGE_3);
                        // Remove EMOTE_STATE_USESTANDING state-emote (移除站立使用状态表情)
                        m_creature->HandleEmote(EMOTE_ONESHOT_NONE);

                        m_uiPhaseTimer = 1;
                        break;
                    case 27:                                // Empty Phase, used to store information about set charge (空阶段，用于存储冲锋信息)
                        m_uiPhaseTimer = 0;
                        break;

                    case 28:                                // 10s after reaching WP 17 (10秒倒计时)
                        DoSummonPack(6);

                        // Summon forth explosive charge (召唤 forth explosive charge)
                        if (m_pInstance)
                            m_pInstance->SetData(TYPE_EXPLOSIVE_CHARGE, DATA_EXPLOSIVE_CHARGE_4);
                        // Remove EMOTE_STATE_USESTANDING state-emote (移除站立使用状态表情)
                        m_creature->HandleEmote(EMOTE_ONESHOT_NONE);

                        m_uiPhaseTimer = 10000;
                        break;
                    case 29:                                // Empty Phase, used to store information about set charge (空阶段，用于存储冲锋信息)
                        m_uiPhaseTimer = 1;
                        break;
                    case 30:                                // shortly before starting WP 18
                        DoScriptText(SAY_CHARGE_4, m_creature);
                        m_uiPhaseTimer = 0;
                        break;

                    case 31:                                // shortly after reaching WP 19
                        if (m_pInstance)
                        {
                            if (GameObject* pDoor = m_pInstance->GetSingleGameObjectFromStorage(GO_CAVE_IN_NORTH))
                                m_creature->SetFacingToObject(pDoor);
                        }
                        DoScriptText(SAY_BLOW_2_10, m_creature);
                        m_uiPhaseTimer = 5000;
                        break;
                    case 32:
                        DoScriptText(SAY_BLOW_2_5, m_creature);
                        m_uiPhaseTimer = 1000;
                        break;
                    case 33:
                        DoSummonPack(7);                    // Summon Grubbis and add (召唤 Grubbis and add)
                        m_uiPhaseTimer = 0;
                        break;

                    case 34:                                // 1 sek after Death of Grubbis
                        if (m_pInstance)
                        {
                            if (GameObject* pDoor = m_pInstance->GetSingleGameObjectFromStorage(GO_CAVE_IN_NORTH))
                                m_creature->SetFacingToObject(pDoor);
                        }
                        m_creature->HandleEmote(EMOTE_ONESHOT_CHEER);
                        m_uiPhaseTimer = 5000;
                        break;
                    case 35:
                        DoScriptText(SAY_BLOW_SOON, m_creature);
                        m_uiPhaseTimer = 5000;
                        break;
                    case 36:
                        DoScriptText(SAY_BLOW_2, m_creature);
                        m_uiPhaseTimer = 2000;
                        break;
                    case 37:
                        m_creature->HandleEmote(EMOTE_ONESHOT_POINT);
                        m_uiPhaseTimer = 1000;
                        break;
                    case 38:
                        DoCastSpellIfCan(m_creature, SPELL_EXPLOSION_NORTH);
                        m_uiPhaseTimer = 500;
                        break;
                    case 39:
                        // Close northern cave-in and let charges explode
                        if (m_pInstance)
                        {
                            m_pInstance->DoUseDoorOrButton(GO_CAVE_IN_NORTH);
                            m_bNorthernCaveInOpened = false;
                            m_pInstance->SetData(TYPE_EXPLOSIVE_CHARGE, DATA_EXPLOSIVE_CHARGE_USE);
                        }
                        m_uiPhaseTimer = 8000;
                        break;
                    case 40:
                        DoCastSpellIfCan(m_creature, SPELL_FIREWORKS_RED);
                        DoScriptText(SAY_FINISH_2, m_creature);
                        m_uiPhaseTimer = 0;
                        break;
                }
                ++m_uiPhase;
            }
            else
                m_uiPhaseTimer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->GetVictim())
            return;

        DoMeleeAttackIfReady();                             // 近战普通攻击判定
    }
};

// 创建并返回 AI 实例工厂函数：npc_blastmaster_emi_shortfuse
UnitAI* GetAI_npc_blastmaster_emi_shortfuse(Creature* pCreature)
{
    return new npc_blastmaster_emi_shortfuseAI(pCreature);
}

// NPC 对话交互入口函数：npc_blastmaster_emi_shortfuse
bool GossipHello_npc_blastmaster_emi_shortfuse(Player* pPlayer, Creature* pCreature)
{
    if (instance_gnomeregan* pInstance = (instance_gnomeregan*)pCreature->GetInstanceData())
    {
        if (pInstance->GetData(TYPE_GRUBBIS) == NOT_STARTED || pInstance->GetData(TYPE_GRUBBIS) == FAIL)
        {
            pPlayer->ADD_GOSSIP_ITEM_ID(GOSSIP_ICON_CHAT, GOSSIP_ITEM_START, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            pPlayer->SEND_GOSSIP_MENU(pPlayer->GetGossipTextId(pCreature), pCreature->GetObjectGuid());
        }
    }
    return true;
}

bool GossipSelect_npc_blastmaster_emi_shortfuse(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction)
{
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 1)
    {
        if (instance_gnomeregan* pInstance = (instance_gnomeregan*)pCreature->GetInstanceData())
        {
            if (pInstance->GetData(TYPE_GRUBBIS) == NOT_STARTED || pInstance->GetData(TYPE_GRUBBIS) == FAIL)
            {
                if (npc_blastmaster_emi_shortfuseAI* pEmiAI = dynamic_cast<npc_blastmaster_emi_shortfuseAI*>(pCreature->AI()))
                    pEmiAI->StartEvent(pPlayer);
            }
        }
    }
    pPlayer->CLOSE_GOSSIP_MENU();

    return true;
}

/*######
## npc_kernobee
## TODO: It appears there are some things missing, including his? alarm-bot
######*/

// 枚举定义：法术技能、生物与环境事件标识
enum
{
    QUEST_A_FINE_MESS           = 2904,                     // afinemess 任务 ID
    TRIGGER_GNOME_EXIT          = 324,                      // Add scriptlib support for it, atm simply use hardcoded values
};

static const float aKernobeePositions[2][3] =
{
    { -330.92f, -3.03f, -152.85f},                          // End position
    { -297.32f, -7.32f, -152.85f}                           // Walk out of the door
};

// 随从/NPC AI：标识 ID（kernobee）
struct npc_kernobeeAI : public FollowerAI
{
    npc_kernobeeAI(Creature* pCreature) : FollowerAI(pCreature)
    {
        m_uiCheckEndposTimer = 10000;
        Reset();
    }

    uint32 m_uiCheckEndposTimer;                            // CheckEndpos 技能计时器

    // 重置战斗状态与技能计时器
    void Reset() override {}

    void ReceiveAIEvent(AIEventType eventType, Unit* /*pSender*/, Unit* pInvoker, uint32 uiMiscValue) override
    {
        if (eventType == AI_EVENT_START_EVENT && pInvoker->GetTypeId() == TYPEID_PLAYER)
        {
            // No idea why he has UNIT_STAND_STATE_DEAD in UDB ..
            m_creature->SetStandState(UNIT_STAND_STATE_STAND);
            StartFollow((Player*)pInvoker, 0, GetQuestTemplateStore(uiMiscValue));
        }
    }

    void UpdateFollowerAI(const uint32 uiDiff) override
    {
        FollowerAI::UpdateFollowerAI(uiDiff);               // Do combat handling

        if (m_creature->IsInCombat() || !HasFollowState(STATE_FOLLOW_INPROGRESS) || HasFollowState(STATE_FOLLOW_COMPLETE))
            return;

        if (m_uiCheckEndposTimer < uiDiff)
        {
            m_uiCheckEndposTimer = 500;
            if (m_creature->IsWithinDist3d(aKernobeePositions[0][0], aKernobeePositions[0][1], aKernobeePositions[0][2], 2 * INTERACTION_DISTANCE))
            {
                SetFollowComplete(true);
                if (Player* pPlayer = GetLeaderForFollower())
                    pPlayer->RewardPlayerAndGroupAtEventExplored(QUEST_A_FINE_MESS, m_creature);
                m_creature->GetMotionMaster()->MovePoint(1, aKernobeePositions[1][0], aKernobeePositions[1][1], aKernobeePositions[1][2]);
                m_creature->ForcedDespawn(2000);
            }
        }
        else
            m_uiCheckEndposTimer -= uiDiff;
    }
};

// 创建并返回 AI 实例工厂函数：npc_kernobee
UnitAI* GetAI_npc_kernobee(Creature* pCreature)
{
    return new npc_kernobeeAI(pCreature);
}

// 任务接取回调函数：npc_kernobee
bool QuestAccept_npc_kernobee(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_A_FINE_MESS)
        pCreature->AI()->SendAIEvent(AI_EVENT_START_EVENT, pPlayer, pCreature, pQuest->GetQuestId());

    return true;
}

// 注册并载入脚本模块：gnomeregan
void AddSC_gnomeregan()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_blastmaster_emi_shortfuse";
    pNewScript->GetAI = &GetAI_npc_blastmaster_emi_shortfuse;
    pNewScript->pGossipHello = &GossipHello_npc_blastmaster_emi_shortfuse;
    pNewScript->pGossipSelect = &GossipSelect_npc_blastmaster_emi_shortfuse;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_kernobee";
    pNewScript->GetAI = &GetAI_npc_kernobee;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_kernobee;
    pNewScript->RegisterSelf();
}
