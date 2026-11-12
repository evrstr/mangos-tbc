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

#ifndef __OBJECT_VISIBILITY_H
#define __OBJECT_VISIBILITY_H

#include "Common.h"

class WorldObject;
class Unit;

// 可视距离等级枚举
enum class VisibilityDistanceType : uint32
{
    Normal = 0,                                             // 普通距离（100 码）
    Tiny = 1,                                               // 微型距离（25 码）
    Small = 2,                                              // 小型距离（50 码）
    Large = 3,                                              // 大型距离（200 码）
    Gigantic = 4,                                           // 超大型距离（400 码）
    Infinite = 5,                                           // 无限/全图网格上限距离（533 码）

    Max
};

// 潜行类型枚举
enum StealthType : uint32
{
    STEALTH_UNIT = 0,                                       // 单位潜行
    STEALTH_TRAP = 1,                                       // 陷阱潜行
    STEALTH_TYPE_MAX,
};

// 隐身类型枚举
enum InvisibilityType : uint32
{
    INVISIBILITY_TRAP = 3,                                  // 陷阱隐身
    INVISIBILITY_DRUNK = 6,                                 // 醉酒幻象隐身
    INVISIBILITY_TYPE_MAX = 32,
};

// 对象可见性数据组件（管理可视距离覆盖、隐身与潜行强度及反侦测状态）
class VisibilityData
{
    public:
        VisibilityData(WorldObject* owner);

        // visibility (可视距离控制)
        bool IsVisibilityOverridden() const { return m_visibilityDistanceOverride != 0.f; }
        void SetVisibilityDistanceOverride(VisibilityDistanceType type);
        bool IsInfiniteVisibility() const;
        bool IsLargeVisibility() const;

        float GetVisibilityDistance() const;
        float GetVisibilityDistanceFor(WorldObject* obj) const;
        // invisibility (隐身与反隐身侦测)
        bool CanDetectInvisibilityOf(WorldObject const* u) const;
        uint32 GetInvisibilityDetectMask() const;
        void SetInvisibilityDetectMask(uint32 index, bool apply);
        uint32 GetInvisibilityMask() const;
        void SetInvisibilityMask(uint32 index, bool apply);
        void SetInvisibilityValue(uint32 index, int32 value) { m_invisibilityValues[index] = value; }
        void AddInvisibilityValue(uint32 index, int32 value) { m_invisibilityValues[index] += value; }
        void AddInvisibilityDetectValue(uint32 index, int32 value) { m_invisibilityDetectValues[index] += value; }
        int32 GetInvisibilityValue(uint32 index) const;
        int32 GetInvisibilityDetectValue(uint32 index) const;
        // stealth (潜行与反潜行侦测)
        uint32 GetStealthMask() const { return m_stealthMask; }
        void SetStealthMask(uint32 index, bool apply);
        void AddStealthStrength(StealthType type, uint32 value) { m_stealthStrength[type] += value; }
        void AddStealthDetectionStrength(StealthType type, uint32 value) { m_stealthDetectStrength[type] += value; }
        uint32 GetStealthStrength(StealthType type) const { return m_stealthStrength[type]; }
        uint32 GetStealthDetectionStrength(StealthType type) const { return m_stealthDetectStrength[type]; }

        float GetStealthVisibilityDistance(Unit const* target, bool alert = false) const;
    private:
        // visibility (可视距离字段)
        float m_visibilityDistanceOverride;                 // 覆盖后的可视距离（0 表示未覆盖）
        // invisibility (隐身字段)
        uint32 m_invisibilityMask;                          // 当前开启的隐身类型位掩码
        uint32 m_detectInvisibilityMask; // is inherited from controller in PC case (反隐身侦测掩码；受玩家控制时继承自控制者)
        int32 m_invisibilityValues[INVISIBILITY_TYPE_MAX];  // 各隐身类型等级值
        int32 m_invisibilityDetectValues[INVISIBILITY_TYPE_MAX]; // 各隐身类型侦测等级值
        // stealth (潜行字段)
        uint32 m_stealthMask;                               // 当前开启的潜行类型位掩码
        uint32 m_stealthStrength[STEALTH_TYPE_MAX];         // 各潜行类型强度值
        uint32 m_stealthDetectStrength[STEALTH_TYPE_MAX];   // 各潜行类型侦测强度值

        WorldObject* m_owner;                               // 所属世界对象指针
};

#endif