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

#ifndef __BATTLEGROUNDMGR_H
#define __BATTLEGROUNDMGR_H

#include "Common.h"
#include "Utilities/EventProcessor.h"
#include "Globals/SharedDefines.h"
#include "Server/DBCEnums.h"
#include "Util/UniqueTrackablePtr.h"
#include "BattleGround.h"
#include "BattleGround/BattleGroundDefines.h"

#include <mutex>
#include <atomic>

typedef std::map<uint32, MaNGOS::unique_trackable_ptr<BattleGround>> BattleGroundSet;

typedef std::unordered_map<uint32, BattleGroundTypeId> BattleMastersMap;
typedef std::unordered_map<uint32, BattleGroundEventIdx> CreatureBattleEventIndexesMap;
typedef std::unordered_map<uint32, BattleGroundEventIdx> GameObjectBattleEventIndexesMap;

class BattleGround;

// 全局战场管理器单例：负责创建/查找/销毁战场实例、构建战场协议包、加载战场军官与事件索引、发放竞技场周/赛季奖励
class BattleGroundMgr
{
    public:
        /* Construction */
        BattleGroundMgr();
        ~BattleGroundMgr();
        void Update(uint32 /*diff*/);                       // 推进战场全局更新、消息队列与过时实例清理

        /* Packet Building (战场相关网络数据包构造辅助接口) */
        static void BuildPlayerJoinedBattleGroundPacket(WorldPacket& /*data*/, Player* /*player*/);
        static void BuildPlayerLeftBattleGroundPacket(WorldPacket& /*data*/, ObjectGuid /*guid*/);
        static void BuildGroupJoinedBattlegroundPacket(WorldPacket& /*data*/, BattleGroundTypeId /*bgTypeId*/, BattleGroundGroupJoinStatus /*status*/);
        static void BuildUpdateWorldStatePacket(WorldPacket& /*data*/, uint32 /*field*/, uint32 /*value*/);
        static void BuildPvpLogDataPacket(WorldPacket& /*data*/, BattleGround* /*bg*/);
        static void BuildBattleGroundStatusPacket(WorldPacket& data, bool bgExists, uint32 bgTypeId, uint32 bgClientInstanceId, bool isRated, uint32 mapId, uint8 queueSlot, uint8 statusId, uint32 time1, uint32 time2, ArenaType arenaType, Team arenaTeam);
        static void BuildPlaySoundPacket(WorldPacket& /*data*/, uint32 /*soundId*/);

        /* Battlegrounds (战场实例与模板管理) */
        BattleGround* GetBattleGroundThroughClientInstance(uint32 /*instanceId*/, BattleGroundTypeId /*bgTypeId*/);
        BattleGround* GetBattleGround(uint32 /*instanceId*/, BattleGroundTypeId /*bgTypeId*/); // there must be uint32 because MAX_BATTLEGROUND_TYPE_ID means unknown (按实例 ID 和类型查找战场)

        BattleGround* GetBattleGroundTemplate(BattleGroundTypeId /*bgTypeId*/) const; // 获取指定类型战场模板（instanceId=0）
        BattleGround* CreateNewBattleGround(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracketEntry, ArenaType arenaType, bool isRated, uint32 instanceId, uint32 clientInstanceId); // 根据模板克隆创建新的战场/竞技场实例及地图

        uint32 CreateBattleGround(BattleGroundTypeId /*bgTypeId*/, bool /*isArena*/, uint32 /*minPlayersPerTeam*/, uint32 /*maxPlayersPerTeam*/, uint32 /*levelMin*/, uint32 /*levelMax*/, char const* /*battleGroundName*/, uint32 /*mapId*/, float /*team1StartLocX*/, float /*team1StartLocY*/, float /*team1StartLocZ*/, float /*team1StartLocO*/, float /*team2StartLocX*/, float /*team2StartLocY*/, float /*team2StartLocZ*/, float /*team2StartLocO*/, float /*startMaxDist*/, uint32 /*playerSkinReflootId*/); // 初始化战场模板并登记

        void AddBattleGround(uint32 instanceId, BattleGroundTypeId bgTypeId, BattleGround* bg);;
        void RemoveBattleGround(uint32 instanceId, BattleGroundTypeId bgTypeId) { m_battleGrounds[bgTypeId].erase(instanceId); }

        void CreateInitialBattleGrounds();                  // 启动时从数据库/DBC 加载全部战场模板
        void DeleteAllBattleGrounds();                      // 关服时释放全部战场实例与模板

        void SendToBattleGround(Player* /*player*/, uint32 /*instanceId*/, BattleGroundTypeId /*bgTypeId*/); // 将玩家传送进入目标战场实例

        uint32 GetMaxRatingDifference() const;
        uint32 GetRatingDiscardTimer()  const;
        uint32 GetPrematureFinishTime() const;


        void DistributeArenaPoints() const;                 // 结算并发放全服每周竞技场点数
        void RewardArenaSeason(uint32 /*seasonId*/);        // 赛季结束发放角斗士等头衔与奖励
        void ResetAllArenaData();                           // 清空全部战队及玩家每周/赛季统计
        void ToggleArenaTesting();                          // 切换竞技场单人测试模式开关
        void ToggleTesting();                               // 切换战场最小人数测试模式开关

        void LoadBattleMastersEntry(bool reload);           // 加载或重载战场军官 NPC 到战场类型的映射
        BattleGroundTypeId GetBattleMasterBG(uint32 entry) const
        {
            BattleMastersMap::const_iterator itr = m_battleMastersMap->find(entry);
            if (itr != m_battleMastersMap->end())
                return itr->second;

            return BATTLEGROUND_TYPE_NONE;
        }
        std::shared_ptr<BattleMastersMap> GetBattleMastersMap() const { return m_battleMastersMap; }

        void LoadBattleEventIndexes(bool reload);           // 加载数据库生物和游戏对象的战场事件双层索引
        const BattleGroundEventIdx GetCreatureEventIndex(uint32 dbGuid) const
        {
            CreatureBattleEventIndexesMap::const_iterator itr = m_creatureBattleEventIndexMap->find(dbGuid);
            if (itr != m_creatureBattleEventIndexMap->end())
                return itr->second;

            return m_creatureBattleEventIndexMap->find(static_cast<uint32>(-1))->second;
        }

        std::shared_ptr<CreatureBattleEventIndexesMap> GetCreatureEventIndexes() const { return m_creatureBattleEventIndexMap; }

        const BattleGroundEventIdx GetGameObjectEventIndex(uint32 dbGuid) const
        {
            GameObjectBattleEventIndexesMap::const_iterator itr = m_gameObjectBattleEventIndexMap->find(dbGuid);
            if (itr != m_gameObjectBattleEventIndexMap->end())
                return itr->second;

            return m_gameObjectBattleEventIndexMap->find(static_cast<uint32>(-1))->second;
        }

        std::shared_ptr<GameObjectBattleEventIndexesMap> GetGameObjectEventIndexes() const { return m_gameObjectBattleEventIndexMap; }

        bool IsArenaTesting() const { return m_arenaTesting; }
        bool IsTesting() const { return m_testing; }

        // Id of the next row of pvpstats_battlegrounds: unique even when several battlegrounds end at the same time
        // (原子生成 pvpstats_battlegrounds 统计表下一条记录 ID)
        uint64 GeneratePvpStatsBattleGroundId() { return m_nextPvpStatsBattleGroundId.fetch_add(1); }
        void LoadPvpStatsBattleGroundId();

        static bool IsArenaType(BattleGroundTypeId /*bgTypeId*/);
        static bool IsBattleGroundType(BattleGroundTypeId bgTypeId) { return !IsArenaType(bgTypeId); }
        static BattleGroundQueueTypeId BgQueueTypeId(BattleGroundTypeId /*bgTypeId*/, ArenaType /*arenaType*/);
        static BattleGroundTypeId BgTemplateId(BattleGroundQueueTypeId /*bgQueueTypeId*/);
        static ArenaType BgArenaType(BattleGroundQueueTypeId /*bgQueueTypeId*/);

        static HolidayIds BgTypeToWeekendHolidayId(BattleGroundTypeId /*bgTypeId*/);
        static BattleGroundTypeId WeekendHolidayIdToBgType(HolidayIds /*holiday*/);
        static bool IsBgWeekend(BattleGroundTypeId /*bgTypeId*/);

        std::set<uint32> const& GetUsedRefLootIds() const { return m_usedRefloot; }

        uint32 GetMinLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId) const;
        uint32 GetMaxLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId) const;
        BattleGroundBracketId GetBattleGroundBracketIdFromLevel(BattleGroundTypeId bgTypeId, uint32 playerLevel) const;

        Messager<BattleGroundMgr>& GetMessager() { return m_messager; }
    private:
        std::mutex schedulerLock;
        std::shared_ptr<BattleMastersMap> m_battleMastersMap;
        std::shared_ptr<CreatureBattleEventIndexesMap> m_creatureBattleEventIndexMap;
        std::shared_ptr<GameObjectBattleEventIndexesMap> m_gameObjectBattleEventIndexMap;

        /* Battlegrounds */
        BattleGroundSet m_battleGrounds[MAX_BATTLEGROUND_TYPE_ID]; // 按战场类型存储模板与全部运行实例
        bool m_arenaTesting;
        bool m_testing;
        std::atomic<uint64> m_nextPvpStatsBattleGroundId;
        std::set<uint32> m_usedRefloot;

        Messager<BattleGroundMgr> m_messager;
};

#define sBattleGroundMgr MaNGOS::Singleton<BattleGroundMgr>::Instance()
#endif
