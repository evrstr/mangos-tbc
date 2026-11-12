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

#include "Entities/Taxi.h"
#include "Entities/Player.h"
#include "Globals/ObjectMgr.h"
#include "World/World.h"

#include <cmath>
#include <sstream>

/////////////////////////////////////////////////
/// @file       Taxi.cpp
/// @date       May, 2018
/// @brief      Helper containers implementations for updated player taxi system. (新版玩家飞行路线系统辅助容器实现)
/////////////////////////////////////////////////

using namespace Taxi;

// 序列化当前飞行路径与断点节点索引，用于角色下线保存
std::string Tracker::Save()
{
    // Writes in modified format. (使用新版路径 ID 格式写出)

    // Original numbers format: "src dest dest dest "                   variable amount of destinations in the string, dangling destinations possible, trailing space @ the end)
    // Updated  numbers format: "resumenode:src:dest:dest:dest"         strictly formatted, introduced in taxi system rewrite
    // Modified numbers format: "resumenode#pathid#pathid#pathid"       strictly formatted, directly uses flights, introduced in an update for redesigned taxi system
    // 旧格式说明："resumenode#pathid#..." 为当前使用的格式

    if (m_state < TRACKER_FLIGHT)
        return "";

    // Bugcheck: container continuity error (断言检查：航段与图集必须非空)
    MANGOS_ASSERT(!m_routes.empty() && !m_atlas.empty());

    std::ostringstream stream;

    // Save resume node for the current route (保存当前航段的断点路点索引)
    uint32 node = GetMap().at(size_t(m_locationIndex))->index;
    // Make sure resume node is not the very first one or the very last one - saving and loading those makes no practical sense to us (确保断点路点夹在起止点之间，避免首尾无效恢复)
    node = std::max(std::min(node, (GetRoute().nodeEnd - 1)), (GetRoute().nodeStart + 1));
    stream << node;

    const size_t count = (sWorld.getConfig(CONFIG_BOOL_LONG_TAXI_PATHS_PERSISTENCE) ? m_routes.size() : 1);

    for (size_t i = 0; i < count; ++i)
        stream << "#" << m_routes[i].pathID;

    return stream.str();
}

// 从序列化字符串恢复飞行路径与断点位置；若为旧孤立数据则通过 destOrphan 返回目的地
bool Tracker::Load(std::string& string, DestID &destOrphan)
{
    if (!Clear())
        return false;

    // Read a modified format. All old entries were converted to orphaned taxi node records via DB update for failsafe loading. (读取当前格式字符串；历史旧格式已通过数据库更新转为孤立节点)

    // Original numbers format: "src dest dest dest "                   variable amount of destinations in the string, dangling destinations possible, trailing space @ the end)
    // Updated  numbers format: "resumenode:src:dest:dest:dest"         strictly formatted, introduced in taxi system rewrite
    // Modified numbers format: "resumenode#pathid#pathid#pathid"       strictly formatted, directly uses flights, introduced in an update for redesigned taxi system

    Tokens tokens = StrSplit(string, "#");

    std::deque<PathID> numbers;

    for (auto& token : tokens)
        numbers.push_back(uint32(std::stoi(token)));

    // Try to parse as robustly as possible (按解析出的数字数量健壮恢复航线)
    switch (numbers.size())
    {
        case 0:             // No routes (无飞行记录)
            return true;
        case 1:             // Orphaned: probably loading from an outdated format, just dangling destination node (仅含单个目的地节点，作为孤立节点处理)
            destOrphan = numbers.front();
            return false;
        default:
        {
            // Extract and verify last known flight path node index (提取并校验上次记录的断点路点索引)
            uint32 nodeResume = numbers.front();
            numbers.pop_front();

            // On successful load, prepares the container right away, otherwise does all the housekeeping (加载成功则立即准备样条，否则清空状态)
            if (!AddRoutes(numbers, 0.0f, false) || !Prepare(nodeResume))
            {
                Clear();
                return false;
            }
        }
    }
    return true;
}

// 按状态机规则切换追踪器状态
bool Tracker::SetState(TrackerState state)
{
    switch (state)
    {
        case TRACKER_FLIGHT:
            m_state = ((m_state == TRACKER_STANDBY || m_state == TRACKER_TRANSFER) ? state : m_state);
            return (m_state == state);
        case TRACKER_TRANSFER:
            m_state = ((m_state == TRACKER_FLIGHT) ? state : m_state);
            return (m_state == state);
        // The rest can't be set outside of the container under any circumstances (其余状态不允许从外部直接设置)
        default:
            break;
    }
    return false;
}

// 追加单条飞行路径条目，计算折扣费用并自动衔接多段航线
bool Tracker::AddRoute(const TaxiPathEntry *entry, float discountMulti /*= 0.0f*/, bool requireModel /*= true*/)
{
    // Cant be altered while taxi ride is prepared (已就绪或飞行中不可修改航线)
    if (m_state > TRACKER_STAGING)
        return false;

    // Verify input (校验路径条目有效)
    if (!entry)
        return false;

    uint32 cost = entry->price;

    const bool commercial = discountMulti > 0.0f;

    // Can't add taxi path without mount display id unless specified otherwise (除非显式允许跳过，否则必须能查到对应阵营坐骑模型)
    uint32 displayId = sObjectMgr.GetTaxiMountDisplayId(entry->from, m_owner.GetTeam());
    if (requireModel && !displayId)
        return false;

    // Use first route's mount appearance for the entirety of the flight (整趟飞行统一沿用首段航线的坐骑外观)
    if (!m_displayId)
        m_displayId = displayId;

    if (commercial)
    {
        // Rounded to the nearest copper (按声望折扣四舍五入至铜币)
        cost = uint32(std::lrintf(float(cost) * discountMulti));
        m_cost += cost;
        // If global preference is set, do not record individual flight costs (开启长航线持久化时，总费用统一在首段扣除，不单独记录分段费用)
        if (sWorld.getConfig(CONFIG_BOOL_LONG_TAXI_PATHS_PERSISTENCE))
            cost = 0;
    }

    TaxiPathNodeList const& nodes = sTaxiPathNodesByPath[entry->ID];
    m_routes.push_back(Route(entry->ID, entry->from, entry->to, nodes.front()->index, nodes.back()->index, cost));

    const size_t count = m_routes.size();

    // Multi-route taxi path: set shortcut (多段飞行时尝试裁剪相邻航段的衔接节点)
    if (count > 1)
        Trim(m_routes[count - 2], m_routes[count - 1]);

    return true;
}

// 按飞行路径 ID 追加单段航线
bool Tracker::AddRoute(PathID pathID, float discountMulti /*= 0.0f*/, bool requireModel /*= true*/)
{
    // Cant be altered while taxi ride is prepared (已就绪或飞行中不可修改)
    if (m_state > TRACKER_STAGING)
        return false;

    TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(pathID);

    return (entry && AddRoute(entry, discountMulti, requireModel));
}

// 按起止飞行点 ID 查找并追加单段航线
bool Tracker::AddRoute(DestID start, DestID end, float discountMulti /*= 0.0f*/, bool requireModel /*= true*/)
{
    // Cant be altered while taxi ride is prepared (已就绪或飞行中不可修改)
    if (m_state > TRACKER_STAGING)
        return false;

    uint32 pathID, cost;
    sObjectMgr.GetTaxiPath(start, end, pathID, cost);

    return (pathID && AddRoute(pathID, discountMulti, requireModel));
}

// 清空后按目的地节点列表批量构建多段飞行路线
bool Tracker::AddRoutes(const std::vector<DestID>& destinations, float discountMulti /*= 0.0f*/, bool requireModel /*= true*/)
{
    if (!Clear())
        return false;

    for (uint32 i = 1; i < destinations.size(); ++i)
    {
        // Stop on bad input (遇到无效航段即停止追加)
        if (!AddRoute(destinations.at(i - 1), destinations.at(i), discountMulti, requireModel))
            break;
    }

    // If at least one insertion was accepted, return true (至少成功添加一段航线即返回 true)
    return !m_routes.empty();
}

// 清空后按路径 ID 双端队列批量构建多段飞行路线
bool Tracker::AddRoutes(const std::deque<PathID>& paths, float discountMulti /*= 0.0f*/, bool requireModel /*= true*/)
{
    if (!Clear())
        return false;

    for (auto i = paths.begin(); i != paths.end(); ++i)
    {
        // Stop on bad input (遇到无效航段即停止追加)
        if (!AddRoute(*i, discountMulti, requireModel))
            break;
    }

    // If at least one insertion was accepted, return true (至少成功添加一段航线即返回 true)
    return !m_routes.empty();
}

// 定稿所有航段，按地图切换点拆分为 Atlas 样条路点集合并计算断点索引
bool Tracker::Prepare(Index nodeResume /*= 0*/)
{
    // Cant be altered while taxi ride is prepared (已就绪或飞行中不可重新准备)
    if (m_state > TRACKER_STAGING)
        return false;

    // Cant be finalized when no routes added (未添加任何航段时不可准备)
    if (m_routes.empty())
        return false;

    Route& current = m_routes.front();

    // If global preference is set, we will charge total cost for the entire ride on first flight at once (开启长航线持久化时，首段记录整趟飞行总费用)
    if (sWorld.getConfig(CONFIG_BOOL_LONG_TAXI_PATHS_PERSISTENCE))
        current.cost = m_cost;

    // Sanity check: make sure that resume node for current route hasn't gotten cut off or out of bounds (边界检查：确保断点索引未超出首段结束节点)
    nodeResume = std::min(nodeResume, current.nodeEnd);

    m_atlas.clear();
    // Open a new map in the atlas (创建首张地图的路点容器)
    m_atlas.push_back(Map());
    // Start populating map(s) (开始填充各张地图的飞行路点)
    const TaxiPathNodeEntry* prev = nullptr;
    for (auto i = m_routes.begin(); i != m_routes.end(); ++i)
    {
        const TaxiPathNodeList& nodes = sTaxiPathNodesByPath[(*i).pathID];
        for (auto j = nodes.begin(); j != nodes.end(); ++j)
        {
            // Abide route trimmed routes when building a spline (跳过已被裁剪掉的起降路点)
            if ((*j)->index > (*i).nodeEnd || (*j)->index < (*i).nodeStart)
                continue;

            // Resume node defined: we are loading, additional care is required (读档恢复模式：处理断点之前的跨图路点与恢复索引计算)
            if (nodeResume && (*i).pathID == current.pathID)
            {
                // Internal algorithm sanity check: make sure resume node for current route will end up on the same map
                // Dont build spline for preceding nodes which are located on another map
                // 不构建断点之前位于其他地图上的旧路点
                if ((*j)->index < nodes[nodeResume]->index && (*j)->mapid != nodes[nodeResume]->mapid)
                    continue;
                // When resume node reached on the route: calculate resume location index on the spline (which will be equal to the current size on push)
                // 到达断点路点时，记录其在当前样条中的恢复位置索引
                if ((*j)->index == nodeResume)
                {
                    m_resumeIndex = m_atlas.back().size();
                    m_locationIndex = m_resumeIndex;
                }
            }
            // Detect level change and insert a new map (检测跨地图切换点并在图集中新开一段地图样条)
            if ((*j)->index >= (*i).nodeStart && (*j)->index <= (*i).nodeEnd)
            {
                if (prev && (*j)->mapid != prev->mapid)
                {
                    // Detect map change and advance atlas by adding a new map (检测到地图切换，校验目标地图存在后新增 Map)
                    if (sMapStore.LookupEntry((*j)->mapid))
                    {
                        // Bugcheck: latest finished map spline is suspiciously short (断言：上一张地图的样条至少需包含 3 个路点)
                        MANGOS_ASSERT(m_atlas.back().size() > 2);
                        m_atlas.push_back(Map());
                    }
                    else
                    {
                        sLog.outError("TAXI: Malformed flight path node detected and ignored. Node %u on flight path id %u refers non-existent mapid %u",
                                      (*j)->index, (*j)->path, (*j)->mapid);
                        continue;
                    }
                }
            }
            // Add entry to the latest Map (将路点加入当前地图样条)
            m_atlas.back().push_back((*j));
            if ((*j)->index > (*i).nodeStart)
                prev = (*j);
        }
    }
    // Bugcheck: latest finished map spline is suspiciously short (断言：最终地图样条至少需包含 3 个路点)
    MANGOS_ASSERT(m_atlas.back().size() > 2);

    m_state = TRACKER_STANDBY;
    return true;
}

// 清空追踪器中的所有航段与进度状态
bool Tracker::Clear(bool forced)
{
    // Cant be cleared while taxi ride is prepared, unless forced (除非强制清空，否则准备就绪或飞行期间不可清空)
    if (m_state > TRACKER_STAGING && !forced)
        return false;
    m_routes.clear();
    m_cost = 0;
    m_displayId = 0;
    m_atlas.clear();
    m_location = nullptr;
    m_locationIndex = 0;
    m_resumeIndex = 0;
    m_state = TRACKER_EMPTY;
    return true;
}

// 飞行途中抵达某路点时更新进度；若发生航段切换则返回 true 并输出起止路径 ID
bool Tracker::UpdateRoute(const TaxiPathNodeEntry* entry, PathID& start, PathID& end)
{
    if (!entry)
        return false;

    // Cant be advanced when no taxi flight is being tracked (非飞行状态不推进路点)
    if (m_state != TRACKER_FLIGHT)
        return false;

    // Bugcheck: continuity error - either routes or splines are missing (断言：飞行中图集与航段队列均不能为空)
    MANGOS_ASSERT(!m_atlas.empty() && !m_routes.empty())

    const Map& map = GetMap();

    // Advance internal spline node counter only if not on the first waypoint (非首个路点时递增当前样条位置索引)
    if (m_location)
    {
        ++m_locationIndex;
        m_resumeIndex = m_locationIndex;
    }

    m_location = entry;

    // Bugcheck: internal spline node counter overflow (断言：路点索引不能越界)
    MANGOS_ASSERT(size_t(m_locationIndex) < map.size())

    if (entry == map.back())
    {
        // Acknowledge spline end: nuke current map in the atlas, reset location index
        // That does not mean, however, that taxi ride was completed right away, as it can be a multimap flight route
        // 当前地图样条已飞抵终点，弹出该 Map 并重置索引（可能还有下一张地图的航段）
        m_atlas.pop_front();
        m_location = nullptr;
        m_locationIndex = 0;
        m_resumeIndex = 0;
    }

    const Route& route = GetRoute();

    if (entry->index == route.nodeEnd)
    {
        // Acknowledge the end of the current route to other systems (通知外部系统当前航段已结束并扣除已消耗费用)
        end = route.pathID;
        // Nuke the route from the container (从航段队列移除已完成航段)
        m_cost -= route.cost;
        m_routes.pop_front();
        if (m_routes.empty())
        {
            m_state = TRACKER_EMPTY;
            m_displayId = 0;
        }
        return true;
    }
    if (entry->index == route.nodeStart)
    {
        // Acknowledge the start of the current route to other systems (通知外部系统当前航段已开始)
        start = route.pathID;
        return true;
    }

    return false;
}

// 裁剪相邻两段航线的降落与起飞重叠部分，优先使用数据库配置捷径，否则按 48 码距离动态估算衔接点
bool Tracker::Trim(Route& first, Route& second)
{
    if (first.destEnd != second.destStart || first.pathID == second.pathID)
        return false;

    // Retail uses serverside pre-definied junction points for all interconnected flight paths.
    // We have a fallback custom automatic runtime trimmer. The result will not match retail and can be visually unpleasing at times.
    // For best experience, the taxi shortcuts table needs to be populated with faithful data.
    // 优先查询服务端预配置的飞行捷径表，缺失时再回退至运行时距离估算裁剪

    // Lookup known shortcuts first (查询已知的起降捷径长度)
    uint32 lengthTakeoff = 0;
    uint32 lengthLanding = 0;

    TaxiShortcutData data;

    // Get the landing range of the first route if exists (获取第一段航线的降落裁剪长度)
    if (sObjectMgr.GetTaxiShortcut(first.pathID, data))
        lengthLanding = data.lengthLanding;

    // Get the takeoff range of the second route if exists (获取第二段航线的起飞裁剪长度)
    if (sObjectMgr.GetTaxiShortcut(second.pathID, data))
        lengthTakeoff = data.lengthTakeoff;

    // Sanity check for bounds (限制裁剪长度不超过航段总节点边界)
    lengthLanding = std::min(lengthLanding, (first.nodeEnd - 1));
    lengthTakeoff = std::min(lengthTakeoff, (second.nodeEnd - 1));

    // Both landing and takeoff data is present (起降数据均存在时直接应用配置裁剪)
    if (lengthLanding && lengthTakeoff)
    {
        first.nodeEnd -= lengthLanding;
        second.nodeStart += lengthTakeoff;
        return true;
    }

    // Try to trim dynamically if data is not available or incomplete (若无配置数据或不完整，则按距中转点 48 码外动态查找衔接路点)
    TaxiPathNodeList const& waypoints1 = sTaxiPathNodesByPath[first.pathID];
    TaxiPathNodeList const& waypoints2 = sTaxiPathNodesByPath[second.pathID];
    TaxiNodesEntry const* destination = sTaxiNodesStore.LookupEntry(first.destEnd);

    if (destination && waypoints1 != waypoints2)
    {
        // Linear complexity for better performance (线性双端扫描以保持较好性能)
        const TaxiPathNodeEntry* last1 = nullptr;
        const TaxiPathNodeEntry* last2 = nullptr;
        const double refdistsq = double(48.0f * 48.0f);
        auto i1 = (waypoints1.rbegin() + lengthLanding);
        auto i2 = (waypoints2.begin() + lengthTakeoff);
        for (; (i1 != waypoints1.rend() && i2 != waypoints2.end()); (++i1, ++i2))
        {
            // Verify that both paths are on the same map (确保两段衔接路点位于同一地图)
            if ((*i1)->mapid != (*i2)->mapid)
                break;
            // Verify that there are no transitions between maps on either end (确保扫描过程中两端均未发生跨地图切换)
            if ((last1 && last1->mapid != (*i1)->mapid) || (last2 && last2->mapid != (*i2)->mapid))
                break;
            // Quick check for distance to destination on 2d plane (计算路点到中转飞行点在 XY 平面上的距离平方)
            auto distsq = [&destination] (TaxiPathNodeEntry const* node) {
                return (std::pow((destination->x - node->x), 2) + std::pow((destination->y - node->y), 2));
            };
            if (distsq(*i1) > refdistsq && distsq(*i2) > refdistsq)
            {
                first.nodeEnd = (*i1)->index;
                second.nodeStart = (*i2)->index;
                return true;
            }
            last1 = (*i1);
            last2 = (*i2);
        }
    }
    return false;
}
