/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_SCRIPTMGR_H
#define SC_SCRIPTMGR_H

#include "Common.h"
#include "Server/DBCStructure.h"
#include "Server/SQLStorages.h"
#include "Spells/SpellMgr.h"

#include <functional>

class Player;
class Creature;
class UnitAI;
class InstanceData;
class Quest;
class Item;
class GameObject;
class SpellCastTargets;
class Map;
class Unit;
class WorldObject;
class Aura;
class Object;
class ObjectGuid;
class GameObjectAI;

// *********************************************************
// ************** Some defines used globally ***************
// 全局通用基础宏定义

// Basic defines (基础常量定义)
#define VISIBLE_RANGE       (166.0f)                        // MAX visible range (size of grid) (最大可视范围 - 地图网格尺寸)
#define DEFAULT_TEXT        "<ScriptDev2 Text Entry Missing!>" // 缺失脚本文本默认占位符

/* Escort Factions (护送阵营定义)
 * TODO: find better naming and definitions.
 * N=Neutral, A=Alliance, H=Horde.
 * NEUTRAL or FRIEND = Hostility to player surroundings (not a good definition)
 * ACTIVE or PASSIVE = Hostility to environment surroundings.
 * (N=中立, A=联盟, H=部落; NEUTRAL/FRIEND=对周围玩家的态度; ACTIVE/PASSIVE=对周围环境的主被动态度)
 */
enum EscortFaction
{
    FACTION_ESCORT_A_NEUTRAL_PASSIVE    = 10,               // 联盟中立被动
    FACTION_ESCORT_H_NEUTRAL_PASSIVE    = 33,               // 部落中立被动
    FACTION_ESCORT_N_NEUTRAL_PASSIVE    = 113,              // 完全中立被动

    FACTION_ESCORT_A_NEUTRAL_ACTIVE     = 231,              // 联盟中立主动
    FACTION_ESCORT_H_NEUTRAL_ACTIVE     = 232,              // 部落中立主动
    FACTION_ESCORT_N_NEUTRAL_ACTIVE     = 250,              // 完全中立主动

    FACTION_ESCORT_N_FRIEND_PASSIVE     = 290,              // 中立友善被动
    FACTION_ESCORT_N_FRIEND_ACTIVE      = 495,              // 中立友善主动

    FACTION_ESCORT_A_PASSIVE            = 774,              // 联盟被动
    FACTION_ESCORT_H_PASSIVE            = 775,              // 部落被动

    FACTION_ESCORT_N_ACTIVE             = 1986,             // 中立主动
    FACTION_ESCORT_H_ACTIVE             = 2046              // 部落主动
};

// *********************************************************
// ************* Some structures used globally *************
// 全局通用结构体与工厂函数

// 创建生物 UnitAI 实例模板工厂函数
template <typename T>
UnitAI* GetNewAIInstance(Creature* creature)
{
    return new T(creature);
}

// 创建游戏对象 GameObjectAI 实例模板工厂函数
template <typename T>
GameObjectAI* GetNewAIInstance(GameObject* gameobject)
{
    return new T(gameobject);
}

// 创建副本脚本 InstanceData 实例模板工厂函数
template <typename T>
InstanceData* GetNewInstanceScript(Map* map)
{
    return new T(map);
}

// C++ 脚本描述符结构体：存储脚本名称以及挂接的各项回调函数指针
struct Script
{
    Script() :
        pGossipHello(nullptr), pGossipHelloGO(nullptr), pGossipSelect(nullptr), pGossipSelectGO(nullptr),
        pGossipSelectWithCode(nullptr), pGossipSelectGOWithCode(nullptr),
        pDialogStatusNPC(nullptr), pDialogStatusGO(nullptr),
        pQuestAcceptNPC(nullptr), pQuestAcceptGO(nullptr), pQuestAcceptItem(nullptr),
        pQuestRewardedNPC(nullptr), pQuestRewardedGO(nullptr),
        pGOUse(nullptr), pItemUse(nullptr), pItemLoot(nullptr), pAreaTrigger(nullptr), pProcessEventId(nullptr),
        pTrapSearching(nullptr), GetGameObjectAI(nullptr), GetAI(nullptr), GetInstanceData(nullptr)
    {}

    std::string Name;                                       // 脚本唯一标识名称（对应数据库中的 ScriptName）

    bool (*pGossipHello)(Player*, Creature*);               // NPC 对话窗口打开问候回调
    bool (*pGossipHelloGO)(Player*, GameObject*);           // 游戏对象对话窗口打开问候回调
    bool (*pGossipSelect)(Player*, Creature*, uint32, uint32); // NPC 对话菜单选项点击回调
    bool (*pGossipSelectGO)(Player*, GameObject*, uint32, uint32); // 游戏对象对话菜单选项点击回调
    bool (*pGossipSelectWithCode)(Player*, Creature*, uint32, uint32, const char*); // 带输入代码的 NPC 对话菜单选项回调
    bool (*pGossipSelectGOWithCode)(Player*, GameObject*, uint32, uint32, const char*); // 带输入代码的游戏对象对话菜单选项回调
    uint32(*pDialogStatusNPC)(const Player*, const Creature*); // 获取 NPC 对话/任务状态标志
    uint32(*pDialogStatusGO)(const Player*, const GameObject*); // 获取游戏对象对话/任务状态标志
    bool (*pQuestAcceptNPC)(Player*, Creature*, Quest const*); // 玩家从 NPC 接受任务回调
    bool (*pQuestAcceptGO)(Player*, GameObject*, Quest const*); // 玩家从游戏对象接受任务回调
    bool (*pQuestAcceptItem)(Player*, Item*, Quest const*); // 玩家从物品接受任务回调
    bool (*pQuestRewardedNPC)(Player*, Creature*, Quest const*); // 玩家在 NPC 交付任务并领取奖励回调
    bool (*pQuestRewardedGO)(Player*, GameObject*, Quest const*); // 玩家在游戏对象交付任务并领取奖励回调
    bool (*pGOUse)(Player*, GameObject*);                   // 玩家交互使用游戏对象回调
    bool (*pItemUse)(Player*, Item*, SpellCastTargets const&); // 玩家使用物品回调
    bool (*pItemLoot)(Player*, Item*, bool);                // 玩家拾取物品回调
    bool (*pAreaTrigger)(Player*, AreaTriggerEntry const*); // 玩家进入区域触发器回调
    bool (*pProcessEventId)(uint32, Object*, Object*, bool); // 处理自定义事件 ID 回调
    std::function<bool(Unit*)>* pTrapSearching;             // 陷阱搜索有效单位谓词函数

    GameObjectAI* (*GetGameObjectAI)(GameObject*);          // 游戏对象 AI 实例工厂函数指针
    UnitAI* (*GetAI)(Creature*);                            // 生物 AI 实例工厂函数指针
    InstanceData* (*GetInstanceData)(Map*);                 // 副本数据管理脚本工厂函数指针

    void RegisterSelf(bool bReportError = true);            // 将当前脚本自注册到 ScriptDevAIMgr
};

// ScriptDevAI 脚本管理器：负责所有 C++ 脚本的生命周期管理、名称索引映射以及各类事件的转发调度
class ScriptDevAIMgr
{
    public:
        ScriptDevAIMgr() : m_scriptCount(0) {}
        ~ScriptDevAIMgr();

        void Initialize();                                  // 初始化并加载所有脚本与文本
        void LoadScriptNames();                             // 从数据库各表中加载唯一的 ScriptName 列表
        void CheckScriptNames();                            // 校验脚本名称，警告缺失脚本并统计总数
        void LoadAreaTriggerScripts();                      // 加载区域触发器与脚本 ID 映射表
        void LoadEventIdScripts();                          // 加载事件 ID 与脚本 ID 映射表

        // 核心事件回调处理接口
        bool OnGossipHello(Player* pPlayer, Creature* pCreature);
        bool OnGossipHello(Player* pPlayer, GameObject* pGo);
        bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction, const char* code);
        bool OnGossipSelect(Player* pPlayer, GameObject* pGo, uint32 uiSender, uint32 uiAction, const char* code);
        bool OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest);
        bool OnQuestAccept(Player* pPlayer, GameObject* pGo, Quest const* pQuest);
        bool OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest);
        bool OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest);
        bool OnQuestRewarded(Player* pPlayer, GameObject* pGo, Quest const* pQuest);
        uint32 GetDialogStatus(const Player* pPlayer, const Creature* pCreature) const;
        uint32 GetDialogStatus(const Player* pPlayer, const GameObject* pGo) const;
        bool OnGameObjectUse(Player* pPlayer, GameObject* pGo);
        std::function<bool(Unit*)>* OnTrapSearch(GameObject* go);
        bool OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets);
        bool OnItemLoot(Player* pPlayer, Item* pItem, bool apply);
        bool OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry);
        bool OnProcessEvent(uint32 uiEventId, Object* pSource, Object* pTarget, bool bIsStart);

        // 脚本注册与检索
        void AddScript(uint32 id, Script* script);
        Script* GetScript(uint32 id) const;
        const char* GetScriptName(uint32 id) const { return id < m_scriptNames.size() ? m_scriptNames[id].c_str() : ""; }
        uint32 GetScriptId(const char* name) const;
        uint32 GetScriptIdsCount() const { return m_scriptNames.size(); }

        // AI 与副本数据工厂调用接口
        UnitAI* GetCreatureAI(Creature* pCreature) const;
        GameObjectAI* GetGameObjectAI(GameObject* gameobject) const;

        InstanceData* CreateInstanceData(Map* pMap);
        uint32 GetAreaTriggerScriptId(uint32 triggerId) const;
        uint32 GetEventIdScriptId(uint32 eventId) const;

    private:
        typedef std::vector<Script*> SDScriptVec;
        typedef std::vector<std::string> ScriptNameMap;
        typedef std::unordered_map<uint32, uint32> AreaTriggerScriptMap;
        typedef std::unordered_map<uint32, uint32> EventIdScriptMap;

        int m_scriptCount;                                  // 已注册脚本总计数
        SDScriptVec m_scripts;                              // 脚本指针数组（以 scriptId 为索引）

        AreaTriggerScriptMap    m_AreaTriggerScripts;       // 区域触发器 ID -> 脚本 ID 映射表
        EventIdScriptMap        m_EventIdScripts;           // 事件 ID -> 脚本 ID 映射表

        ScriptNameMap           m_scriptNames;              // 有序的脚本名称列表（用于二分查找快速检索 scriptId）
};

// *********************************************************
// ************* Some functions used globally **************
// 全局辅助函数

// Generic scripting text function (通用脚本台词播放函数)
void DoScriptText(int32 iTextEntry, WorldObject* pSource, Unit* pTarget = nullptr, uint32 chatTypeOverride = 0);
void DoBroadcastText(int32 iTextEntry, WorldObject* pSource, Unit* pTarget = nullptr, uint32 chatTypeOverride = 0);
void DoOrSimulateScriptTextForMap(int32 iTextEntry, uint32 uiCreatureEntry, Map* pMap, Creature* pCreatureSource = nullptr, Unit* pTarget = nullptr);

#define sScriptDevAIMgr MaNGOS::Singleton<ScriptDevAIMgr>::Instance() // ScriptDevAIMgr 单例访问宏

#endif
