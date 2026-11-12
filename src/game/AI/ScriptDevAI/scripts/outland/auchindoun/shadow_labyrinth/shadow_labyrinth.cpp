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
SDName: Instance_Shadow_Labyrinth
SD%Complete: 85
SDComment:
SDCategory: Auchindoun, Shadow Labyrinth
EndScriptData */

// 暗影迷宫副本数据管理实现
/* 副本概述：
 * 暗影迷宫是奥金顿地下城之一，包含赫尔默大使、煽动者布莱卡特、沃匹尔大师与摩摩尔四个首领遭遇战。
 */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "shadow_labyrinth.h"
#include "Spells/Scripts/SpellScript.h"
#include "World/WorldStateDefines.h"

/* Shadow Labyrinth encounters:
1 - Ambassador Hellmaw event (1 - 赫尔默大使事件)
2 - Blackheart the Inciter event (2 - 煽动者布莱卡特事件)
3 - Grandmaster Vorpil event (3 - 沃匹尔大师事件)
4 - Murmur event (4 - 摩摩尔事件)
*/

// 构造函数：初始化暗影迷宫副本数据
instance_shadow_labyrinth::instance_shadow_labyrinth(Map* pMap) : ScriptedInstance(pMap)
{
    Initialize();
}

// 初始化首领战进度数组
void instance_shadow_labyrinth::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

// 游戏对象创建登记：恢复大门状态
void instance_shadow_labyrinth::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
        case GO_REFECTORY_DOOR:
            if (m_auiEncounter[TYPE_HELLMAW] == DONE)
                pGo->SetGoState(GO_STATE_ACTIVE);
            break;
        default:
            return;
    }

    m_goEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

// 生物创建登记：记录首领与关键实体 GUID
void instance_shadow_labyrinth::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
        case NPC_VORPIL:
        case NPC_HELLMAW:
        case NPC_BLACKHEART_THE_INCITER:
        case NPC_MURMUR:
            m_npcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
            break;
        case NPC_CONTAINMENT_BEAM:
        case NPC_CABAL_SPELLBINDER:
        case NPC_CABAL_SUMMONER:
            m_npcEntryGuidCollection[pCreature->GetEntry()].push_back(pCreature->GetObjectGuid());
            break;
    }
}

// 设置首领战进度数据并触发门禁/存盘
void instance_shadow_labyrinth::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
        case TYPE_HELLMAW:
            m_auiEncounter[0] = uiData;
            break;

        case TYPE_INCITER:
            if (uiData == DONE)
                DoUseDoorOrButton(GO_REFECTORY_DOOR);
            m_auiEncounter[1] = uiData;
            break;

        case TYPE_VORPIL:
            m_auiEncounter[2] = uiData;
            break;

        case TYPE_MURMUR:
            m_auiEncounter[3] = uiData;
            break;
    }

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " "
                   << m_auiEncounter[2] << " " << m_auiEncounter[3];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

// 获取首领战进度数据
uint32 instance_shadow_labyrinth::GetData(uint32 uiType) const
{
    switch (uiType)
    {
        case TYPE_HELLMAW:  return m_auiEncounter[0];
        case TYPE_INCITER:  return m_auiEncounter[1];
        case TYPE_VORPIL:   return m_auiEncounter[2];
        case TYPE_MURMUR:   return m_auiEncounter[3];

        default:
            return 0;
    }
}

// 记录存活的苦修者 GUID
void instance_shadow_labyrinth::SetData64(uint32 uiData, uint64 uiGuid)
{
    // If Hellmaw already completed, just ignore (若赫尔默已击败则忽略)
    if (GetData(TYPE_HELLMAW) == DONE)
        return;

    // Note: this is handled in Acid. The purpose is check which Cabal Ritualists is alive, in case of server reset (检查苦修者存活)
    // The function is triggered by eventAI on generic timer
    if (uiData == DATA_CABAL_RITUALIST)
        m_sRitualistsAliveGUIDSet.insert(ObjectGuid(uiGuid));
}

// 生物死亡处理：所有暗影议会苦修者死亡后解除赫尔默大使的放逐状态
void instance_shadow_labyrinth::OnCreatureDeath(Creature* pCreature)
{
    // unbanish Hellmaw when all Cabal Ritualists are dead (苦修者死尽时解除赫尔默放逐)
    if (pCreature->GetEntry() == NPC_CABAL_RITUALIST)
    {
        m_sRitualistsAliveGUIDSet.erase(pCreature->GetObjectGuid());
        // TODO: Make containment beam collapse when a group of ritualists dies

        if (m_sRitualistsAliveGUIDSet.empty())
        {
            if (Creature* hellmaw = GetSingleCreatureFromStorage(NPC_HELLMAW))
                hellmaw->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, hellmaw, hellmaw);

            GuidVector containmentVector;
            GetCreatureGuidVectorFromStorage(NPC_CONTAINMENT_BEAM, containmentVector);
            for (ObjectGuid& guid : containmentVector)
            {
                if (Creature* pBeam = pCreature->GetMap()->GetCreature(guid))
                    pBeam->ForcedDespawn();
            }
        }
    }
}

// 生物重生处理：重置赫尔默大使状态
void instance_shadow_labyrinth::OnCreatureRespawn(Creature* creature)
{
    switch (creature->GetEntry())
    {
        case NPC_HELLMAW:
            creature->AI()->SendAIEvent(AI_EVENT_CUSTOM_B, creature, creature);
            break;
    }
}

// The Screaming Hall (尖啸大厅怪物组团灭回调)
// All Worldstates get Activated via database when door opens
void instance_shadow_labyrinth::OnCreatureGroupDespawn(CreatureGroup* group, Creature* /*pCreature*/)
{

    switch (group->GetGroupId())
    {
        // Blackheart the Inciter room (布莱卡特房间怪物组)
        // All groups have a chance to spawn assasins (怪物群有几率激活刺客)
        case SL_BLACKHEAT_GROUP_01:
        case SL_BLACKHEAT_GROUP_02:
        case SL_BLACKHEAT_GROUP_03:
        case SL_BLACKHEAT_GROUP_04:
        case SL_BLACKHEAT_GROUP_05:
        case SL_BLACKHEAT_GROUP_06:
        case SL_BLACKHEAT_GROUP_07:
        case SL_BLACKHEAT_GROUP_08:
        case SL_BLACKHEAT_GROUP_09:
        case SL_BLACKHEAT_GROUP_10:
        {
            switch (urand(0, 5))
            {
                case 0:
                instance->GetVariableManager().SetVariable(WORLD_STATE_SHADOW_LAB_ASSASIN_01, 1);
            }
            
            switch (urand(0, 5))
            {
                case 0:
                instance->GetVariableManager().SetVariable(WORLD_STATE_SHADOW_LAB_ASSASIN_02, 1);
            }
            break;
        }
        // Murmur Room (摩摩尔房间怪物组)
        case SL_SPAWN_GROUP_043:
            instance->GetVariableManager().SetVariable(WORLD_STATE_SHADOW_LAB_GROUP_49, 0);
            break;
        case SL_SPAWN_GROUP_044:
            instance->GetVariableManager().SetVariable(WORLD_STATE_SHADOW_LAB_GROUP_50, 0);
            break;
        case SL_SPAWN_GROUP_045:
            instance->GetVariableManager().SetVariable(WORLD_STATE_SHADOW_LAB_GROUP_51, 0);
            break;
        case SL_SPAWN_GROUP_046:
            instance->GetVariableManager().SetVariable(WORLD_STATE_SHADOW_LAB_GROUP_52, 0);
            break;
    }
}

// 读取副本存盘数据
void instance_shadow_labyrinth::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >> m_auiEncounter[3];

    for (uint32& i : m_auiEncounter)
    {
        if (i == IN_PROGRESS)
            i = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

// 获取暗影迷宫副本数据脚本实例
InstanceData* GetInstanceData_instance_shadow_labyrinth(Map* pMap)
{
    return new instance_shadow_labyrinth(pMap);
}

// 尖啸大厅大门 AI：当沃匹尔击败且玩家靠近 35 码内自动开门
struct go_screaming_hall_door : public GameObjectAI
{
    go_screaming_hall_door(GameObject* go) : GameObjectAI(go), m_doorCheckNearbyPlayersTimer(1000), m_doorOpen(false)
    {
        m_pInstance = (ScriptedInstance*)go->GetInstanceData();
    }

    ScriptedInstance* m_pInstance;                       // 副本数据指针
    uint32 m_doorCheckNearbyPlayersTimer;               // 检查周围玩家计时器
    bool m_doorOpen;                                    // 门是否已开启

    void UpdateAI(const uint32 diff) override           // 玩家距离检测步进
    {
        if (m_doorOpen)
            return;

        if (m_doorCheckNearbyPlayersTimer <= diff)
        {
            // If player is in 35y range of door, open it if Vorpil boss is done (玩家处于35码内且沃匹尔已死则开门)
            if (m_go->GetInstanceData()->GetData(TYPE_VORPIL) == DONE)
            {
                m_go->GetMap()->ExecuteDistWorker(m_go, 35.0f, [&](Player * player)
                {
                    if (m_doorOpen)
                        return;
                    m_go->Use(player);
                    m_doorOpen = true;

                    if (Creature* pMurmur = m_pInstance->GetSingleCreatureFromStorage(NPC_MURMUR))
                        pMurmur->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, pMurmur, pMurmur);
                });
            }
            m_doorCheckNearbyPlayersTimer = 1000;
        }
        else
            m_doorCheckNearbyPlayersTimer -= diff;
    }
};

// 获取尖啸大厅大门 AI 实例
GameObjectAI* GetAIgo_screaming_hall_door(GameObject* go)
{
    return new go_screaming_hall_door(go);
}

// 猛兽形态法术光环脚本
struct ShapeOfTheBeast : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (apply)
            aura->GetCaster()->CastSpell(aura->GetCaster(), SPELL_SHAPE_OF_BEAST, TRIGGERED_OLD_TRIGGERED);
        else
            aura->GetCaster()->RemoveAurasDueToSpell(SPELL_SHAPE_OF_BEAST);
    }
};

// 注册暗影迷宫副本与大门脚本
void AddSC_instance_shadow_labyrinth()
{
    RegisterSpellScript<ShapeOfTheBeast>("spell_shape_of_the_beast");

    Script* pNewScript = new Script;
    pNewScript->Name = "instance_shadow_labyrinth";
    pNewScript->GetInstanceData = &GetInstanceData_instance_shadow_labyrinth;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "go_screaming_hall_door";
    pNewScript->GetGameObjectAI = &GetAIgo_screaming_hall_door;
    pNewScript->RegisterSelf();
}
