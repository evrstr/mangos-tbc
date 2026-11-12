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

#ifndef _TAXI_H
#define _TAXI_H

#include "Server/DBCStores.h"

#include <string>
#include <deque>

/////////////////////////////////////////////////
/// @file       Taxi.h
/// @date       May, 2018
/// @brief      Helper container includes for updated player taxi system. (新版玩家飞行路线系统的辅助容器定义)
/////////////////////////////////////////////////

class Player;

// 飞行路线系统命名空间
namespace Taxi
{
    // Aliases to help differentiate different numbers from each other (类型别名，用于区分不同编号的语义)
    typedef uint32 PathID;  // Flight path entry id (飞行路径条目 ID)
    typedef uint32 DestID;  // Destination entry id (taxi flight master stop as seen by the game internals) (飞行节点/目的地条目 ID)
    typedef uint32 Index;   // Index of a flight spline waypoint in a single flight route (单段飞行路径中的样条路点索引)

    /// Route is a struct describing a single taxi flight path between 2 destinations (描述两点之间单段飞行路径的结构体)
    struct Route
    {
        Route(PathID pathID, DestID pathStart, DestID pathEnd, Index nodeStart, Index nodeEnd, uint32 cost) :
            pathID(pathID), destStart(pathStart), destEnd(pathEnd), nodeStart(nodeStart), nodeEnd(nodeEnd), cost(cost) {}

        PathID pathID;      // Taxi flight path entry id (飞行路径 ID)
        DestID destStart;   // Start destination id for entry (起点飞行节点 ID)
        DestID destEnd;     // End destination for entry (终点飞行节点 ID)
        Index nodeStart;    // Real entry index start or a modified junction point for multi-route flights (实际起始路点索引，或多段航线裁剪后的衔接点)
        Index nodeEnd;      // Real enrty index end or a modified junction point for multi-route flights (实际结束路点索引，或多段航线裁剪后的衔接点)
        uint32 cost;        // Fully pre-calculated cost of this particular route (in non-persisting multi-route flights) (本段航线预计算费用)
    };

    /// Roadmap is a sequence of Routes, the source for building a spline or a series of splines for the entire ride (航线序列，用于构建整趟飞行的样条路径)
    typedef std::deque<Route> Roadmap;
    /// Waypoint is an alias for TaxiPathNodeEntry, a part of the taxi flight spline (飞行样条路径上的单个路点指针别名)
    typedef TaxiPathNodeEntry const* Waypoint;
    /// Map is a container which describes a full proto-spline made out of taxi node entries in a sequential order for a single game level (单张地图内按顺序排列的飞行路点序列)
    typedef std::deque<Waypoint> Map;
    /// Atlas contains one or more Maps in a sequential order (按顺序包含一个或多个跨地图航段的路点集合)
    typedef std::deque<Map> Atlas;

    // 飞行路线追踪器状态枚举
    enum TrackerState
    {
        TRACKER_EMPTY    = 0, // Container is empty (容器为空)
        TRACKER_STAGING  = 1, // At least one route added, its possible to add more routes (已添加至少一段航线，可继续追加)
        TRACKER_STANDBY  = 2, // Container is finalized: all routes and splines are calculated (航线与样条已就绪，等待起飞)
        TRACKER_FLIGHT   = 3, // Container is tracking a taxi flight spline (正在飞行追踪中)
        TRACKER_TRANSFER = 4, // Container is waiting for server to apply a new taxi flight spline after map change (等待跨地图切换后应用新航段样条)
    };

    /// This is a container representing player's current taxi ride's routes and destinations. (表示玩家当前飞行旅程各航段与目的地状态的追踪器容器)
    class Tracker
    {
        public:
            explicit Tracker(Player& owner) :
                m_owner(owner), m_state(TRACKER_EMPTY), m_cost(0), m_displayId(0), m_location(nullptr), m_locationIndex(0), m_resumeIndex(0), m_debug(false) {}

            // Disable funny ctors (禁用拷贝与移动构造/赋值)
            Tracker(Tracker const&) = delete;
            void operator=(const Tracker&) = delete;
            Tracker(Tracker&&) = delete;
            Tracker& operator=(Tracker&&) = delete;

            /// Save contents: string serialization (将当前飞行状态序列化为数据库存储字符串)
            std::string Save();
            /// Load contents: string deserialization (从存储字符串反序列化恢复飞行状态)
            bool Load(std::string& string, DestID& destOrphan);

            TrackerState GetState() const { return m_state; }
            /// Notify the container to switch into certain state if applicable (在满足状态转移条件时切换追踪器状态)
            bool SetState(TrackerState state);

            /// Get current active route (a single taxi path) (获取当前激活的单段航线)
            const Route& GetRoute() const { return m_routes.front(); }
            /// Get all pre-calculated routes (获取全部预计算航线队列)
            const Roadmap& GetRoadmap() const { return m_routes; }
            /// Get the current spline for the map (获取当前地图对应的飞行路点序列)
            const Map& GetMap() const { return m_atlas.front(); }
            /// Get a collection of splines for the entire ride (获取整趟飞行的全部地图路点集合)
            const Atlas& GetAtlas() const { return m_atlas; }

            /// Get cost of the current route (获取当前航段费用)
            uint32 GetCost() const { return (!m_routes.empty() ? m_routes.front().cost : 0); }
            /// Get total cost of the entire ride with all routes included (获取整趟飞行的总费用)
            uint32 GetCostTotal() const { return m_cost; }

            /// Get flying mount displayid for the entire ride (in effect until logout) (获取整趟飞行使用的坐骑显示 ID)
            uint32 GetMountDisplayId() const { return m_displayId; }

            /// Get the last tracked waypoint on the current taxi spline (获取当前样条上最近一次追踪到的路点)
            Waypoint GetCurrentWaypoint() const { return m_location; }
            /// Get the last tracked node position on the current taxi spline (获取当前样条上最近一次追踪到的索引位置)
            size_t GetCurrentWaypointIndex() const { return m_locationIndex; }
            /// Get the last known node we must resume from in the current taxi spline in the case of loading (获取读档恢复时当前样条的断点索引)
            size_t GetResumeWaypointIndex() const { return m_resumeIndex; }

            /// Add a taxi flight path to the container in addition to the last one, returns true if input is correct (向追踪器追加一条飞行路径，合法则返回 true)
            bool AddRoute(TaxiPathEntry const* entry, float discountMulti = 0.0f, bool requireModel = true);
            /// Add a taxi flight path to the container by ID in addition to the last one, returns true if input is correct (按路径 ID 追加一条飞行路径)
            bool AddRoute(PathID pathID, float discountMulti = 0.0f, bool requireModel = true);
            /// Add a taxi flight path to the container by destinations in addition to the last one, returns true if input is correct (按起止目的地 ID 追加一条飞行路径)
            bool AddRoute(DestID start, DestID end, float discountMulti = 0.0f, bool requireModel = true);
            /// Replace any existing container contents with a chain of flights defined by a vector of destinations, returns true if input is correct. (用目的地序列重建整趟多段飞行路线)
            bool AddRoutes(std::vector<DestID> const& destinations, float discountMulti = 0.0f, bool requireModel = true);
            /// Replace any existing container contents with a chain of flights defined by a deque of flight path ids, returns true if input is correct. (用路径 ID 序列重建整趟多段飞行路线)
            bool AddRoutes(std::deque<PathID> const& paths, float discountMulti = 0.0f, bool requireModel = true);
            /// Finalize the container and prepare for the tracking of the flight. Used when all routes are added. It will be impossible to alter the container until the flight ends (完成航线添加并构建各图集样条，进入待命状态)
            bool Prepare(Index nodeResume = 0);
            /// Clear the container by removing all contents. Can't be performed if taxi flight tracking is currently active, unless forced (清空追踪器数据；飞行中除非强制否则不可清空)
            bool Clear(bool forced = false);

            /// Update tick: notify the container about arrival at the taxi spline node, advances internal node tracker and provides info about changes. Returns true when current route has changed (到达新路点时推进内部追踪索引并检测航段切换)
            bool UpdateRoute(const TaxiPathNodeEntry *entry, PathID& start, PathID& end);

            /// Try to join two routes together by determining junction points. Returns true on success. (尝试裁剪相邻两段航线的起降衔接点以实现平滑转乘)
            static bool Trim(Route& first, Route& second);

        private:
            Player& m_owner;                                // 所属玩家引用
            TrackerState m_state;                           // 当前追踪器状态
            uint32 m_cost;                                  // 整趟飞行累计费用
            uint32 m_displayId;                             // 飞行坐骑模型显示 ID
            Waypoint m_location;                            // 当前所在路点指针
            size_t m_locationIndex;                         // 当前地图路点序列中的位置索引
            size_t m_resumeIndex;                           // 恢复飞行时的起始路点索引
            Roadmap m_routes;                               // 航段队列
            Atlas m_atlas;                                  // 按地图切分的路点集合

        public:
            bool m_debug;                                   // 调试日志开关
    };
}

#endif
