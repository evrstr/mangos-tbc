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
SDName: Instance - Sethekk Halls
SD%Complete: 60
SDComment: Summoning event for Anzu NYI
SDCategory: Auchindoun, Sethekk Halls
EndScriptData */

// 塞泰克大厅副本数据管理实现
/* 副本概述：
 * 塞泰克大厅是奥金顿的地下城之一，包含暗织者斯瑟、利爪之王艾吉斯以及德鲁伊史诗飞行形态任务召唤首领安苏。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "sethekk_halls.h"
#include "Entities/TemporarySpawn.h"
#include "World/WorldStateDefines.h"

// 构造函数：初始化副本数据
instance_sethekk_halls::instance_sethekk_halls(Map* pMap) : ScriptedInstance(pMap), m_anzuTimer(0), m_anzuStage(0)
{
    Initialize();
}

// 初始化首领进度数组
void instance_sethekk_halls::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

// 副本生物创建时记录安苏召唤仪式各引导实体
void instance_sethekk_halls::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
        case NPC_INVIS_RAVEN_GOD_CASTER:
            m_npcEntryGuidCollection[pCreature->GetEntry()].push_back(pCreature->GetObjectGuid());
            break;
        case NPC_INVIS_RAVEN_GOD_PORTAL:
        case NPC_INVIS_RAVEN_GOD_TARGET:
        case NPC_ANZU:
            m_npcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
            break;
    }
}

// 副本游戏对象创建时恢复门禁与宝箱状态
void instance_sethekk_halls::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
        case GO_IKISS_SWING_DOOR:
            if (m_auiEncounter[TYPE_IKISS] == DONE)
                pGo->SetGoState(GO_STATE_ACTIVE);
            break;
        case GO_IKISS_CHEST:
            if (m_auiEncounter[TYPE_IKISS] == DONE)
                pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT | GO_FLAG_INTERACT_COND);
            break;
        case GO_RAVENS_CLAW:
        case GO_MOONSTONE:
        case GO_TEST_RIFT:
            break;
        default:
            return;
    }

    m_goEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

// 设置首领战进度数据并触发门禁/存盘
void instance_sethekk_halls::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
        case TYPE_SYTH:
            m_auiEncounter[uiType] = uiData;
            break;
        case TYPE_ANZU:
            m_auiEncounter[uiType] = uiData;
            // Respawn the Raven's Claw if event fails (事件失败时重新刷新乌鸦之爪)
            if (uiData == FAIL)
            {
                if (GameObject* pClaw = GetSingleGameObjectFromStorage(GO_RAVENS_CLAW))
                    pClaw->Respawn();
            }
            break;
        case TYPE_IKISS:
            if (uiData == DONE)
            {
                DoUseDoorOrButton(GO_IKISS_SWING_DOOR, DAY);
                DoToggleGameObjectFlags(GO_IKISS_CHEST, GO_FLAG_NO_INTERACT | GO_FLAG_INTERACT_COND, false);
            }
            m_auiEncounter[uiType] = uiData;
            break;
        default:
            return;
    }

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " " << m_auiEncounter[2];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

// 获取首领战进度数据
uint32 instance_sethekk_halls::GetData(uint32 uiType) const
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];

    return 0;
}

// 读取副本存盘数据
void instance_sethekk_halls::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2];

    for (uint32& i : m_auiEncounter)
    {
        if (i == IN_PROGRESS)
            i = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

// 安苏召唤过场动画法术枚举
enum AnzuIntro
{
    TRIGGER_COUNT = 14,                                 // 触发器数量

    SPELL_OTHERWORLDLY_PORTAL = 39952,                  // 异界传送门
    SPELL_PURPLE_BANISH_STATE = 32566,                  // 紫色放逐状态视觉
    SPELL_RAVEN_GOD_SUMMON_BEAMS = 39978,               // 乌鸦之神召唤光束
    SPELL_ULTRIS_DESTROYED = 35757,                     // 虚空湮灭法术
    SPELL_RED_LIGHTNING_BOLT = 39990,                   // 红色闪电箭
};

// 召唤仪式各施法者点位坐标
float triggerPositions[TRIGGER_COUNT][4] =
{
    { -55.26986f, 313.4585f, 50.90593f, 0.3316126f},
    { -59.17055f, 314.39f, 51.65655f, 0.541052f},
    { -79.32272f, 314.4245f, 50.42631f, 2.181662f},
    { -86.93535f, 314.4072f, 50.36619f, 5.916666f},
    { -82.65174f, 314.4917f, 49.48673f, 0.4712389f},
    { -110.0913f, 314.4524f, 50.17954f, 1.43117f},
    { -113.7065f, 314.51f, 51.44148f, 0.2617994f},
    { -112.4148f, 258.7079f, 50.66346f, 0.4363323f},
    { -108.5766f, 258.7865f, 51.30656f, 3.124139f},
    { -88.22328f, 258.7913f, 50.12397f, 2.600541f},
    { -84.26708f, 258.7147f, 49.33371f, 4.328416f},
    { -80.07829f, 258.7955f, 50.39458f, 0.2268928f},
    { -59.91186f, 259.3824f, 50.82623f, 1.37881f},
    { -55.34531f, 258.1663f, 51.48593f, 1.623156f},
};

// 启动安苏召唤仪式过场
void instance_sethekk_halls::StartAnzuIntro(Player* player)
{
    Creature* target = player->SummonCreature(NPC_INVIS_RAVEN_GOD_TARGET, -87.08602f, 287.8433f, 83.77484f, 3.455752f, TEMPSPAWN_MANUAL_DESPAWN, 0, true);
    target->CastSpell(nullptr, SPELL_PURPLE_BANISH_STATE, TRIGGERED_NONE);
    Creature* portal = player->SummonCreature(NPC_INVIS_RAVEN_GOD_PORTAL, -87.28478f, 287.6349f, 27.17761f, 0.5759587f, TEMPSPAWN_MANUAL_DESPAWN, 0, true);
    portal->CastSpell(nullptr, SPELL_OTHERWORLDLY_PORTAL, TRIGGERED_NONE);
    for (uint32 i = 0; i < TRIGGER_COUNT; ++i)
    {
        Creature* caster = player->SummonCreature(NPC_INVIS_RAVEN_GOD_CASTER, triggerPositions[i][0], triggerPositions[i][1], triggerPositions[i][2], triggerPositions[i][3], TEMPSPAWN_MANUAL_DESPAWN, 0, true);
        caster->CastSpell(nullptr, SPELL_RAVEN_GOD_SUMMON_BEAMS, TRIGGERED_NONE);
    }
    GameObject* moonstone = GetSingleGameObjectFromStorage(GO_MOONSTONE);
    moonstone->SetLootState(GO_READY);
    moonstone->SetRespawnTime(2 * MINUTE * IN_MILLISECONDS);
    moonstone->Refresh();
    GameObject* testRift = GetSingleGameObjectFromStorage(GO_TEST_RIFT);
    testRift->SetLootState(GO_READY);
    testRift->SetRespawnTime(2 * MINUTE * IN_MILLISECONDS);
    testRift->Refresh();
    m_anzuTimer = 6000;
    m_anzuStage = 0;
}

// 完成安苏召唤仪式并生成安苏首领实体
void instance_sethekk_halls::FinishAnzuIntro()
{
    Creature* target = GetSingleCreatureFromStorage(NPC_INVIS_RAVEN_GOD_TARGET, true);
    target->SummonCreature(NPC_ANZU, -89.20406f, 287.9736f, 26.5665f, 3.001966f, TEMPSPAWN_MANUAL_DESPAWN, 0);
    static_cast<TemporarySpawn*>(target)->UnSummon();
    GuidVector casterVector;
    GetCreatureGuidVectorFromStorage(NPC_INVIS_RAVEN_GOD_CASTER, casterVector);
    for (ObjectGuid guid : casterVector)
        if (Creature* caster = instance->GetCreature(guid))
            static_cast<TemporarySpawn*>(caster)->UnSummon();
    Creature* portal = GetSingleCreatureFromStorage(NPC_INVIS_RAVEN_GOD_PORTAL, true);
    static_cast<TemporarySpawn*>(portal)->UnSummon();
    GameObject* ravensClaw = GetSingleGameObjectFromStorage(GO_RAVENS_CLAW);
    ravensClaw->SetRespawnTime(7 * DAY);
    ravensClaw->SetLootState(GO_JUST_DEACTIVATED);
    GameObject* moonstone = GetSingleGameObjectFromStorage(GO_MOONSTONE);
    moonstone->SetLootState(GO_JUST_DEACTIVATED);
    GameObject* testRift = GetSingleGameObjectFromStorage(GO_TEST_RIFT);
    testRift->SetLootState(GO_JUST_DEACTIVATED);
}

// 仪式过场阶段时间步进循环
void instance_sethekk_halls::Update(uint32 diff)
{
    if (m_anzuTimer)
    {
        if (m_anzuTimer <= diff)
        {
            switch (m_anzuStage)
            {
                case 0: // make target move (阶段 0：仪式目标移动)
                {
                    Creature * target = GetSingleCreatureFromStorage(NPC_INVIS_RAVEN_GOD_TARGET, true);
                    target->GetMotionMaster()->MovePoint(0, -87.14738f, 287.8225f, 33.48315f);
                    m_anzuTimer = 20000;
                    break;
                }
                case 1: // 阶段 1：施放湮灭视觉法术
                {
                    Creature* target = GetSingleCreatureFromStorage(NPC_INVIS_RAVEN_GOD_TARGET, true);
                    target->CastSpell(nullptr, SPELL_ULTRIS_DESTROYED, TRIGGERED_NONE);
                    m_anzuTimer = 9000;
                    break;
                }
                case 2: // 阶段 2：施放红色闪电箭
                {
                    Creature * target = GetSingleCreatureFromStorage(NPC_INVIS_RAVEN_GOD_TARGET, true);
                    target->CastSpell(nullptr, SPELL_RED_LIGHTNING_BOLT, TRIGGERED_NONE);
                    m_anzuTimer = 1000;
                    break;
                }
                case 3: // done (阶段 3：仪式完成)
                    FinishAnzuIntro();
                    break;
            }
            ++m_anzuStage;
        }
        else m_anzuTimer -= diff;
    }
}

// 处理玩家使用月亮石触发安苏召唤仪式事件
bool ProcessEventId_event_spell_summon_raven_god(uint32 /*uiEventId*/, Object* source, Object* /*pTarget*/, bool bIsStart)
{
    if (bIsStart && source->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = static_cast<Player*>(source);
        if (instance_sethekk_halls* instance = (instance_sethekk_halls*)(player->GetInstanceData()))
        {
            // This should be checked by despawning the Raven Claw Go; However it's better to double check the condition (防止重复召唤)
            if (instance->GetData(TYPE_ANZU) == DONE || instance->GetData(TYPE_ANZU) == IN_PROGRESS)
                return true;

            // Don't summon him twice (防止重复召唤)
            if (instance->GetSingleCreatureFromStorage(NPC_ANZU, true))
                return true;

            instance->StartAnzuIntro(player);
        }
    }
    return false;
}

// 获取塞泰克大厅副本数据脚本实例
InstanceData* GetInstanceData_instance_sethekk_halls(Map* pMap)
{
    return new instance_sethekk_halls(pMap);
}

// 注册塞泰克大厅副本脚本
void AddSC_instance_sethekk_halls()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "instance_sethekk_halls";
    pNewScript->GetInstanceData = &GetInstanceData_instance_sethekk_halls;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "event_spell_summon_raven_god";
    pNewScript->pProcessEventId = &ProcessEventId_event_spell_summon_raven_god;
    pNewScript->RegisterSelf();
}
