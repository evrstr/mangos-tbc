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

#ifndef MANGOS_GRIDSTATES_H
#define MANGOS_GRIDSTATES_H

#include "Maps/Map.h"

/// 地图网格（NGrid）生命周期状态基类
class GridState
{
    public:

        /// 状态更新回调：推进计时并执行状态迁移或网格卸载
        virtual void Update(Map&, NGridType&, GridInfo&, const uint32& x, const uint32& y, const uint32& t_diff) const = 0;
        virtual ~GridState() {}
};

/// 无效状态：未初始化或占位状态，不执行任何更新
class InvalidState : public GridState
{
    public:

        void Update(Map&, NGridType&, GridInfo&, const uint32& x, const uint32& y, const uint32& t_diff) const override;
        virtual ~InvalidState() {}
};

/// 激活状态：网格内或周边存在活跃对象/玩家，定期检查是否可转入空闲态
class ActiveState : public GridState
{
    public:

        void Update(Map&, NGridType&, GridInfo&, const uint32& x, const uint32& y, const uint32& t_diff) const override;
        virtual ~ActiveState() {}
};

/// 空闲状态：重置网格过期计时器并立即流转至待卸载状态
class IdleState : public GridState
{
    public:

        void Update(Map&, NGridType&, GridInfo&, const uint32& x, const uint32& y, const uint32& t_diff) const override;
        virtual ~IdleState() {}
};

/// 待移除状态：等待过期计时结束且未加锁后执行网格卸载
class RemovalState : public GridState
{
    public:

        void Update(Map&, NGridType&, GridInfo&, const uint32& x, const uint32& y, const uint32& t_diff) const override;
        virtual ~RemovalState() {}
};

#endif
