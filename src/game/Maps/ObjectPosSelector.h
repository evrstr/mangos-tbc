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

#ifndef _OBJECT_POS_SELECTOR_H
#define _OBJECT_POS_SELECTOR_H

#include<Common.h>

#include<map>

class WorldObject;

enum UsedAreaSide { USED_POS_PLUS, USED_POS_MINUS };

inline UsedAreaSide operator ~(UsedAreaSide side)
{
    return side == USED_POS_PLUS ? USED_POS_MINUS : USED_POS_PLUS;
}

inline float SignOf(UsedAreaSide side)
{
    return side == USED_POS_PLUS ? 1.0f : -1.0f;
}

// 对象位置选择器：用于在中心点周围指定距离上查找不被已有对象占用的可用角度位置
struct ObjectPosSelector
{
    // 已占用角度区域信息
    struct OccupiedArea
    {
        OccupiedArea(float _angleOffset, WorldObject const* obj) : angleOffset(_angleOffset), occupyingObj(obj) {}
        float angleOffset;
        WorldObject const* occupyingObj;
    };
    // angle pos -> OccupiedArea (角度位置 -> 已占用区域)
    typedef std::multimap<float, OccupiedArea> UsedAreaList;
    typedef UsedAreaList::value_type UsedArea;

    ObjectPosSelector(float x, float y, float dist, float searchedForSize, WorldObject const* searchPosFor);

    void AddUsedArea(WorldObject const* obj, float angle, float dist); // 添加一个已被其他对象占用的角度区域

    bool CheckOriginalAngle() const; // 检查原始朝向（0 偏移角度）是否可用

    void InitializeAngle(); // 初始化左右两侧的角度搜索游标

    bool NextAngle(float& angle); // 获取下一个候选角度（优先空闲角度，否则尝试已占用角度）
    bool NextUsedAngle(float& angle); // 从已占用区域周围寻找退让角度

    bool CheckAngle(UsedArea const& usedArea, UsedAreaSide side, float angle) const; // 检查给定角度在指定侧是否不与占用区域重叠
    void InitializeAngle(UsedAreaSide side); // 初始化指定侧的搜索角度与迭代器
    bool NextSideAngle(UsedAreaSide side, float& angle); // 在指定侧步进查找下一个可用角度

    float m_centerX;
    float m_centerY;
    float m_searcherDist;                                   // distance for searching pos (搜索位置的半径距离)
    float m_searchedForReqHAngle;                           // angle size/2 of searcher object (at dist distance) (搜索对象在目标距离处所需的半角宽度)

    UsedAreaList m_UsedAreaLists[2];                        // list left/right side used angles (with angle size) (左右两侧已占用角度列表及其占用角宽)

    UsedAreaList::const_iterator m_nextUsedAreaItr[2];      // next used used areas for check at left/right side, possible angles selected in range m_smallStepAngle..m_nextUsedAreaItr (左右两侧下一个待检查的已占用区域迭代器)

    float m_stepAngle[2];                                   // current checked angle position at sides (less m_nextUsedArea), positive value (左右两侧当前检查到的角度位置，正值)

    WorldObject const* m_searchPosFor;                      // For whom a position is searched (can be nullptr) (为其搜索位置的目标对象，可为 nullptr)
};
#endif
