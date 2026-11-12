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

#ifndef MANGOS_NGRID_H
#define MANGOS_NGRID_H

/** NGrid is nothing more than a wrapper of the Grid with an NxN cells
 * NGrid 是包含 N×N 个 Grid 单元格的二维大网格封装
 */

#include "GameSystem/Grid.h"
#include "GameSystem/GridReference.h"
#include "Util/Timer.h"

#include <cassert>

/// 网格元信息：维护卸载超时计时器与卸载锁定状态
class GridInfo
{
    public:

        GridInfo()
            : i_timer(0), i_unloadActiveLockCount(0), i_unloadExplicitLock(false)
        {
        }

        GridInfo(time_t expiry, bool unload = true)
            : i_timer(expiry), i_unloadActiveLockCount(0), i_unloadExplicitLock(!unload)
        {
        }

        const TimeTracker& getTimeTracker() const { return i_timer; }

        /// 判断当前是否被锁定而禁止卸载
        bool getUnloadLock() const
        {
            return i_unloadActiveLockCount || i_unloadExplicitLock;
        }

        void setUnloadExplicitLock(bool on) { i_unloadExplicitLock = on; }
        void incUnloadActiveLock() { ++i_unloadActiveLockCount; }
        void decUnloadActiveLock() { if (i_unloadActiveLockCount) --i_unloadActiveLockCount; }

        void setTimer(const TimeTracker& pTimer) { i_timer = pTimer; }
        void ResetTimeTracker(time_t interval) { i_timer.Reset(interval); }
        void UpdateTimeTracker(time_t diff) { i_timer.Update(diff); }

    private:

        TimeTracker i_timer;
        uint16 i_unloadActiveLockCount : 16;                // lock from active object spawn points (prevent clone loading) | 来自活跃对象刷新点的卸载锁计数（防止重复加载）
        bool i_unloadExplicitLock      : 1;                 // explicit manual lock or config setting | 显式手动锁定或配置禁卸标志
};

/// 网格生命周期状态枚举
typedef enum
{
    GRID_STATE_INVALID = 0,
    GRID_STATE_ACTIVE = 1,
    GRID_STATE_IDLE = 2,
    GRID_STATE_REMOVAL = 3,
    MAX_GRID_STATE = 4
} grid_state_t;

/// 包含 N×N 单元格的大网格模板类
template
<
    uint32 N,
    class ACTIVE_OBJECT,
    class WORLD_OBJECT_TYPES,
    class GRID_OBJECT_TYPES
    >
class NGrid
{
    public:

        typedef Grid<ACTIVE_OBJECT, WORLD_OBJECT_TYPES, GRID_OBJECT_TYPES> GridType;

        NGrid(uint32 id, uint32 x, uint32 y, time_t expiry, bool unload = true)
            : i_gridId(id), i_x(x), i_y(y), i_cellstate(GRID_STATE_INVALID), i_GridObjectDataLoaded(false)
        {
            i_GridInfo = GridInfo(expiry, unload);
        }

        /// 按单元格坐标 (x, y) 只读访问子 Grid
        const GridType& operator()(uint32 x, uint32 y) const
        {
            assert(x < N);
            assert(y < N);
            return i_cells[x][y];
        }

        /// 按单元格坐标 (x, y) 可写访问子 Grid
        GridType& operator()(uint32 x, uint32 y)
        {
            assert(x < N);
            assert(y < N);
            return i_cells[x][y];
        }

        const uint32& GetGridId() const { return i_gridId; }
        grid_state_t GetGridState() const { return i_cellstate; }
        void SetGridState(grid_state_t s) { i_cellstate = s; }
        uint32 getX() const { return i_x; }
        uint32 getY() const { return i_y; }

        /// 将当前 NGrid 链接到地图的网格引用管理器中
        void link(GridRefManager<NGrid<N, ACTIVE_OBJECT, WORLD_OBJECT_TYPES, GRID_OBJECT_TYPES> >* pTo)
        {
            i_Reference.link(pTo, this);
        }

        bool isGridObjectDataLoaded() const { return i_GridObjectDataLoaded; }
        void setGridObjectDataLoaded(bool pLoaded) { i_GridObjectDataLoaded = pLoaded; }

        GridInfo* getGridInfoRef() { return &i_GridInfo; }
        const TimeTracker& getTimeTracker() const { return i_GridInfo.getTimeTracker(); }
        bool getUnloadLock() const { return i_GridInfo.getUnloadLock(); }
        void setUnloadExplicitLock(bool on) { i_GridInfo.setUnloadExplicitLock(on); }
        void incUnloadActiveLock() { i_GridInfo.incUnloadActiveLock(); }
        void decUnloadActiveLock() { i_GridInfo.decUnloadActiveLock(); }
        void ResetTimeTracker(time_t interval) { i_GridInfo.ResetTimeTracker(interval); }
        void UpdateTimeTracker(time_t diff) { i_GridInfo.UpdateTimeTracker(diff); }

        /// 向指定单元格添加世界对象
        template<class SPECIFIC_OBJECT>
        void AddWorldObject(const uint32 x, const uint32 y, SPECIFIC_OBJECT* obj)
        {
            getGridType(x, y).AddWorldObject(obj);
        }

        /// 从指定单元格移除世界对象
        template<class SPECIFIC_OBJECT>
        void RemoveWorldObject(const uint32 x, const uint32 y, SPECIFIC_OBJECT* obj)
        {
            getGridType(x, y).RemoveWorldObject(obj);
        }

        /// 遍历整个 N×N 网格的所有单元格并应用访问者
        template<class T, class TT>
        void Visit(TypeContainerVisitor<T, TypeMapContainer<TT> >& visitor)
        {
            for (uint32 x = 0; x < N; ++x)
                for (uint32 y = 0; y < N; ++y)
                    i_cells[x][y].Visit(visitor);
        }

        /// 对单个指定单元格应用访问者
        template<class T, class TT>
        void Visit(const uint32& x, const uint32& y, TypeContainerVisitor<T, TypeMapContainer<TT> >& visitor)
        {
            getGridType(x, y).Visit(visitor);
        }

        /// 统计整个 NGrid 中所有活跃对象数量
        uint32 ActiveObjectsInGrid() const
        {
            uint32 count = 0;
            for (uint32 x = 0; x < N; ++x)
                for (uint32 y = 0; y < N; ++y)
                    count += i_cells[x][y].ActiveObjectsInGrid();

            return count;
        }

        /// 向指定单元格添加网格对象
        template<class SPECIFIC_OBJECT>
        bool AddGridObject(const uint32 x, const uint32 y, SPECIFIC_OBJECT* obj)
        {
            return getGridType(x, y).AddGridObject(obj);
        }

        /// 从指定单元格移除网格对象
        template<class SPECIFIC_OBJECT>
        bool RemoveGridObject(const uint32 x, const uint32 y, SPECIFIC_OBJECT* obj)
        {
            return getGridType(x, y).RemoveGridObject(obj);
        }

    private:

        GridType& getGridType(const uint32& x, const uint32& y)
        {
            assert(x < N);
            assert(y < N);
            return i_cells[x][y];
        }

        uint32 i_gridId;
        GridInfo i_GridInfo;
        GridReference<NGrid<N, ACTIVE_OBJECT, WORLD_OBJECT_TYPES, GRID_OBJECT_TYPES> > i_Reference;
        uint32 i_x;
        uint32 i_y;
        grid_state_t i_cellstate;
        GridType i_cells[N][N];
        bool i_GridObjectDataLoaded;
};

#endif
