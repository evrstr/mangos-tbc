/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_FOLLOWERAI_H
#define SC_FOLLOWERAI_H

// 跟随状态枚举
enum FollowState
{
    STATE_FOLLOW_NONE       = 0x000,                        // 未跟随状态
    STATE_FOLLOW_INPROGRESS = 0x001,                        // must always have this state for any follow (跟随进行中状态，任何跟随行为必须置位)
    STATE_FOLLOW_RETURNING  = 0x002,                        // when returning to combat start after being in combat (脱离战斗后返回开战位置)
    STATE_FOLLOW_PAUSED     = 0x004,                        // disables following (暂停跟随)
    STATE_FOLLOW_COMPLETE   = 0x008,                        // follow is completed and may end (跟随已完成)
    STATE_FOLLOW_PREEVENT   = 0x010,                        // not implemented (allow pre event to run, before follow is initiated) (跟随前置剧情事件，暂未实现)
    STATE_FOLLOW_POSTEVENT  = 0x020                         // can be set at complete and allow post event to run (跟随后置剧情事件)
};

// 跟随型 NPC AI 基类：动态跟随玩家移动、处理队伍领袖死亡移交、脱战回位及战斗协助
class FollowerAI : public ScriptedAI
{
    public:
        explicit FollowerAI(Creature* creature);
        ~FollowerAI() {}

        // virtual void WaypointReached(uint32 pointId) = 0;

        void MovementInform(uint32 motionType, uint32 pointId) override; // 移动状态通知：回位到达开战点后恢复跟随

        void EnterEvadeMode() override;                     // 闪避脱战并跑回战斗发生点

        void JustDied(Unit* killer) override;               // 死亡处理：若有护送跟随任务则判定失败

        void CorpseRemoved(uint32& /*respawnDelay*/) override; // 尸体清除后重置状态

        void UpdateAI(const uint32 diff) override;               // the "internal" update, calls UpdateFollowerAI() (内部主更新：检测与领袖距离与脱节处理)
        virtual void UpdateFollowerAI(const uint32 diff);        // used when it's needed to add code in update (abilities, scripted events, etc) (子类更新虚函数)

        // 开始跟随目标玩家（可切换临时阵营并绑定任务）
        void StartFollow(Player* leader, uint32 factionForFollower = 0, const Quest* quest = nullptr);

        void SetFollowPaused(bool paused);                 // if special event require follow mode to hold/resume during the follow (暂停或恢复跟随)
        void SetFollowComplete(bool withEndEvent = false);  // 标记跟随完成（可选择是否等待后置剧情事件）

        bool HasFollowState(uint32 followState) const { return (m_followState & followState) != 0; } // 检查指定跟随状态

        bool AssistPlayerInCombat(Unit* who) override;      // 协助处于战斗中的领袖/玩家

    protected:
        Player* GetLeaderForFollower();                     // 获取当前有效领袖指针（支持在小队存活成员中自动切换）

    private:
        void AddFollowState(uint32 uiFollowState) { m_followState |= uiFollowState; }
        void RemoveFollowState(uint32 uiFollowState) { m_followState &= ~uiFollowState; }

        ObjectGuid m_leaderGuid;                            // 当前跟随的领袖玩家 GUID
        uint32 m_updateFollowTimer;                         // 跟随与距离检测更新计时器
        uint32 m_followState;                               // 跟随状态位掩码

        const Quest* m_questForFollow;                     // normally we have a quest (关联的任务指针)
};

#endif
