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

#include "Maps/MapGuidAllocator.h"
#include "Log/Log.h"
#include "Util/Util.h"
#include "World/World.h"

// A shorter round means recycled guids may still be known by clients (target, focus, addons)
// 更短的轮转周期意味着回收后的 GUID 仍可能被客户端（目标/焦点/插件）缓存认识
static time_t const MAP_GUID_SHORT_ROUND = DAY;

MapGuidAllocator::MapGuidAllocator(HighGuid high, uint32 mapId, uint32 instanceId, GuidInUse inUse)
    : m_high(high), m_mapId(mapId), m_instanceId(instanceId), m_inUse(std::move(inUse)),
      m_first(1), m_end(ObjectGuid::GetMaxCounter(high) - 1000), m_next(1), m_wraps(0),
      m_roundStart(std::chrono::steady_clock::now())
{
}

// 重置可分配计数器区间 [first, end) 并清空轮转计数
void MapGuidAllocator::SetRange(uint32 first, uint32 end)
{
    std::lock_guard<std::mutex> guard(m_lock);
    m_first = first;
    m_end = end;
    m_next = first;
    m_wraps = 0;
    m_roundStart = std::chrono::steady_clock::now();
}

// 为指定 entry 生成一个未占用的局部低位 GUID 计数器
uint32 MapGuidAllocator::Generate(uint32 entry)
{
    std::lock_guard<std::mutex> guard(m_lock);

    // First round: in order, nothing to check (第一轮直接顺序分配，无需检查占用)
    if (!m_wraps && m_next < m_end)
        return m_next++;

    // Afterwards: skip the counters still in use for this entry, scanning at most one full round
    // 后续轮次：跳过仍被同一 entry 占用的计数器，最多扫描一整轮
    uint32 const size = m_first < m_end ? m_end - m_first : 0;
    for (uint32 i = 0; i < size; ++i)
    {
        if (m_next >= m_end)
            StartNewRound();

        uint32 const counter = m_next++;
        if (!m_inUse(ObjectGuid(m_high, entry, counter)))
            return counter;
    }

    // A full round with nothing free for this entry only happens if something else is already
    // wrong (e.g. objects that should have been removed are still referenced): stop the server the
    // same way ObjectGuidGenerator does on overflow, instead of silently refusing object creations one
    // by one while whatever is holding onto them keeps doing so.
    // 扫描完整一轮仍无空闲计数器通常意味着存在对象泄漏（本应销毁的对象仍被引用）：
    // 按 ObjectGuidGenerator 溢出策略直接关闭服务器，避免逐个静默创建失败而掩盖泄漏问题。
    sLog.outError("Map %u (instance %u): no free %s guid for entry %u among %u counters. Can't continue, shutting down server.",
                  m_mapId, m_instanceId, ObjectGuid::GetTypeName(m_high), entry, size);
    World::StopNow(ERROR_EXIT_CODE);
    return 0;
}

// 游标到达区间末尾时回绕到起点并记录本轮耗时，过短则输出告警日志
void MapGuidAllocator::StartNewRound()
{
    std::chrono::steady_clock::time_point const now = std::chrono::steady_clock::now();
    time_t const roundTime = std::chrono::duration_cast<std::chrono::seconds>(now - m_roundStart).count();
    m_roundStart = now;
    m_next = m_first;
    ++m_wraps;

    sLog.outString("Map %u (instance %u): %s guid counter wrapped around after a round of %s (wrap %u), counters still in use are skipped",
                   m_mapId, m_instanceId, ObjectGuid::GetTypeName(m_high), secsToTimeString(roundTime, true).c_str(), m_wraps);
    if (roundTime < MAP_GUID_SHORT_ROUND)
        sLog.outError("Map %u (instance %u): %s guid counter made a full round in less than %s, recycled guids may still be known by clients",
                      m_mapId, m_instanceId, ObjectGuid::GetTypeName(m_high), secsToTimeString(MAP_GUID_SHORT_ROUND, true).c_str());
}
