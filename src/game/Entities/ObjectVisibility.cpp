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

#include "ObjectVisibility.h"

#include "Util/Util.h"
#include "Entities/Player.h"
#include "Entities/GameObject.h"

// 各可视距离等级对应的实际距离表（码）
constexpr float VisibilityDistances[AsUnderlyingType(VisibilityDistanceType::Max)] =
{
    DEFAULT_VISIBILITY_DISTANCE,
    VISIBILITY_DISTANCE_TINY,
    VISIBILITY_DISTANCE_SMALL,
    VISIBILITY_DISTANCE_LARGE,
    VISIBILITY_DISTANCE_GIGANTIC,
    MAX_VISIBILITY_DISTANCE
};

// 初始化可见性数据并清零隐身/潜行强度数组
VisibilityData::VisibilityData(WorldObject* owner) : m_visibilityDistanceOverride(0.f), m_invisibilityMask(0), m_detectInvisibilityMask(0), m_owner(owner)
{
    memset(m_invisibilityValues, 0, sizeof(m_invisibilityValues));
    memset(m_invisibilityDetectValues, 0, sizeof(m_invisibilityDetectValues));

    memset(m_stealthStrength, 0, sizeof(m_stealthStrength));
    memset(m_stealthDetectStrength, 0, sizeof(m_stealthDetectStrength));
}

// 设置非玩家对象的可视距离覆盖值，并在对象已入世界时通知地图更新
void VisibilityData::SetVisibilityDistanceOverride(VisibilityDistanceType type)
{
    MANGOS_ASSERT(type < VisibilityDistanceType::Max);
    if (m_owner->IsPlayer())
        return;

    float oldVisiblity = m_visibilityDistanceOverride;
    m_visibilityDistanceOverride = VisibilityDistances[AsUnderlyingType(type)];
    if (m_owner->IsInWorld() && m_visibilityDistanceOverride != oldVisiblity)
        m_owner->GetMap()->VisiblityDistanceChanged(m_owner, oldVisiblity, type);
}

// 判断是否启用了超大/无限级可视距离
bool VisibilityData::IsInfiniteVisibility() const
{
    return m_visibilityDistanceOverride > VISIBILITY_DISTANCE_GIGANTIC;
}

// 判断是否启用了大型及以上可视距离
bool VisibilityData::IsLargeVisibility() const
{
    return m_visibilityDistanceOverride >= VISIBILITY_DISTANCE_LARGE;
}

// 获取当前对象的实际可视距离（若未覆盖则回退至地图默认距离）
float VisibilityData::GetVisibilityDistance() const
{
    if (IsVisibilityOverridden())
        return m_visibilityDistanceOverride;
    else
        return m_owner->GetMap()->GetVisibilityDistance();
}

// 获取观察指定目标对象时应使用的可视距离
float VisibilityData::GetVisibilityDistanceFor(WorldObject* obj) const
{
    if (IsVisibilityOverridden() && obj->GetTypeId() == TYPEID_PLAYER)
        return m_visibilityDistanceOverride;
    else
        return obj->GetVisibilityData().GetVisibilityDistance();
}

// 判断当前对象是否能侦测到目标的隐身状态
bool VisibilityData::CanDetectInvisibilityOf(WorldObject const* target) const
{
    if (uint32 mask = (GetInvisibilityDetectMask() & target->GetVisibilityData().GetInvisibilityMask()))
    {
        for (int32 i = 0; i < INVISIBILITY_TYPE_MAX; ++i)
        {
            if (((1 << i) & mask) == 0)
                continue;

            // find invisibility level (获取隐身强度等级)
            int32 invLevel = GetInvisibilityValue(i);

            // find invisibility detect level - this is taken from controlling player or self (获取反隐身侦测等级——取自控制者玩家或自身)
            int32 detectLevel = GetInvisibilityDetectValue(i);

            if (invLevel <= detectLevel)
                return true;
        }
    }

    return false;
}

// 获取反隐身侦测掩码（被玩家控制时继承控制者的侦测掩码）
uint32 VisibilityData::GetInvisibilityDetectMask() const
{
    if (m_owner->isType(TYPEMASK_UNIT))
    {
        Unit* owner = static_cast<Unit*>(m_owner);
        if (owner->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
        {
            Player const* controller = owner->GetControllingPlayer();
            if (controller)
                return controller->GetVisibilityData().m_detectInvisibilityMask; // directly access value without bypass (直接读取控制者原始值，避免递归绕过)
        }
    }
    return m_detectInvisibilityMask;
}

// 设置或清除指定类型的反隐身侦测标记
void VisibilityData::SetInvisibilityDetectMask(uint32 index, bool apply)
{
    if (apply)
        m_detectInvisibilityMask |= (1 << index);
    else
        m_detectInvisibilityMask &= ~(1 << index);
}

// 获取当前隐身类型掩码
uint32 VisibilityData::GetInvisibilityMask() const
{
    return m_invisibilityMask;
}

// 设置或清除指定类型的隐身标记
void VisibilityData::SetInvisibilityMask(uint32 index, bool apply)
{
    if (apply)
        m_invisibilityMask |= (1 << index);
    else
        m_invisibilityMask &= ~(1 << index);
}

// 获取指定类型的隐身强度值
int32 VisibilityData::GetInvisibilityValue(uint32 index) const
{
    return m_invisibilityValues[index];
}

// 获取指定类型的反隐身侦测等级（被玩家控制时继承控制者的侦测值）
int32 VisibilityData::GetInvisibilityDetectValue(uint32 index) const
{
    if (m_owner->isType(TYPEMASK_UNIT))
    {
        Unit* owner = static_cast<Unit*>(m_owner);
        if (owner->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED))
        {
            Player const* controller = owner->GetControllingPlayer();
            if (controller)
                return controller->GetVisibilityData().m_invisibilityDetectValues[index]; // directly access value without bypass (直接读取控制者原始值，避免递归绕过)
        }
    }
    return m_invisibilityDetectValues[index];
}

// stealth (潜行掩码设置)
void VisibilityData::SetStealthMask(uint32 index, bool apply)
{
    if (apply)
        m_stealthMask |= (1 << index);
    else
        m_stealthMask &= ~(1 << index);
}

// 计算目标单位能发现本潜行对象的距离（受等级差、潜行强度、反潜行强度及警戒加成影响）
float VisibilityData::GetStealthVisibilityDistance(Unit const* target, bool alert) const
{
    // Starting points (基础侦测点数)
    int32 detectionValue = 30;

    // Level difference: 5 point / level, starting from level 1.
    // There may be spells for this and the starting points too, but
    // not in the DBCs of the client.
    // 等级差修正：从 1 级起每级 5 点；客户端 DBC 中未直接包含对应法术数据
    detectionValue += int32(target->GetLevelForTarget(dynamic_cast<Unit*>(m_owner)) - 1) * 5;

    // Apply modifiers (应用侦测与潜行强度修正)
    detectionValue += target->GetVisibilityData().GetStealthDetectionStrength(STEALTH_UNIT);
    if (m_owner->GetTypeId() == TYPEID_GAMEOBJECT) // trap case (陷阱对象需扣除其拥有者等级对应的潜行加成)
        if (Unit* owner = static_cast<GameObject*>(m_owner)->GetOwner())
            detectionValue -= int32(owner->GetLevelForTarget(dynamic_cast<Unit*>(m_owner)) - 1) * 5;

    detectionValue -= GetStealthStrength(STEALTH_UNIT);

    // Calculate max distance (换算为最大发现距离，每点 0.3 码)
    float visibilityRange = float(detectionValue) * 0.3f;

    // If this unit is an NPC then player detect range doesn't apply (若侦测者是玩家，则受限于玩家最大反潜行侦测距离上限)
    if (target->GetTypeId() == TYPEID_PLAYER && visibilityRange > MAX_PLAYER_STEALTH_DETECT_RANGE)
        visibilityRange = MAX_PLAYER_STEALTH_DETECT_RANGE;

    // When checking for alert state, look 8% further, and then 1.5 yards more than that. (处于警戒检查状态时，探测半径额外增加 8% 再加 1.5 码)
    if (alert)
        visibilityRange += (visibilityRange * 0.08f) + 1.5f;

    return std::max(target->GetCombatReach(), visibilityRange);
}