/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
* This program is free software licensed under GPL version 2
* Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_INSTANCE_H
#define SC_INSTANCE_H

#include "Maps/InstanceData.h"
#include "Maps/Map.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"

// 副本首领遭遇战状态枚举
enum EncounterState
{
    NOT_STARTED   = 0,                                      // 未开始
    IN_PROGRESS   = 1,                                      // 进行中
    FAIL          = 2,                                      // 遭遇失败（灭团）
    DONE          = 3,                                      // 遭遇完成（首领击杀）
    SPECIAL       = 4                                       // 特殊状态/阶段
};

// 副本数据存盘与加载调试日志宏
#define OUT_SAVE_INST_DATA             debug_log("SD2: Saving Instance Data for Instance %s (Map %d, Instance Id %d)", instance->GetMapName(), instance->GetId(), instance->GetInstanceId())
#define OUT_SAVE_INST_DATA_COMPLETE    debug_log("SD2: Saving Instance Data for Instance %s (Map %d, Instance Id %d) completed.", instance->GetMapName(), instance->GetId(), instance->GetInstanceId())
#define OUT_LOAD_INST_DATA(a)          debug_log("SD2: Loading Instance Data for Instance %s (Map %d, Instance Id %d). Input is '%s'", instance->GetMapName(), instance->GetId(), instance->GetInstanceId(), a)
#define OUT_LOAD_INST_DATA_COMPLETE    debug_log("SD2: Instance Data Load for Instance %s (Map %d, Instance Id: %d) is complete.", instance->GetMapName(), instance->GetId(), instance->GetInstanceId())
#define OUT_LOAD_INST_DATA_FAIL        script_error_log("Unable to load Instance Data for Instance %s (Map %d, Instance Id: %d).", instance->GetMapName(), instance->GetId(), instance->GetInstanceId())

// 副本数据管理脚本基类：提供首领状态同步、门禁交互控制、世界状态分发与关键实体缓存
class ScriptedInstance : public InstanceData
{
    public:
        typedef std::map<uint32, ObjectGuid> EntryGuidMap;
        typedef std::unordered_map<uint32, GuidVector> EntryGuidCollection;

        ScriptedInstance(Map* map) : InstanceData(map) {}
        ~ScriptedInstance() {}

        // Default accessor functions (根据 Entry 从缓存中获取单一或批量对象)
        GameObject* GetSingleGameObjectFromStorage(uint32 entry, bool skipDebugLog = false) const;
        Creature* GetSingleCreatureFromStorage(uint32 entry, bool skipDebugLog = false) const;
        void GetCreatureGuidVectorFromStorage(uint32 entry, GuidVector& entryGuidVector, bool skipDebugLog = false) const;
        void GetGameObjectGuidVectorFromStorage(uint32 entry, GuidVector& entryGuidVector, bool skipDebugLog = false) const;

        // Change active state of doors or buttons (操作门或机关按钮状态)
        void DoUseDoorOrButton(ObjectGuid guid, uint32 withRestoreTime = 0, bool useAlternativeState = false);
        void DoUseDoorOrButton(uint32 entry, uint32 withRestoreTime = 0, bool useAlternativeState = false);
        void DoUseOpenableObject(uint32 entry, bool open, uint32 withRestoreTime = 0, bool useAlternativeState = false);

        // Respawns a GO having negative spawntimesecs in gameobject-table (重新刷新指定游戏对象)
        void DoRespawnGameObject(ObjectGuid guid, uint32 timeToDespawn = MINUTE);
        void DoRespawnGameObject(uint32 entry, uint32 timeToDespawn = MINUTE);

        // Toggle the flags of a GO (切换游戏对象标志)
        void DoToggleGameObjectFlags(ObjectGuid guid, uint32 GOflags, bool apply);
        void DoToggleGameObjectFlags(uint32 entry, uint32 GOflags, bool apply);

        // Sends world state update to all players in instance (向副本内所有玩家同步世界状态)
        virtual void DoUpdateWorldState(uint32 stateId, uint32 stateData);

        // Get a Player from map (获取副本地图内的玩家指针)
        Player* GetPlayerInMap(bool bOnlyAlive = false, bool bCanBeGamemaster = true) const;

        // Wrapper for simulating map-wide text in this instance. It is expected that the Creature is stored in m_npcEntryGuidStore if loaded.
        // 在副本全图广播生物台词（若生物已加载直接使用，否则进行模拟播放）
        void DoOrSimulateScriptTextForThisInstance(int32 textEntry, uint32 creatureEntry)
        {
            // Prevent debug output in GetSingleCreatureFromStorage
            DoOrSimulateScriptTextForMap(textEntry, creatureEntry, instance, GetSingleCreatureFromStorage(creatureEntry, true));
        }

        void BanPlayersIfNoGm(const std::string& reason);   // 安全检查：无 GM 在场时封禁异常玩家

    protected:
        void DespawnGuids(GuidVector& spawns); // despawns all creature guids and clears contents (批量消失指定 GUID 列表实体)
        void RespawnDbGuids(std::vector<uint32>& spawns, uint32 respawnDelay); // respawns all dbguid creatures (按数据库 GUID 重新刷新实体)

        // Storage for GO-Guids and NPC-Guids
        EntryGuidMap m_goEntryGuidStore;                   // Store unique GO-Guids by entry (按 Entry 缓存唯一的游戏对象 GUID)
        EntryGuidMap m_npcEntryGuidStore;                  // Store unique NPC-Guids by entry (按 Entry 缓存唯一的生物 GUID)
        EntryGuidCollection m_npcEntryGuidCollection;      // Store all Guids by entry (按 Entry 聚合存储生物 GUID 向量)
        EntryGuidCollection m_goEntryGuidCollection;       // Store all Guids by entry (按 Entry 聚合存储游戏对象 GUID 向量)
};

// Class for world maps (May need additional zone-wide functions later on)
// 大世界地图专用脚本基类
class ScriptedMap : public ScriptedInstance
{
    public:
        ScriptedMap(Map* map) : ScriptedInstance(map) {}
};

// 剧情对话步骤类型
enum DialogueStepType
{
    DIALOGUE_STEP_ACTION,                                   // 动作步骤
    DIALOGUE_STEP_TEXT,                                     // 纯文本步骤
    DIALOGUE_STEP_MAX
};

/// A static const array of this structure must be handled to DialogueHelper
// 单边剧情对话步骤条目结构体
struct DialogueEntry
{
    int32 textEntry;                                       ///< To be said text entry (脚本文本条目 ID)
    uint32 sayerEntry;                                    ///< Entry of the mob who should say (说话生物 Entry)
    uint32 timer;                                         ///< Time delay until next text of array is said (0 stops) (距下一步骤的延迟毫秒，0 表示终止)
    DialogueStepType type {DIALOGUE_STEP_ACTION};
};

/// A static const array of this structure must be handled to DialogueHelper
// 双边（联盟/部落分流）剧情对话步骤条目结构体
struct DialogueEntryTwoSide
{
    int32 textEntry;                                       ///< To be said text entry (first side) (第一侧台词条目 ID)
    uint32 sayerEntry;                                    ///< Entry of the mob who should say (first side) (第一侧说话者 Entry)
    int32 textEntryAlt;                                    ///< To be said text entry (second side) (第二侧台词条目 ID)
    uint32 sayerEntryAlt;                                 ///< Entry of the mob who should say (second side) (第二侧说话者 Entry)
    uint32 timer;                                         ///< Time delay until next text of array is said (0 stops) (延时毫秒)
    DialogueStepType type {DIALOGUE_STEP_ACTION};
};

/// Helper class handling a dialogue given as static const array of DialogueEntry or DialogueEntryTwoSide
// 剧情对话推进辅助管理器：根据配置表依次播放剧情对话并调度动作
class DialogueHelper
{
    public:
        // The array MUST be terminated by {0,0,0}
        DialogueHelper(DialogueEntry const* dialogueArray);
        // The array MUST be terminated by {0,0,0,0,0}
        DialogueHelper(DialogueEntryTwoSide const* dialogueTwoSideArray);
        virtual ~DialogueHelper() = default;

        /// Function to initialize the dialogue helper for instances. If not used with instances, GetSpeakerByEntry MUST be overwritten to obtain the speakers
        void InitializeDialogueHelper(ScriptedInstance* instance, bool canSimulateText = false) { m_instance = instance; m_canSimulate = canSimulateText; }
        /// Set if take first entries or second entries
        void SetDialogueSide(bool isFirstSide) { m_isFirstSide = isFirstSide; } // 设置使用第一侧还是第二侧阵营对话

        void StartNextDialogueText(int32 textEntry);         // 启动从指定台词 Entry 开始的对话流程

        void DialogueUpdate(uint32 diff);                   // 周期更新推进对话时间轴

        // for use on death or respawn applicably
        void DisableDialogue() { m_timer = 0; }             // 禁用并中断对话

    protected:
        /// Will be called when a dialogue step was done
        virtual void JustDidDialogueStep(int32 /*entry*/) {} // 完成某步对话后的虚函数回调
        /// Will be called to get a speaker, MUST be implemented if not used in instances
        virtual Creature* GetSpeakerByEntry(uint32 /*entry*/) { return nullptr; } // 获取说话生物实例

    private:
        void DoNextDialogueStep();

        ScriptedInstance* m_instance;

        DialogueEntry const* m_dialogueArray;
        DialogueEntry const* m_currentEntry;
        DialogueEntryTwoSide const* m_dialogueTwoSideArray;
        DialogueEntryTwoSide const* m_currentEntryTwoSide;

        uint32 m_timer;
        bool m_isFirstSide;
        bool m_canSimulate;
};

#endif
