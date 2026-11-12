/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_ESCORTAI_H
#define SC_ESCORTAI_H

// Remove this include, when EscortAI stores uint32 quest-id instead of Quest*
#include "Globals/ObjectMgr.h"

// 护送任务状态枚举
enum EscortState
{
    STATE_ESCORT_NONE       = 0x000,                        // nothing in progress (未进行任何护送)
    STATE_ESCORT_ESCORTING  = 0x001,                        // escort are in progress (护送正在进行中)
    STATE_ESCORT_RETURNING  = 0x002,                        // escort is returning after being in combat (脱战后正在返回路径点)
    STATE_ESCORT_PAUSED     = 0x004                         // will not proceed with waypoints before state is removed (护送暂停状态，暂停期间停止移动)
};

// 护送任务 NPC AI 基类：封装路径点巡航、玩家脱节检测、战斗援助及任务成败处理
struct npc_escortAI : public ScriptedAI
{
    public:
        explicit npc_escortAI(Creature* creature);
        explicit npc_escortAI(Creature* creature, uint32 combatActions);
        ~npc_escortAI() {}

        void GetAIInformation(ChatHandler& reader) override; // 调试命令：输出护送 AI 状态信息

        virtual void Aggro(Unit* who) override;             // 进入战斗处理

        // UnitAI functions
        void JustDied(Unit* killer) override;               // 护送目标死亡回调：使玩家及队伍任务失败

        void CorpseRemoved(uint32& /*respawnDelay*/) override; // 尸体清除后重置护送状态

        void UpdateAI(const uint32 diff) override;               // the "internal" update, calls UpdateEscortAI() (内部主更新：检查玩家距离，调用 UpdateEscortAI)
        virtual void UpdateEscortAI(const uint32 diff);          // used when it's needed to add code in update (abilities, scripted events, etc) (子类扩展更新：施法或脚本事件)

        void MovementInform(uint32 moveType, uint32 pointId) override; // 移动状态通知：触发路径点到达/开始

        virtual void WaypointReached(uint32 pointId) = 0;   // 到达指定路径点回调（纯虚函数，子类必须实现）
        virtual void WaypointStart(uint32 /*pointId*/) {}   // 出发前往指定路径点回调

        // 启动护送：配置跑步模式、玩家、任务、即时刷新及循环路径
        void Start(bool run = false, const Player* player = nullptr, const Quest* quest = nullptr, bool instantRespawn = false, bool canLoopPath = false, uint32 waypointPath = 0);
        void End(); // resets data on successful end for special mobs (特殊生物护送成功结束后清理数据)
        void QuestEnd();                                    // 护送任务结束（断开任务指针关联）

        void SetRun(bool run = true);                       // 切换跑步/行走状态
        void SetEscortPaused(bool paused);                  // 暂停或恢复护送移动

        bool HasEscortState(uint32 escortState) const { return (m_escortState & escortState) != 0; } // 检查是否具有指定护送状态标志

        // update current point (更新当前目标路径点)
        void SetCurrentWaypoint(uint32 pointId);

        void FailQuestForPlayerAndGroup();                  // 使受护送玩家及其小队任务判定失败

        bool AssistPlayerInCombat(Unit* who) override;      // 协助玩家战斗：在视野内攻击正在攻击玩家的敌人

        uint32 GetCurrentWaypointPath() const override { return m_currentEscortWaypointPath; } // 获取当前护送路径点路径 ID

    protected:
        Player* GetPlayerForEscort() const { return m_creature->GetMap()->GetPlayer(m_playerGuid); } // 获取当前被护送玩家指针
        bool IsSD2EscortMovement(uint32 moveType) const;     // 判定移动类型是否为 SD2 护送路径点移动
        virtual void JustStartedEscort() {}                 // 护送刚启动时的事件回调

        void SetEscortWaypoints(uint32 pathId);             // 设置护送路径点路径 ID

        const Quest* m_questForEscort;                     // generally passed in Start() when regular escort script. (关联的护送任务指针)

    private:
        bool IsPlayerOrGroupInRange();                      // 检查护送玩家或其小队成员是否在有效距离内

        void AddEscortState(uint32 escortState) { m_escortState |= escortState; }
        void RemoveEscortState(uint32 escortState) { m_escortState &= ~escortState; }

        ObjectGuid m_playerGuid;                            // 受护送玩家 GUID
        uint32 m_playerCheckTimer;                          // 玩家距离检测计时器
        uint32 m_escortState;                               // 护送状态掩码标志


        bool m_isRunning;                                  // all creatures are walking by default (has flag SPLINEFLAG_WALKMODE) (是否处于跑步模式，默认行走)
        bool m_canInstantRespawn;                          // if creature should respawn instantly after escort over (if not, database respawntime are used) (护送结束后是否立即复活)
        bool m_canReturnToStart;                           // if creature can walk same path (loop) without despawn. Not for regular escort quests. (是否循环路径点而不消失)

        uint32 m_waypointPathID;                            // 默认路径点路径 ID
        uint32 m_currentEscortWaypointPath;                 // 当前实际执行的路径点路径 ID
};

#endif
