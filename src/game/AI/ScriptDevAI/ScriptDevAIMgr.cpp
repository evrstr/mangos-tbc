/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
* This program is free software licensed under GPL version 2
* Please see the included DOCS/LICENSE.TXT for more information */

#include "include/sc_common.h"
#include "Policies/Singleton.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include "Server/DBCStores.h"
#include "Globals/ObjectMgr.h"
#include "Util/ProgressBar.h"
#include "system/system.h"
#include "ScriptDevAIMgr.h"
#include "include/sc_creature.h"

#ifdef BUILD_SCRIPTDEV
#include "system/ScriptLoader.h"
#endif

INSTANTIATE_SINGLETON_1(ScriptDevAIMgr);

void FillSpellSummary();                                    // 预填充法术信息摘要

// 从数据库加载脚本文本、自定义文本、对话文本与路径点数据
void LoadDatabase()
{
    // Load content
    // pSystemMgr.LoadVersion(); // currently we are not checking for version; function to be completely removed in the future
    pSystemMgr.LoadScriptTexts();                           // 加载核心脚本文本 (script_texts)
    pSystemMgr.LoadScriptTextsCustom();                     // 加载自定义脚本文本 (custom_texts)
    pSystemMgr.LoadScriptGossipTexts();                     // 加载脚本对话文本 (gossip_texts)
    pSystemMgr.LoadScriptWaypoints();                       // 加载脚本路径点数据 (script_waypoints)
}

//*********************************
//*** Functions used globally ***
// 全局调用辅助函数

/**
* Function that does script text (播放脚本文本台词)
*
* @param iTextEntry Entry of the text, stored in SD2-database (脚本文本条目 ID，存储在 SD2 数据库中，必须为负数)
* @param pSource Source of the text (文本说话来源对象)
* @param pTarget Can be nullptr (depending on CHAT_TYPE of iTextEntry). Possible target for the text (受众目标，可为空)
*/
void DoScriptText(int32 iTextEntry, WorldObject* pSource, Unit* pTarget, uint32 chatTypeOverride)
{
    if (!pSource)
    {
        script_error_log("DoScriptText entry %i, invalid Source pointer.", iTextEntry);
        return;
    }

    if (iTextEntry >= 0)
    {
        script_error_log("DoScriptText with source entry %u (TypeId=%u, guid=%u) attempts to process text entry %i, but text entry must be negative.",
                         pSource->GetEntry(), pSource->GetTypeId(), pSource->GetGUIDLow(), iTextEntry);
        return;
    }

    DoDisplayText(pSource, iTextEntry, pTarget, chatTypeOverride);
}

/**
* Function that does broadcast text (播放广播台词)
*
* @param iTextEntry Entry of the text (广播文本条目 ID，必须为正数)
* @param pSource Source of the text (说话来源对象)
* @param pTarget Can be nullptr (depending on CHAT_TYPE of iTextEntry). Possible target for the text (受众目标，可为空)
*/
void DoBroadcastText(int32 iTextEntry, WorldObject* pSource, Unit* pTarget, uint32 chatTypeOverride)
{
    if (!pSource)
    {
        script_error_log("DoScriptText entry %i, invalid Source pointer.", iTextEntry);
        return;
    }

    if (iTextEntry <= 0)
    {
        script_error_log("DoBroadcastText with source entry %u (TypeId=%u, guid=%u) attempts to process text entry %i, but text entry must be positive.",
                         pSource->GetEntry(), pSource->GetTypeId(), pSource->GetGUIDLow(), iTextEntry);
        return;
    }

    DoDisplayText(pSource, iTextEntry, pTarget, chatTypeOverride);
}

/**
* Function that either simulates or does script text for a map (在整张地图上播放或模拟生物喊话/台词)
*
* @param iTextEntry Entry of the text, stored in SD2-database, only type CHAT_TYPE_ZONE_YELL supported (仅支持区域大喊类型)
* @param uiCreatureEntry Id of the creature of whom saying will be simulated (模拟说话的生物模板 ID)
* @param pMap Given Map on which the map-wide text is displayed (目标地图指针)
* @param pCreatureSource Can be nullptr. If pointer to Creature is given, then the creature does the map-wide text (说话生物实例，若为空则模拟)
* @param pTarget Can be nullptr. Possible target for the text (受众目标，可为空)
*/
void DoOrSimulateScriptTextForMap(int32 iTextEntry, uint32 uiCreatureEntry, Map* pMap, Creature* pCreatureSource /*=nullptr*/, Unit* pTarget /*=nullptr*/)
{
    if (!pMap)
    {
        script_error_log("DoOrSimulateScriptTextForMap entry %i, invalid Map pointer.", iTextEntry);
        return;
    }

    if (iTextEntry >= 0)
    {
        script_error_log("DoOrSimulateScriptTextForMap with source entry %u for map %u attempts to process text entry %i, but text entry must be negative.", uiCreatureEntry, pMap->GetId(), iTextEntry);
        return;
    }

    CreatureInfo const* pInfo = GetCreatureTemplateStore(uiCreatureEntry);
    if (!pInfo)
    {
        script_error_log("DoOrSimulateScriptTextForMap has invalid source entry %u for map %u.", uiCreatureEntry, pMap->GetId());
        return;
    }

    MangosStringLocale const* pData = GetMangosStringData(iTextEntry);
    if (!pData)
    {
        script_error_log("DoOrSimulateScriptTextForMap with source entry %u for map %u could not find text entry %i.", uiCreatureEntry, pMap->GetId(), iTextEntry);
        return;
    }

    debug_log("SD2: DoOrSimulateScriptTextForMap: text entry=%i, Sound=%u, Type=%u, Language=%u, Emote=%u",
              iTextEntry, pData->SoundId, pData->Type, pData->LanguageId, pData->Emote);

    if (pData->Type != CHAT_TYPE_ZONE_YELL && pData->Type != CHAT_TYPE_ZONE_EMOTE)
    {
        script_error_log("DoSimulateScriptTextForMap entry %i has not supported chat type %u.", iTextEntry, pData->Type);
        return;
    }

    if (pData->SoundId)
        pMap->PlayDirectSoundToMap(pData->SoundId);

    ChatMsg chatMsg = (pData->Type == CHAT_TYPE_ZONE_EMOTE ? CHAT_MSG_MONSTER_EMOTE : CHAT_MSG_MONSTER_YELL);

    if (pCreatureSource)                                // If provided pointer for sayer, use direct version
        pMap->MonsterYellToMap(pCreatureSource->GetObjectGuid(), iTextEntry, chatMsg, pData->LanguageId, pTarget);
    else                                                // Simulate yell
        pMap->MonsterYellToMap(pInfo, iTextEntry, chatMsg, pData->LanguageId, pTarget);
}

//*********************************
//*** Functions used internally ***
// 内部调用函数

// 脚本自注册实现：通过脚本名称检索 ID 并存入脚本管理器，若未在数据库注册则销毁自身
void Script::RegisterSelf(bool bReportError)
{
    if (uint32 id = sScriptDevAIMgr.GetScriptId(Name.c_str()))
        sScriptDevAIMgr.AddScript(id, this);
    else
    {
        if (bReportError)
            script_error_log("Script registering but ScriptName %s is not assigned in database. Script will not be used.", Name.c_str());

        delete this;
    }
}

//********************************
//*** Functions to be Exported ***
// 核心事件导出分发接口

// 处理玩家与 NPC 对话窗口打开事件（问候）
bool ScriptDevAIMgr::OnGossipHello(Player* pPlayer, Creature* pCreature)
{
    Script* pTempScript = GetScript(pCreature->GetScriptId());

    if (!pTempScript || !pTempScript->pGossipHello)
        return false;

    pPlayer->GetPlayerMenu()->ClearMenus();

    return pTempScript->pGossipHello(pPlayer, pCreature);
}

// 处理玩家与游戏对象对话窗口打开事件
bool ScriptDevAIMgr::OnGossipHello(Player* pPlayer, GameObject* pGo)
{
    Script* pTempScript = GetScript(pGo->GetGOInfo()->ScriptId);

    if (!pTempScript || !pTempScript->pGossipHelloGO)
        return false;

    pPlayer->GetPlayerMenu()->ClearMenus();

    return pTempScript->pGossipHelloGO(pPlayer, pGo);
}

// 处理玩家选择 NPC 对话菜单选项事件（支持普通选项与密码/代码输入选项）
bool ScriptDevAIMgr::OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction, const char* code)
{
    debug_log("SD2: Gossip selection, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = GetScript(pCreature->GetScriptId());

    if (!pTempScript)
        return false;

    if (code)
    {
        if (!pTempScript->pGossipSelectWithCode)
            return false;

        pPlayer->GetPlayerMenu()->ClearMenus();
        return pTempScript->pGossipSelectWithCode(pPlayer, pCreature, uiSender, uiAction, code);
    }

    if (!pTempScript->pGossipSelect)
        return false;

    pPlayer->GetPlayerMenu()->ClearMenus();
    return pTempScript->pGossipSelect(pPlayer, pCreature, uiSender, uiAction);
}

// 处理玩家选择游戏对象对话菜单选项事件（支持普通选项与密码/代码输入选项）
bool ScriptDevAIMgr::OnGossipSelect(Player* pPlayer, GameObject* pGo, uint32 uiSender, uint32 uiAction, const char* code)
{
    debug_log("SD2: GO Gossip selection, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = GetScript(pGo->GetGOInfo()->ScriptId);

    if (!pTempScript)
        return false;

    if (code)
    {
        if (!pTempScript->pGossipSelectGOWithCode)
            return false;

        pPlayer->GetPlayerMenu()->ClearMenus();
        return pTempScript->pGossipSelectGOWithCode(pPlayer, pGo, uiSender, uiAction, code);
    }

    if (!pTempScript->pGossipSelectGO)
        return false;

    pPlayer->GetPlayerMenu()->ClearMenus();
    return pTempScript->pGossipSelectGO(pPlayer, pGo, uiSender, uiAction);
}

// 获取 NPC 对当前玩家的对话与任务状态图标（问号/叹号等）
uint32 ScriptDevAIMgr::GetDialogStatus(const Player* pPlayer, const Creature* pCreature) const
{
    Script* pTempScript = GetScript(pCreature->GetScriptId());

    if (!pTempScript || !pTempScript->pDialogStatusNPC)
        return DIALOG_STATUS_UNDEFINED;

    pPlayer->GetPlayerMenu()->ClearMenus();

    return pTempScript->pDialogStatusNPC(pPlayer, pCreature);
}

// 获取游戏对象对当前玩家的对话与任务状态图标
uint32 ScriptDevAIMgr::GetDialogStatus(const Player* pPlayer, const GameObject* pGo) const
{
    Script* pTempScript = GetScript(pGo->GetGOInfo()->ScriptId);

    if (!pTempScript || !pTempScript->pDialogStatusGO)
        return DIALOG_STATUS_UNDEFINED;

    pPlayer->GetPlayerMenu()->ClearMenus();

    return pTempScript->pDialogStatusGO(pPlayer, pGo);
}

// 处理玩家从 NPC 处接受任务事件
bool ScriptDevAIMgr::OnQuestAccept(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    Script* pTempScript = GetScript(pCreature->GetScriptId());

    if (!pTempScript || !pTempScript->pQuestAcceptNPC)
        return false;

    pPlayer->GetPlayerMenu()->ClearMenus();

    return pTempScript->pQuestAcceptNPC(pPlayer, pCreature, pQuest);
}

// 处理玩家从游戏对象处接受任务事件
bool ScriptDevAIMgr::OnQuestAccept(Player* pPlayer, GameObject* pGo, const Quest* pQuest)
{
    Script* pTempScript = GetScript(pGo->GetGOInfo()->ScriptId);

    if (!pTempScript || !pTempScript->pQuestAcceptGO)
        return false;

    pPlayer->GetPlayerMenu()->ClearMenus();

    return pTempScript->pQuestAcceptGO(pPlayer, pGo, pQuest);
}

// 处理玩家从物品处接受任务事件
bool ScriptDevAIMgr::OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest)
{
    Script* pTempScript = GetScript(pItem->GetProto()->ScriptId);

    if (!pTempScript || !pTempScript->pQuestAcceptItem)
        return false;

    pPlayer->GetPlayerMenu()->ClearMenus();

    return pTempScript->pQuestAcceptItem(pPlayer, pItem, pQuest);
}

// 处理玩家与游戏对象交互使用事件
bool ScriptDevAIMgr::OnGameObjectUse(Player* pPlayer, GameObject* pGo)
{
    Script* pTempScript = GetScript(pGo->GetGOInfo()->ScriptId);

    if (!pTempScript || !pTempScript->pGOUse)
        return false;

    return pTempScript->pGOUse(pPlayer, pGo);
}

// 获取游戏对象陷阱搜索目标单位的自定义谓词函数
std::function<bool(Unit*)>* ScriptDevAIMgr::OnTrapSearch(GameObject* go)
{
    Script* pTempScript = GetScript(go->GetGOInfo()->ScriptId);

    if (!pTempScript)
        return nullptr;

    return pTempScript->pTrapSearching;
}

// 处理玩家在 NPC 处完成并交付任务领取奖励事件
bool ScriptDevAIMgr::OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest)
{
    Script* pTempScript = GetScript(pCreature->GetScriptId());

    if (!pTempScript || !pTempScript->pQuestRewardedNPC)
        return false;

    pPlayer->GetPlayerMenu()->ClearMenus();

    return pTempScript->pQuestRewardedNPC(pPlayer, pCreature, pQuest);
}

// 处理玩家在游戏对象处完成并交付任务领取奖励事件
bool ScriptDevAIMgr::OnQuestRewarded(Player* pPlayer, GameObject* pGo, Quest const* pQuest)
{
    Script* pTempScript = GetScript(pGo->GetGOInfo()->ScriptId);

    if (!pTempScript || !pTempScript->pQuestRewardedGO)
        return false;

    pPlayer->GetPlayerMenu()->ClearMenus();

    return pTempScript->pQuestRewardedGO(pPlayer, pGo, pQuest);
}

// 处理玩家触发区域触发器事件 (AreaTrigger)
bool ScriptDevAIMgr::OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry)
{
    Script* pTempScript = GetScript(GetAreaTriggerScriptId(atEntry->id));

    if (!pTempScript || !pTempScript->pAreaTrigger)
        return false;

    return pTempScript->pAreaTrigger(pPlayer, atEntry);
}

// 处理触发指定事件 ID 脚本（如飞行节点、法术触发等）
bool ScriptDevAIMgr::OnProcessEvent(uint32 uiEventId, Object* pSource, Object* pTarget, bool bIsStart)
{
    Script* pTempScript = GetScript(GetEventIdScriptId(uiEventId));

    if (!pTempScript || !pTempScript->pProcessEventId)
        return false;

    // bIsStart may be false, when event is from taxi node events (arrival=false, departure=true) (飞行节点事件中到达为 false，出发为 true)
    return pTempScript->pProcessEventId(uiEventId, pSource, pTarget, bIsStart);
}

// 根据生物 ScriptId 创建并返回对应的 UnitAI 实例
UnitAI* ScriptDevAIMgr::GetCreatureAI(Creature* pCreature) const
{
    Script* pTempScript = GetScript(pCreature->GetScriptId());

    if (!pTempScript || !pTempScript->GetAI)
        return nullptr;

    return pTempScript->GetAI(pCreature);
}

// 根据游戏对象 ScriptId 创建并返回对应的 GameObjectAI 实例
GameObjectAI* ScriptDevAIMgr::GetGameObjectAI(GameObject* gameobject) const
{
    Script* pTempScript = GetScript(gameobject->GetScriptId());

    if (!pTempScript || !pTempScript->GetGameObjectAI)
        return nullptr;

    return pTempScript->GetGameObjectAI(gameobject);
}

// 处理玩家使用物品脚本事件
bool ScriptDevAIMgr::OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets)
{
    Script* pTempScript = GetScript(pItem->GetProto()->ScriptId);

    if (!pTempScript || !pTempScript->pItemUse)
        return false;

    return pTempScript->pItemUse(pPlayer, pItem, targets);
}

// 处理玩家拾取物品脚本事件
bool ScriptDevAIMgr::OnItemLoot(Player* pPlayer, Item* pItem, bool apply)
{
    Script* pTempScript = GetScript(pItem->GetProto()->ScriptId);

    if (!pTempScript || !pTempScript->pItemLoot)
        return false;

    return pTempScript->pItemLoot(pPlayer, pItem, apply);
}

// 为地图创建副本数据管理脚本实例 (InstanceData)
InstanceData* ScriptDevAIMgr::CreateInstanceData(Map* pMap)
{
    Script* pTempScript = GetScript(pMap->GetScriptId());

    if (!pTempScript || !pTempScript->GetInstanceData)
        return nullptr;

    return pTempScript->GetInstanceData(pMap);
}

// 析构函数：释放所有脚本实例内存
ScriptDevAIMgr::~ScriptDevAIMgr()
{
    // Free resources before library unload (卸载前释放所有脚本对象)
    for (SDScriptVec::const_iterator itr = m_scripts.begin(); itr != m_scripts.end(); ++itr)
        delete *itr;

    m_scripts.clear();

    m_scriptCount = 0;

    setScriptLibraryErrorFile(nullptr, nullptr);
}

// 向指定 ID 槽位添加脚本指针
void ScriptDevAIMgr::AddScript(uint32 id, Script* script)
{
    if (!id)
        return;

    m_scripts[id] = script;
    ++m_scriptCount;
}

// 根据脚本 ID 获取脚本指针
Script* ScriptDevAIMgr::GetScript(uint32 id) const
{
#ifdef BUILD_SCRIPTDEV
    if (!id || id < m_scripts.size())
        return m_scripts[id];
#endif
    return nullptr;
}

// 初始化 ScriptDevAI 脚本管理器：加载数据库文本与所有 C++ 脚本
void ScriptDevAIMgr::Initialize()
{
#ifdef BUILD_SCRIPTDEV
    // ScriptDev startup (ScriptDev 启动横幅)
    outstring_log("  _____           _       _   _____ ");
    outstring_log(" / ____|         (_)     | | |  __ \\");
    outstring_log("| (___   ___ _ __ _ _ __ | |_| |  | | _____   __");
    outstring_log(" \\___ \\ / __| '__| | '_ \\| __| |  | |/ _ \\ \\ / /");
    outstring_log(" ____) | (__| |  | | |_) | |_| |__| |  __/\\ V / ");
    outstring_log("|_____/ \\___|_|  |_| .__/ \\__|_____/ \\___| \\_/ ");
    outstring_log("                   | |");
    outstring_log("                   |_| http://www.cmangos.net");



    // Load database (must be called after SD2Config.SetSource). (从数据库加载脚本文本，必须在配置源设置后调用)
    LoadDatabase();

    outstring_log("SD2: Loading C++ scripts\n");
    BarGoLink bar(1);
    bar.step();

    // Resize script ids to needed amount of assigned ScriptNames (from core) (将脚本容器大小调整为核心分配的脚本名称总数)
    m_scripts.resize(GetScriptIdsCount(), nullptr);

    FillSpellSummary();

    AddScripts();                                           // 注册所有 C++ 脚本
#else
    outstring_log(">> ScriptDev is disabled!\n");
#endif
}

// 从各个模板表和触发器表中加载所有唯一的 ScriptName
void ScriptDevAIMgr::LoadScriptNames()
{
    m_scriptNames.push_back("");
    auto queryResult = WorldDatabase.Query(
                              "SELECT DISTINCT(ScriptName) FROM creature_template WHERE ScriptName <> '' "
                              "UNION "
                              "SELECT DISTINCT(ScriptName) FROM gameobject_template WHERE ScriptName <> '' "
                              "UNION "
                              "SELECT DISTINCT(ScriptName) FROM item_template WHERE ScriptName <> '' "
                              "UNION "
                              "SELECT DISTINCT(ScriptName) FROM scripted_areatrigger WHERE ScriptName <> '' "
                              "UNION "
                              "SELECT DISTINCT(ScriptName) FROM scripted_event_id WHERE ScriptName <> '' "
                              "UNION "
                              "SELECT DISTINCT(ScriptName) FROM instance_template WHERE ScriptName <> '' "
                              "UNION "
                              "SELECT DISTINCT(ScriptName) FROM world_template WHERE ScriptName <> ''");

    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded empty set of Script Names!");
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());
    uint32 count = 0;

    do
    {
        bar.step();
        m_scriptNames.push_back((*queryResult)[0].GetString());
        ++count;
    }
    while (queryResult->NextRow());

    std::sort(m_scriptNames.begin(), m_scriptNames.end());   // 升序排序以支持后续二分查找

    sLog.outString(">> Loaded %d Script Names", count);
    sLog.outString();
}

// 检查所有已分配的脚本名称是否都已实现对应的 C++ 脚本
void ScriptDevAIMgr::CheckScriptNames()
{
    // Check existence scripts for all registered by core script names (检查核心注册的所有脚本名称是否均已实现)
    for (uint32 i = 1; i < GetScriptIdsCount(); ++i)
    {
        if (!m_scripts[i])
            script_error_log("No script found for ScriptName '%s'.", GetScriptName(i));
    }
    outstring_log(">> Loaded %i C++ Scripts.", m_scriptCount);
}

// 二分查找脚本名称对应的数字 ID
uint32 ScriptDevAIMgr::GetScriptId(const char* name) const
{
    // use binary search to find the script name in the sorted vector
    // assume "" is the first element (在已排序的向量中使用二分查找，首项为空字符串)
    if (!name)
        return 0;

    ScriptNameMap::const_iterator itr = std::lower_bound(m_scriptNames.begin(), m_scriptNames.end(), name);

    if (itr == m_scriptNames.end() || *itr != name)
        return 0;

    return uint32(itr - m_scriptNames.begin());
}

// 从 scripted_areatrigger 表加载区域触发器 ID 到脚本 ID 的映射
void ScriptDevAIMgr::LoadAreaTriggerScripts()
{
    m_AreaTriggerScripts.clear();                           // need for reload case (重新加载时清空映射)
    auto queryResult = WorldDatabase.Query("SELECT entry, ScriptName FROM scripted_areatrigger");

    uint32 count = 0;

    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u scripted areatrigger", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = queryResult->Fetch();

        uint32 triggerId = fields[0].GetUInt32();
        const char* scriptName = fields[1].GetString();

        if (!sAreaTriggerStore.LookupEntry(triggerId))
        {
            sLog.outErrorDb("Table `scripted_areatrigger` has area trigger (ID: %u) not listed in `AreaTrigger.dbc`.", triggerId);
            continue;
        }

        m_AreaTriggerScripts[triggerId] = GetScriptId(scriptName);
    }
    while (queryResult->NextRow());

    sLog.outString(">> Loaded %u areatrigger scripts", count);
    sLog.outString();

}

// 从 scripted_event_id 表加载事件 ID 到脚本 ID 的映射
void ScriptDevAIMgr::LoadEventIdScripts()
{
    m_EventIdScripts.clear();                           // need for reload case (重新加载时清空映射)
    auto queryResult = WorldDatabase.Query("SELECT id, ScriptName FROM scripted_event_id");

    uint32 count = 0;

    if (!queryResult)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u scripted event id", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(queryResult->GetRowCount());

    std::set<uint32> eventIds;                              // Store possible event ids (存储所有可能的有效事件 ID 集合)
    ScriptMgr::CollectPossibleEventIds(eventIds);

    do
    {
        ++count;
        bar.step();

        Field* fields = queryResult->Fetch();

        uint32 eventId = fields[0].GetUInt32();
        const char* scriptName = fields[1].GetString();

        std::set<uint32>::const_iterator itr = eventIds.find(eventId);
        if (itr == eventIds.end())
            sLog.outErrorDb("Table `scripted_event_id` has id %u not referring to any gameobject_template type 10 data2 field, type 3 data6 field, type 13 data 2 field, type 29 or any spell effect %u or path taxi node data",
                            eventId, SPELL_EFFECT_SEND_EVENT);

        m_EventIdScripts[eventId] = GetScriptId(scriptName);
    }
    while (queryResult->NextRow());

    sLog.outString(">> Loaded %u scripted event id", count);
    sLog.outString();
}

// 根据区域触发器 ID 获取脚本 ID
uint32 ScriptDevAIMgr::GetAreaTriggerScriptId(uint32 triggerId) const
{
    AreaTriggerScriptMap::const_iterator itr = m_AreaTriggerScripts.find(triggerId);
    if (itr != m_AreaTriggerScripts.end())
        return itr->second;

    return 0;
}

// 根据事件 ID 获取脚本 ID
uint32 ScriptDevAIMgr::GetEventIdScriptId(uint32 eventId) const
{
    EventIdScriptMap::const_iterator itr = m_EventIdScripts.find(eventId);
    if (itr != m_EventIdScripts.end())
        return itr->second;

    return 0;
}
