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

#include "Maps/ObjectPosSelector.h"
#include "Entities/Object.h"

// The bigger this value, the more space npcs require around their target
// 此系数越大，NPC 在目标周围占位所需的间隔角度越大
#define OCCUPY_POS_ANGLE_ATAN_FACTOR                      1.8f

ObjectPosSelector::ObjectPosSelector(float x, float y, float dist, float searchedForSize, WorldObject const* searchPosFor) :
    m_centerX(x), m_centerY(y), m_searcherDist(dist), m_searchPosFor(searchPosFor)
{
    // if size == 0, m_anglestep will become 0 -> freeze (若尺寸为 0 则步进角为 0 会导致死循环，使用默认值兜底)
    if (searchedForSize == 0.0f)
        searchedForSize = DEFAULT_WORLD_OBJECT_SIZE;

    // undefined behaviour (避免除以 0 的未定义行为)
    if (m_searcherDist == 0.0f)
        m_searcherDist = DEFAULT_WORLD_OBJECT_SIZE;

    m_searchedForReqHAngle = atan(OCCUPY_POS_ANGLE_ATAN_FACTOR * searchedForSize / m_searcherDist);

    // Really init in InitilizeAngle (实际初始化在 InitializeAngle 中完成)
    m_nextUsedAreaItr[USED_POS_PLUS]  = m_UsedAreaLists[USED_POS_PLUS].begin();
    m_nextUsedAreaItr[USED_POS_MINUS] = m_UsedAreaLists[USED_POS_MINUS].begin();
    m_stepAngle[USED_POS_PLUS]  = 0.0f;
    m_stepAngle[USED_POS_MINUS] = 0.0f;
}

/**
 * Add used area (circle) near target object excluded from possible searcher position
 * 添加目标附近已被占用的圆形区域，从候选搜索位置中排除
 *
 * @param obj  Object that occupies area (占用该区域的对象)
 * @param angle Angle of used circle center point from target-searcher line (占用圆心相对于参考连线的夹角)
 * @param dist  Distance from target object center point to used circle center point (占用对象中心到目标中心的距离)
 *
 * Used circles data stored as projections to searcher dist size circle as angle coordinate and half angle size
 * 已占用圆以角度坐标与半角宽度的形式投影并存储到搜索半径圆周上
 */
void ObjectPosSelector::AddUsedArea(WorldObject const* obj, float angle, float dist)
{
    MANGOS_ASSERT(obj);

    // skip some unexpected results. (距离为 0 时跳过以避免除零)
    if (dist == 0.0f)
        return;

    // (half) angle that obj occupies (计算该对象在目标距离上占据的半角大小)
    float sr_angle = atan(OCCUPY_POS_ANGLE_ATAN_FACTOR * obj->GetObjectBoundingRadius() / dist);

    if (angle >= 0)
        m_UsedAreaLists[USED_POS_PLUS].insert(UsedArea(angle, OccupiedArea(sr_angle, obj)));
    else
        m_UsedAreaLists[USED_POS_MINUS].insert(UsedArea(-angle, OccupiedArea(sr_angle, obj)));
}

/**
 * Check searcher circle not intercepting with used circle
 * 检查搜索者占位圆是否不与指定已占用圆重叠
 *
 * @param usedArea Used circle as projection to searcher distance circle in angles form
 * @param side     Side of used circle
 * @param angle    Checked angle
 *
 * @return true, if used circle not intercepted with searcher circle in terms projection angles
 */
bool ObjectPosSelector::CheckAngle(UsedArea const& usedArea, UsedAreaSide side, float angle) const
{
    float used_angle = usedArea.first * SignOf(side);
    float used_offset = usedArea.second.angleOffset;

    return fabs(used_angle - angle) > used_offset || (m_searchPosFor && usedArea.second.occupyingObj == m_searchPosFor);
}

/**
 * Check original (0.0f) angle fit to existed used area excludes
 * 检查原始正前方（0.0f 角度）是否与现有已占用区域不冲突
 *
 * @return true, if 0.0f angle with m_searcher_halfangle*2 angle size not intercept with used circles
 */
bool ObjectPosSelector::CheckOriginalAngle() const
{
    // check first left/right used angles if exists (检查左右两侧最靠近 0 度的首个已占用区域)
    return (m_UsedAreaLists[USED_POS_PLUS].empty()  || CheckAngle(*m_UsedAreaLists[USED_POS_PLUS].begin(), USED_POS_PLUS, 0.0f)) &&
           (m_UsedAreaLists[USED_POS_MINUS].empty() || CheckAngle(*m_UsedAreaLists[USED_POS_MINUS].begin(), USED_POS_MINUS, 0.0f));
}

/**
 * Initialize data for search angles starting from first possible angle at both sides
 * 初始化左右两侧角度搜索状态
 */
void ObjectPosSelector::InitializeAngle()
{
    InitializeAngle(USED_POS_PLUS);
    InitializeAngle(USED_POS_MINUS);
}

/**
 * Initialize data for search angles starting from first possible angle at side
 * 初始化单侧角度搜索起点（考虑对侧首个占用区域是否越过 0 度线）
 */
void ObjectPosSelector::InitializeAngle(UsedAreaSide side)
{
    m_nextUsedAreaItr[side] = m_UsedAreaLists[side].begin();

    // if another side not alow use 0.0f angle calculate possible value in 0..m_searchedForReqHAngle range
    // 若对侧占用区域越界导致不能从 0.0f 起步，则计算本侧最小合法起始角度
    if (!m_UsedAreaLists[~side].empty())
    {
        UsedArea const& otherArea = *m_UsedAreaLists[~side].begin();
        m_stepAngle[side] = std::max(m_searchedForReqHAngle + otherArea.second.angleOffset - otherArea.first, 0.0f);
    }
    else                                                    // Other side empty. start from 0 (对侧为空则从 0 度起始)
        m_stepAngle[side] = 0.0f;

    // As m_stepAngle will be incremented first in ::NextSideAngle (预先减去一个步长，因为 NextSideAngle 会先自增)
    m_stepAngle[side] -= m_searchedForReqHAngle;
}

/**
 * Find next angle in free area
 * 在空闲角度区域中交替向左右两侧查找下一个可用角度
 *
 * @param angle    Return at success found angle
 *
 * @return true, if angle found
 */
bool ObjectPosSelector::NextAngle(float& angle)
{
    // loop until both side fail and leave 0..PI (循环推进左右两侧直到超出 [0, PI] 范围)
    for (;;)
    {
        // ++ direction less updated (优先推进当前角度较小的一侧)
        if (m_stepAngle[USED_POS_PLUS] < M_PI_F && m_stepAngle[USED_POS_PLUS] <= m_stepAngle[USED_POS_MINUS])
        {
            if (NextSideAngle(USED_POS_PLUS, angle))
                return true;
        }
        // -- direction less updated
        else if (m_stepAngle[USED_POS_MINUS] < M_PI_F)
        {
            if (NextSideAngle(USED_POS_MINUS, angle))
                return true;
        }
        // both sides finishes (两侧均已遍历完毕)
        else
            break;
    }

    // no angles
    return false;
}

/**
 * Find next angle at side
 * 在指定侧步进查找下一个不被占用的角度，若碰到占用区域则跳过该占用段
 *
 * @param side     Side of angle
 * @param angle    Return at success found angle
 *
 * @return true, if angle found
 *
 */
bool ObjectPosSelector::NextSideAngle(UsedAreaSide side, float& angle)
{
    // next possible angle (尝试下一个候选角度)
    m_stepAngle[side] += (m_searchedForReqHAngle + 0.01);

    // prevent jump to another side (防止角度超过 PI 越界到另一侧)
    if (m_stepAngle[side] > M_PI_F)
        return false;

    // no used area anymore on this side (本侧后续已无占用区域，直接返回)
    if (m_nextUsedAreaItr[side] == m_UsedAreaLists[side].end())
    {
        angle = m_stepAngle[side] * SignOf(side);
        return true;
    }

    // Already occupied and no better found (若当前占用属于自身，或下一占用区尚在安全距离之外，则当前角度可用)
    if ((m_searchPosFor && m_nextUsedAreaItr[side]->second.occupyingObj == m_searchPosFor) ||
            // Next occupied is too far away
            (m_stepAngle[side] + m_searchedForReqHAngle < m_nextUsedAreaItr[side]->first - m_nextUsedAreaItr[side]->second.angleOffset))
    {
        angle = m_stepAngle[side] * SignOf(side);
        return true;
    }

    // angle set at first possible pos after passed m_nextUsedAreaItr (发生重叠时将游标直接跳过当前占用区末尾)
    m_stepAngle[side] = m_nextUsedAreaItr[side]->first + m_nextUsedAreaItr[side]->second.angleOffset;

    ++m_nextUsedAreaItr[side];

    return false;
}

/**
 * Find next angle in used area, that used if no angle found in free area with LoS
 * 当空闲区域均不满足视线等条件时，按距离从小到大在已占用角度中选择回退位置
 *
 * @param angle    Return at success found angle
 *
 * @return true, if angle found
 */
bool ObjectPosSelector::NextUsedAngle(float& angle)
{
    if (m_nextUsedAreaItr[USED_POS_PLUS] == m_UsedAreaLists[USED_POS_PLUS].end() &&
            m_nextUsedAreaItr[USED_POS_MINUS] == m_UsedAreaLists[USED_POS_MINUS].end())
        return false;

    // ++ direction less updated
    if (m_nextUsedAreaItr[USED_POS_PLUS] != m_UsedAreaLists[USED_POS_PLUS].end() &&
            (m_nextUsedAreaItr[USED_POS_MINUS] == m_UsedAreaLists[USED_POS_MINUS].end() ||
             m_nextUsedAreaItr[USED_POS_PLUS]->first <= m_nextUsedAreaItr[USED_POS_MINUS]->first))
    {
        angle = m_nextUsedAreaItr[USED_POS_PLUS]->first * SignOf(USED_POS_PLUS);
        ++m_nextUsedAreaItr[USED_POS_PLUS];
    }
    else
    {
        angle = m_nextUsedAreaItr[USED_POS_MINUS]->first * SignOf(USED_POS_MINUS);
        ++m_nextUsedAreaItr[USED_POS_MINUS];
    }

    return true;
}
