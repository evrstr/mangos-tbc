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

#include "Reputation/ReputationMgr.h"
#include "Server/DBCStores.h"
#include "Entities/Player.h"
#include "Server/WorldPacket.h"
#include "Globals/ObjectMgr.h"

// 从仇恨(36000)到崇拜(1000)各声望阶梯所需点数
const int32 ReputationMgr::PointsInRank[MAX_REPUTATION_RANK] = {36000, 3000, 3000, 3000, 6000, 12000, 21000, 1000};

// 根据声望总值换算对应的声望等级（ReputationRank）
ReputationRank ReputationMgr::ReputationToRank(int32 standing)
{
    int32 limit = Reputation_Cap + 1;
    for (int i = MAX_REPUTATION_RANK - 1; i >= MIN_REPUTATION_RANK; --i)
    {
        limit -= PointsInRank[i];
        if (standing >= limit)
            return ReputationRank(i);
    }
    return MIN_REPUTATION_RANK;
}

// 获取指定 FactionEntry 对应的玩家声望状态记录
FactionState const* ReputationMgr::GetState(FactionEntry const* factionEntry) const
{
    return (factionEntry && factionEntry->HasReputation()) ? GetState(RepListID(factionEntry->reputationListID)) : nullptr;
}

// 按声望列表索引获取状态记录
FactionState const* ReputationMgr::GetState(RepListID id) const
{
    FactionStateList::const_iterator repItr = m_factions.find(id);
    return repItr != m_factions.end() ? &repItr->second : nullptr;
}

// 获取指定阵营 ID 的当前总声望（基础声望 + 增量）
int32 ReputationMgr::GetReputation(uint32 faction_id) const
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry<FactionEntry>(faction_id);

    if (!factionEntry)
    {
        sLog.outError("ReputationMgr::GetReputation: Can't get reputation of %s for unknown faction (faction id) #%u.", m_player->GetName(), faction_id);
        return 0;
    }

    return GetReputation(factionEntry);
}

// 获取指定阵营条目的当前总声望（基础声望 + 增量）
int32 ReputationMgr::GetReputation(FactionEntry const* factionEntry) const
{
    if (!factionEntry)
        return 0;

    if (FactionState const* state = GetState(factionEntry))
        return GetBaseReputation(factionEntry) + state->Standing;

    return 0;
}

// 根据玩家种族与职业掩码获取该阵营的初始基础声望
int32 ReputationMgr::GetBaseReputation(FactionEntry const* factionEntry) const
{
    if (!factionEntry)
        return 0;

    uint32 raceMask = m_player->getRaceMask();
    uint32 classMask = m_player->getClassMask();

    int idx = factionEntry->GetIndexFitTo(raceMask, classMask);

    return idx >= 0 ? factionEntry->BaseRepValue[idx] : 0;
}

// 检查玩家是否对指定阵营 ID 处于交战状态
bool ReputationMgr::IsAtWar(uint32 faction_id) const
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry<FactionEntry>(faction_id);

    if (!factionEntry)
    {
        sLog.outError("ReputationMgr::IsAtWar: Can't get state of 'At War' flag of %s for unknown faction (faction id) #%u.", m_player->GetName(), faction_id);
        return false;
    }

    return IsAtWar(factionEntry);
}

// 检查玩家是否对指定阵营处于交战状态
bool ReputationMgr::IsAtWar(FactionEntry const* factionEntry) const
{
    if (!factionEntry)
        return false;

    if (FactionState const* state = GetState(factionEntry))
        return (state->Flags & FACTION_FLAG_AT_WAR) != 0;

    return false;
}

// 获取玩家在指定阵营的当前声望等级
ReputationRank ReputationMgr::GetRank(FactionEntry const* factionEntry) const
{
    int32 reputation = GetReputation(factionEntry);
    return ReputationToRank(reputation);
}

// 获取玩家在指定阵营的初始声望等级
ReputationRank ReputationMgr::GetBaseRank(FactionEntry const* factionEntry) const
{
    int32 reputation = GetBaseReputation(factionEntry);
    return ReputationToRank(reputation);
}

// 若该阵营受光环/效果强制覆盖反应态度，返回强制声望等级
ReputationRank const* ReputationMgr::GetForcedRankIfAny(FactionTemplateEntry const* factionTemplateEntry) const
{
    if (!factionTemplateEntry)
        return nullptr;

    ForcedReactions::const_iterator forceItr = m_forcedReactions.find(factionTemplateEntry->faction);
    return forceItr != m_forcedReactions.end() ? &forceItr->second : nullptr;
}

// 应用或移除对某阵营的强制态度覆盖
void ReputationMgr::ApplyForceReaction(uint32 faction_id, ReputationRank rank, bool apply)
{
    if (apply)
        m_forcedReactions[faction_id] = rank;
    else
        m_forcedReactions.erase(faction_id);
}

// 按玩家种族与职业计算阵营默认状态标志位
uint32 ReputationMgr::GetDefaultStateFlags(FactionEntry const* factionEntry) const
{
    if (!factionEntry)
        return 0;

    uint32 raceMask = m_player->getRaceMask();
    uint32 classMask = m_player->getClassMask();

    int idx = factionEntry->GetIndexFitTo(raceMask, classMask);

    return idx >= 0 ? factionEntry->ReputationFlags[idx] : 0;
}

// 向客户端下发所有强制阵营态度表（SMSG_SET_FORCED_REACTIONS）
void ReputationMgr::SendForceReactions()
{
    WorldPacket data;
    data.Initialize(SMSG_SET_FORCED_REACTIONS, 4 + m_forcedReactions.size() * (4 + 4));
    data << uint32(m_forcedReactions.size());
    for (ForcedReactions::const_iterator itr = m_forcedReactions.begin(); itr != m_forcedReactions.end(); ++itr)
    {
        data << uint32(itr->first);                         // faction_id (Faction.dbc) (阵营 ID)
        data << uint32(itr->second);                        // reputation rank (强制声望等级)
    }
    m_player->SendDirectMessage(data);
}

// 发送指定阵营及所有待同步阵营的声望变更包（SMSG_SET_FACTION_STANDING）
void ReputationMgr::SendState(FactionState const* faction)
{
    uint32 count = 1;

    WorldPacket data(SMSG_SET_FACTION_STANDING, (16));      // last check 2.4.0 (基于 2.4.0 协议校验)
    data << (float) 0;                                      // refer-a-friend bonus reputation (战友招募声望加成系数)

    size_t p_count = data.wpos();
    data << (uint32) count;                                 // placeholder (阵营数量占位符)

    data << (uint32) faction->ReputationListID;
    data << (uint32) faction->Standing;

    for (auto& m_faction : m_factions)
    {
        FactionState& subFaction = m_faction.second;
        if (subFaction.needSend)
        {
            subFaction.needSend = false;
            if (subFaction.ReputationListID != faction->ReputationListID)
            {
                data << uint32(subFaction.ReputationListID);
                data << uint32(subFaction.Standing);

                ++count;
            }
        }
    }

    data.put<uint32>(p_count, count);
    m_player->SendDirectMessage(data);
}

// 登录时向客户端下发全部 128 个声望槽位的初始化数据（SMSG_INITIALIZE_FACTIONS）
void ReputationMgr::SendInitialReputations()
{
    WorldPacket data(SMSG_INITIALIZE_FACTIONS, (4 + 128 * 5));
    data << uint32(0x00000080);

    RepListID a = 0;

    for (auto& m_faction : m_factions)
    {
        // fill in absent fields (填充空缺索引槽位)
        for (; a != m_faction.first; ++a)
        {
            data << uint8(0x00);
            data << uint32(0x00000000);
        }

        // fill in encountered data (写入当前阵营标志与声望增量)
        data << uint8(m_faction.second.Flags);
        data << uint32(m_faction.second.Standing);

        m_faction.second.needSend = false;

        ++a;
    }

    // fill in absent fields (补齐剩余至 128 个槽位)
    for (; a != 128; ++a)
    {
        data << uint8(0x00);
        data << uint32(0x00000000);
    }

    m_player->SendDirectMessage(data);
}

// 通知客户端将指定阵营设为在声望面板中可见（SMSG_SET_FACTION_VISIBLE）
void ReputationMgr::SendVisible(FactionState const* faction) const
{
    if (m_player->GetSession()->PlayerLoading())
        return;

    // make faction visible in reputation list at client (在客户端声望列表中显示该阵营)
    WorldPacket data(SMSG_SET_FACTION_VISIBLE, 4);
    data << faction->ReputationListID;
    m_player->SendDirectMessage(data);
}

// 根据 Faction.dbc 初始化玩家的所有可追踪声望条目为默认状态
void ReputationMgr::Initialize()
{
    m_factions.clear();

    for (unsigned int i = 1; i < sFactionStore.GetMaxEntry(); ++i)
    {
        FactionEntry const* factionEntry = sFactionStore.LookupEntry<FactionEntry>(i);

        if (factionEntry && (factionEntry->HasReputation()))
        {
            FactionState newFaction;
            newFaction.ID = factionEntry->ID;
            newFaction.ReputationListID = factionEntry->reputationListID;
            newFaction.Standing = 0;
            newFaction.Flags = GetDefaultStateFlags(factionEntry);
            newFaction.needSend = true;
            newFaction.needSave = true;

            m_factions[newFaction.ReputationListID] = newFaction;
        }
    }
}

// 设置或增量修改阵营声望，并处理数据库/DBC 定义的阵营声望溢出（Spillover）
bool ReputationMgr::SetReputation(FactionEntry const* factionEntry, int32 standing, bool incremental, bool noSpillover)
{
    if (!factionEntry)
        return false;

    bool anyRankIncreased = false;
    if (!noSpillover)
    {
        // if spillover definition exists in DB, override DBC (若数据库存在声望溢出配置则覆盖 DBC 规则)
        if (const RepSpilloverTemplate* repTemplate = sObjectMgr.GetRepSpilloverTemplate(factionEntry->ID))
        {
            for (uint32 i = 0; i < MAX_SPILLOVER_FACTIONS; ++i)
            {
                if (repTemplate->faction[i])
                {
                    if (m_player->GetReputationRank(repTemplate->faction[i]) <= ReputationRank(repTemplate->faction_rank[i]))
                    {
                        // bonuses are already given, so just modify standing by rate (加成已在主声望中应用，此处按溢出比例同步调整关联阵营)
                        int32 spilloverRep = standing * repTemplate->faction_rate[i];
                        if (SetOneFactionReputation(sFactionStore.LookupEntry<FactionEntry>(repTemplate->faction[i]), spilloverRep, incremental))
                            anyRankIncreased = true;
                    }
                }
            }
        }
        else
        {
            float spillOverRepOut = standing;
            // check for sub-factions that receive spillover (查找接收溢出的子阵营列表)
            SimpleFactionsList const* flist = GetFactionTeamList(factionEntry->ID);
            // if has no sub-factions, check for factions with same parent (若无子阵营，则检查同属一个父阵营的姐妹/母阵营)
            if (!flist && factionEntry->team && factionEntry->spilloverRateOut != 0.0f)
            {
                spillOverRepOut *= factionEntry->spilloverRateOut;
                if (FactionEntry const* parent = sFactionStore.LookupEntry<FactionEntry>(factionEntry->team))
                {
                    FactionStateList::iterator parentState = m_factions.find(parent->reputationListID);
                    // some team factions have own reputation standing, in this case do not spill to other sub-factions
                    // 若父阵营自身拥有独立声望条，则仅溢出给父阵营而不平分给其他子阵营
                    if (parentState != m_factions.end() && (parentState->second.Flags & FACTION_FLAG_TEAM_REPUTATION))
                    {
                        if (SetOneFactionReputation(parent, int32(spillOverRepOut), incremental))
                            anyRankIncreased = true;
                    }
                    else    // spill to "sister" factions (溢出至同组姐妹阵营)
                    {
                        flist = GetFactionTeamList(factionEntry->team);
                    }
                }
            }
            if (flist)
            {
                // Spillover to affiliated factions (向关联子阵营逐一应用溢出声望)
                for (uint32 itr : *flist)
                {
                    if (FactionEntry const* factionEntryCalc = sFactionStore.LookupEntry<FactionEntry>(itr))
                    {
                        if (factionEntryCalc == factionEntry || GetRank(factionEntryCalc) > ReputationRank(factionEntryCalc->spilloverMaxRankIn))
                            continue;

                        int32 spilloverRep = int32(spillOverRepOut * factionEntryCalc->spilloverRateIn);
                        if (spilloverRep != 0 || !incremental)
                            if (SetOneFactionReputation(factionEntryCalc, spilloverRep, incremental))
                                anyRankIncreased = true;
                    }
                }
            }
        }
    }

    // spillover done, update faction itself (溢出处理完毕后更新目标阵营自身声望并下发客户端)
    FactionStateList::iterator faction = m_factions.find(RepListID(factionEntry->reputationListID));
    if (faction != m_factions.end())
    {
        if (SetOneFactionReputation(factionEntry, standing, incremental))
            anyRankIncreased = true;

        // only this faction gets reported to client, even if it has no own visible standing
        // 仅将该目标阵营与待发送队列下发给客户端
        SendState(&faction->second);
    }
    return anyRankIncreased;
}

// 更新单个阵营的声望值、截断上下限并自动维护交战标志，返回声望等级是否提升
bool ReputationMgr::SetOneFactionReputation(FactionEntry const* factionEntry, int32 standing, bool incremental)
{
    if (!factionEntry)
        return false;

    FactionStateList::iterator itr = m_factions.find(RepListID(factionEntry->reputationListID));
    if (itr != m_factions.end())
    {
        FactionState& faction = itr->second;
        int32 BaseRep = GetBaseReputation(factionEntry);

        if (incremental)
            standing += faction.Standing + BaseRep;

        if (standing > Reputation_Cap)
            standing = Reputation_Cap;
        else if (standing < Reputation_Bottom)
            standing = Reputation_Bottom;

        ReputationRank rankOld = ReputationToRank(faction.Standing + BaseRep);
        ReputationRank rankNew = ReputationToRank(standing);

        faction.Standing = standing - BaseRep;
        faction.needSend = true;
        faction.needSave = true;

        SetVisible(&faction);

        if (rankNew != rankOld)
        {
            // Server alters "At war" flag on two occasions: (服务端在以下两种等级跨界时自动调整“交战”标志：)
            // * When reputation dips to "Hostile": forced tick and now locked for manual changes (跌入敌对及以下时强制勾选交战)
            if (rankNew < REP_UNFRIENDLY && rankNew < rankOld && rankOld > REP_HOSTILE)
                SetAtWar(&itr->second, true);
            // * When reputation improves to "Neutral": untick by id, can be manually overriden for eligible factions (提升至中立及以上时取消交战)
            else if (rankNew > REP_UNFRIENDLY && rankNew > rankOld && rankOld < REP_NEUTRAL)
                SetAtWar(RepListID(factionEntry->reputationListID), false);
        }

        m_player->ReputationChanged(factionEntry);

        if (rankNew > rankOld)
            return true;
    }
    return false;
}

// 通过阵营模板将对应阵营设为可见
void ReputationMgr::SetVisible(FactionTemplateEntry const* factionTemplateEntry)
{
    if (!factionTemplateEntry || !factionTemplateEntry->faction)
        return;

    if (FactionEntry const* factionEntry = sFactionStore.LookupEntry<FactionEntry>(factionTemplateEntry->faction))
        SetVisible(factionEntry);
}

// 将指定阵营条目设为可见
void ReputationMgr::SetVisible(FactionEntry const* factionEntry)
{
    if (!factionEntry || !factionEntry->HasReputation())
        return;

    FactionStateList::iterator itr = m_factions.find(RepListID(factionEntry->reputationListID));
    if (itr == m_factions.end())
        return;

    SetVisible(&itr->second);
}

// 设置阵营可见标志并通知客户端（受强制隐藏标志约束）
void ReputationMgr::SetVisible(FactionState* faction)
{
    if (!faction)
        return;

    // always invisible or hidden faction can't be make visible (强制不可见或隐藏的阵营禁止设为可见)
    if (faction->Flags & (FACTION_FLAG_INVISIBLE_FORCED | FACTION_FLAG_HIDDEN))
        return;

    // already set
    if (faction->Flags & FACTION_FLAG_VISIBLE)
        return;

    faction->Flags |= FACTION_FLAG_VISIBLE;
    faction->needSend = true;
    faction->needSave = true;

    SendVisible(faction);
}

// 按列表 ID 切换交战状态
void ReputationMgr::SetAtWar(RepListID repListID, bool on)
{
    FactionStateList::iterator itr = m_factions.find(repListID);
    if (itr == m_factions.end())
        return;

    // always invisible or hidden faction can't change war state (强制隐藏阵营不允许修改交战状态)
    if (itr->second.Flags & (FACTION_FLAG_INVISIBLE_FORCED | FACTION_FLAG_HIDDEN))
        return;

    SetAtWar(&itr->second, on);
}

// 内部设置交战状态（受 FACTION_FLAG_PEACE_FORCED 限制）
void ReputationMgr::SetAtWar(FactionState* faction, bool atWar)
{
    if (!faction)
        return;

    // not allow declare war to faction unless already hated or less (强制和平阵营除非声望已达仇恨，否则不允许宣战)
    if (atWar && (faction->Flags & FACTION_FLAG_PEACE_FORCED) != 0 && ReputationToRank(faction->Standing) > REP_HATED)
        return;

    // already set
    if (((faction->Flags & FACTION_FLAG_AT_WAR) != 0) && atWar)
        return;

    if (atWar)
        faction->Flags |= FACTION_FLAG_AT_WAR;
    else
        faction->Flags &= ~uint32(FACTION_FLAG_AT_WAR);

    faction->needSend = true;
    faction->needSave = true;
}

// 按列表 ID 切换阵营“闲置”状态
void ReputationMgr::SetInactive(RepListID repListID, bool on)
{
    FactionStateList::iterator itr = m_factions.find(repListID);
    if (itr == m_factions.end())
        return;

    SetInactive(&itr->second, on);
}

// 内部设置阵营“闲置”标志
void ReputationMgr::SetInactive(FactionState* faction, bool inactive)
{
    if (!faction)
        return;

    // always invisible or hidden faction can't be inactive (不可见或隐藏阵营不能设为闲置)
    if (inactive && ((faction->Flags & (FACTION_FLAG_INVISIBLE_FORCED | FACTION_FLAG_HIDDEN)) || !(faction->Flags & FACTION_FLAG_VISIBLE)))
        return;

    // already set
    if (((faction->Flags & FACTION_FLAG_INACTIVE) != 0) == inactive)
        return;

    if (inactive)
        faction->Flags |= FACTION_FLAG_INACTIVE;
    else
        faction->Flags &= ~FACTION_FLAG_INACTIVE;

    faction->needSend = true;
    faction->needSave = true;
}

// 从数据库查询结果加载玩家声望及标志位，并校验强制态度与和平约束
void ReputationMgr::LoadFromDB(std::unique_ptr<QueryResult> queryResult)
{
    // Set initial reputations (so everything is nifty before DB data load) (先初始化全部默认阵营，再覆盖数据库记录)
    Initialize();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT faction,standing,flags FROM character_reputation WHERE guid = '%u'",GetGUIDLow());

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            FactionEntry const* factionEntry = sFactionStore.LookupEntry<FactionEntry>(fields[0].GetUInt32());
            if (factionEntry && factionEntry->HasReputation())
            {
                FactionState* faction = &m_factions[factionEntry->reputationListID];

                // update standing to current (恢复已保存的声望增量值)
                faction->Standing = int32(fields[1].GetUInt32());

                uint32 dbFactionFlags = fields[2].GetUInt32();

                if (dbFactionFlags & FACTION_FLAG_VISIBLE)
                    SetVisible(faction);                    // have internal checks for forced invisibility (内部会校验强制不可见标志)

                if (dbFactionFlags & FACTION_FLAG_INACTIVE)
                    SetInactive(faction, true);             // have internal checks for visibility requirement (内部会校验可见性前提)

                if (dbFactionFlags & FACTION_FLAG_AT_WAR)   // DB at war
                    SetAtWar(faction, true);                // have internal checks for FACTION_FLAG_PEACE_FORCED (内部会校验强制和平标志)
                else                                        // DB not at war
                {
                    // allow remove if visible (and then not FACTION_FLAG_INVISIBLE_FORCED or FACTION_FLAG_HIDDEN)
                    if (faction->Flags & FACTION_FLAG_VISIBLE)
                        SetAtWar(faction, false);           // have internal checks for FACTION_FLAG_PEACE_FORCED
                }

                // set atWar for hostile (若当前实际或强制声望为敌对及以下，强制启用交战标志)
                ForcedReactions::const_iterator forceItr = m_forcedReactions.find(factionEntry->ID);
                if (forceItr != m_forcedReactions.end())
                {
                    if (forceItr->second <= REP_HOSTILE)
                        SetAtWar(faction, true);
                }
                else if (GetRank(factionEntry) <= REP_HOSTILE)
                    SetAtWar(faction, true);

                // reset changed flag if values similar to saved in DB (若最终标志与数据库一致则无需重复下发或保存)
                if (faction->Flags == dbFactionFlags)
                {
                    faction->needSend = false;
                    faction->needSave = false;
                }
            }
        }
        while (queryResult->NextRow());
    }
}

// 将带有 needSave 脏标记的阵营声望写入 character_reputation 表
void ReputationMgr::SaveToDB()
{
    static SqlStatementID delRep ;
    static SqlStatementID insRep ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delRep, "DELETE FROM character_reputation WHERE guid = ? AND faction=?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insRep, "INSERT INTO character_reputation (guid,faction,standing,flags) VALUES (?, ?, ?, ?)");

    for (auto& m_faction : m_factions)
    {
        FactionState& faction = m_faction.second;
        if (faction.needSave)
        {
            stmtDel.PExecute(m_player->GetGUIDLow(), faction.ID);
            stmtIns.PExecute(m_player->GetGUIDLow(), faction.ID, faction.Standing, faction.Flags);
            faction.needSave = false;
        }
    }
}
