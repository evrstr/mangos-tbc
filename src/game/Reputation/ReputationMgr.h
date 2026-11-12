/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
 *
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

#ifndef __MANGOS_REPUTATION_MGR_H
#define __MANGOS_REPUTATION_MGR_H

#include "Common.h"
#include "Globals/SharedDefines.h"
#include "Server/DBCStructure.h"
#include <map>

// 声望阵营状态标志位
enum FactionFlags
{
    FACTION_FLAG_VISIBLE            = 0x01,                 // makes visible in client (set or can be set at interaction with target of this faction) (在客户端声望面板中可见，交互该阵营目标时自动激活)
    FACTION_FLAG_AT_WAR             = 0x02,                 // enable AtWar-button in client. player controlled (except opposition team always war state), Flag only set on initial creation (开启“交战状态”标志，除对立阵营外可由玩家手动切换)
    FACTION_FLAG_HIDDEN             = 0x04,                 // hidden faction from reputation pane in client (player can gain reputation, but this update not sent to client) (隐藏阵营：玩家可正常增减声望但不在面板显示且不下发更新)
    FACTION_FLAG_INVISIBLE_FORCED   = 0x08,                 // always overwrite FACTION_FLAG_VISIBLE and hide faction in rep.list, used for hide opposite team factions (强制不可见：覆盖 VISIBLE 标志，常用于隐藏敌对阵营声望)
    FACTION_FLAG_PEACE_FORCED       = 0x10,                 // always overwrite FACTION_FLAG_AT_WAR, used for prevent war with own team factions (强制和平：覆盖 AT_WAR 标志，防止与己方阵营开战)
    FACTION_FLAG_INACTIVE           = 0x20,                 // player controlled, state stored in characters.data ( CMSG_SET_FACTION_INACTIVE ) (玩家手动勾选“置于闲置”状态)
    FACTION_FLAG_RIVAL              = 0x40,                 // flag for the two competing outland factions (外域互为敌对竞争关系阵营的标志，如奥尔多与占星者)
    FACTION_FLAG_TEAM_REPUTATION    = 0x80,                 // faction has own reputation standing despite teaming up sub-factions; spillover from subfactions will go this instead of other subfactions (父级阵营自身拥有独立声望条，子阵营声望溢出将汇总至此而非平分给兄弟阵营)
};

typedef uint32 RepListID;
// 单个阵营在玩家身上的运行时声望状态
struct FactionState
{
    uint32 ID;                                              // Faction.dbc 主键 ID
    RepListID ReputationListID;                             // 客户端声望列表索引 ID
    uint32 Flags;                                           // 当前阵营状态标志（FactionFlags 组合）
    int32  Standing;                                        // 相对基础声望的增量值
    bool needSend;                                          // 是否需要向客户端下发同步更新
    bool needSave;                                          // 是否存在未落盘的数据库变更
};

typedef std::map<RepListID, FactionState> FactionStateList;
typedef std::pair<FactionStateList::const_iterator, FactionStateList::const_iterator> FactionStateListPair;

typedef std::map<uint32, ReputationRank> ForcedReactions;

class Player;
class QueryResult;

// 玩家声望管理器：负责加载/保存声望数据、计算声望等级与阵营溢出、同步状态到客户端
class ReputationMgr
{
    public:                                                 // constructors and global modifiers (构造与持久化接口)
        explicit ReputationMgr(Player* owner) : m_player(owner) {}
        ~ReputationMgr() {}

        void SaveToDB();                                    // 将有变动的阵营声望保存到 character_reputation 表
        void LoadFromDB(std::unique_ptr<QueryResult> queryResult); // 从数据库查询结果恢复玩家声望状态
    public:                                                 // statics (静态常量与换算函数)
        static const int32 PointsInRank[MAX_REPUTATION_RANK]; // 各声望等级（仇恨到崇拜）对应的声望区间跨度
        static const int32 Reputation_Cap    =  42999;      // 声望最大值上限（崇拜满值）
        static const int32 Reputation_Bottom = -42000;      // 声望最小值下限（仇恨底限）

        static ReputationRank ReputationToRank(int32 standing); // 将总声望数值换算为 ReputationRank 等级
    public:                                                 // accessors (只读查询接口)
        FactionStateList const& GetStateList() const { return m_factions; }

        FactionState const* GetState(FactionEntry const* factionEntry) const;
        FactionState const* GetState(RepListID id) const;

        int32 GetReputation(uint32 faction_id) const;
        int32 GetReputation(FactionEntry const* factionEntry) const;
        int32 GetBaseReputation(FactionEntry const* factionEntry) const;

        bool IsAtWar(uint32 faction_id) const;
        bool IsAtWar(FactionEntry const* factionEntry) const;

        ReputationRank GetRank(FactionEntry const* factionEntry) const;
        ReputationRank GetBaseRank(FactionEntry const* factionEntry) const;

        ReputationRank const* GetForcedRankIfAny(FactionTemplateEntry const* factionTemplateEntry) const;

    public:                                                 // modifiers (声望与标志修改接口)
        // 直接设置指定阵营声望值
        bool SetReputation(FactionEntry const* factionEntry, int32 standing, bool noSpillover = false)
        {
            return SetReputation(factionEntry, standing, false, noSpillover);
        }
        // 按增量修改指定阵营声望值
        bool ModifyReputation(FactionEntry const* factionEntry, int32 standing, bool noSpillover = false)
        {
            return SetReputation(factionEntry, standing, true, noSpillover);
        }

        void SetVisible(FactionTemplateEntry const* factionTemplateEntry);
        void SetVisible(FactionEntry const* factionEntry);
        void SetAtWar(RepListID repListID, bool on);
        void SetInactive(RepListID repListID, bool on);

        void ApplyForceReaction(uint32 faction_id, ReputationRank rank, bool apply);

    public:                                                 // senders (客户端网络包发送接口)
        void SendInitialReputations();
        void SendForceReactions();
        void SendState(FactionState const* faction);

    private:                                                // internal helper functions (内部辅助方法)
        void Initialize();
        uint32 GetDefaultStateFlags(const FactionEntry* factionEntry) const;
        bool SetReputation(FactionEntry const* factionEntry, int32 standing, bool incremental, bool reputationSpillover);
        bool SetOneFactionReputation(FactionEntry const* factionEntry, int32 standing, bool incremental);
        void SetVisible(FactionState* faction);
        void SetAtWar(FactionState* faction, bool atWar);
        void SetInactive(FactionState* faction, bool inactive);
        void SendVisible(FactionState const* faction) const;
    private:
        Player* m_player;
        FactionStateList m_factions;
        ForcedReactions m_forcedReactions;
};

#endif
