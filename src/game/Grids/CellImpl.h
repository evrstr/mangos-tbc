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

#ifndef MANGOS_CELLIMPL_H
#define MANGOS_CELLIMPL_H

#include "Common.h"
#include "Grids/Cell.h"
#include "Maps/Map.h"
#include <cmath>

inline Cell::Cell(CellPair const& p)
{
    data.Part.grid_x = p.x_coord / MAX_NUMBER_OF_CELLS;
    data.Part.grid_y = p.y_coord / MAX_NUMBER_OF_CELLS;
    data.Part.cell_x = p.x_coord % MAX_NUMBER_OF_CELLS;
    data.Part.cell_y = p.y_coord % MAX_NUMBER_OF_CELLS;
    data.Part.nocreate = 0;
    data.Part.reserved = 0;
}

inline CellArea Cell::CalculateCellArea(float x, float y, float radius)
{
    if (radius <= 0.0f)
    {
        CellPair center = MaNGOS::ComputeCellPair(x, y).normalize();
        return CellArea(center, center);
    }

    return CellArea
           (
               MaNGOS::ComputeCellPair(x - radius, y - radius).normalize(),
               MaNGOS::ComputeCellPair(x + radius, y + radius).normalize()
           );
}

template<class T, class CONTAINER>
inline void
Cell::Visit(const CellPair& standing_cell, TypeContainerVisitor<T, CONTAINER>& visitor, Map& m, const WorldObject& obj, float radius) const
{
    Cell::Visit(standing_cell, visitor, m, obj.GetPositionX(), obj.GetPositionY(), radius + obj.GetObjectBoundingRadius());
}


template<class T, class CONTAINER>
inline void
Cell::Visit(const CellPair& standing_cell, TypeContainerVisitor<T, CONTAINER>& visitor, Map& m, float x, float y, float radius) const
{
    if (standing_cell.x_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP || standing_cell.y_coord >= TOTAL_NUMBER_OF_CELLS_PER_MAP)
        return;

    // no jokes here... Actually placing ASSERT() here was good idea, but
    // we had some problems with DynamicObjects, which pass radius = 0.0f (DB issue?)
    // maybe it is better to just return when radius <= 0.0f?
    // 注：此前考虑过在此加 ASSERT()，但部分 DynamicObject 会传入 radius = 0.0f，因此当 radius <= 0.0f 时直接访问当前单元格并返回
    if (radius <= 0.0f)
    {
        m.Visit(*this, visitor);
        return;
    }
    // lets limit the upper value for search radius | 限制搜索半径上限不超过最大可视距离
    if (radius > MAX_VISIBILITY_DISTANCE)
        radius = MAX_VISIBILITY_DISTANCE;

    // lets calculate object coord offsets from cell borders. | 计算搜索半径覆盖的单元格边界范围
    CellArea area = Cell::CalculateCellArea(x, y, radius);
    // if radius fits inside standing cell | 若搜索范围完全落在当前站立单元格内，则仅访问当前单元格
    if (!area)
    {
        m.Visit(*this, visitor);
        return;
    }

    CellPair& begin_cell = area.low_bound;
    CellPair& end_cell = area.high_bound;
    // visit all cells, found in CalculateCellArea()
    // if radius is known to reach cell area more than 4x4 then we should call optimized VisitCircle
    // currently this technique works with MAX_NUMBER_OF_CELLS 16 and higher, with lower values
    // there are nothing to optimize because SIZE_OF_GRID_CELL is too big...
    // 遍历 CalculateCellArea() 计算出的单元格范围；当跨越超过 4x4 个单元格时改用八边形 VisitCircle 优化
    if (((end_cell.x_coord - begin_cell.x_coord) > 4) && ((end_cell.y_coord - begin_cell.y_coord) > 4))
    {
        VisitCircle(visitor, m, begin_cell, end_cell);
        return;
    }

    // ALWAYS visit standing cell first!!! Since we deal with small radiuses
    // it is very essential to call visitor for standing cell firstly...
    // 必须最先访问当前站立单元格（小半径搜索时优先命中本地对象至关重要）
    m.Visit(*this, visitor);

    // loop the cell range | 遍历其余矩形范围单元格
    for (uint32 i = begin_cell.x_coord; i <= end_cell.x_coord; ++i)
    {
        for (uint32 j = begin_cell.y_coord; j <= end_cell.y_coord; ++j)
        {
            CellPair cell_pair(i, j);
            // lets skip standing cell since we already visited it | 跳过已访问过的站立单元格
            if (cell_pair != standing_cell)
            {
                Cell r_zone(cell_pair);
                r_zone.data.Part.nocreate = data.Part.nocreate;
                m.Visit(r_zone, visitor);
            }
        }
    }
}

template<class T, class CONTAINER>
inline void
Cell::VisitCircle(TypeContainerVisitor<T, CONTAINER>& visitor, Map& m, const CellPair& begin_cell, const CellPair& end_cell) const
{
    // here is an algorithm for 'filling' circum-squared octagon | 使用外切正方形内的八边形填充算法近似圆形遍历
    uint32 x_shift = (uint32)ceilf((end_cell.x_coord - begin_cell.x_coord) * 0.3f - 0.5f);
    // lets calculate x_start/x_end coords for central strip... | 计算中央矩形条带的 X 起止坐标
    const uint32 x_start = begin_cell.x_coord + x_shift;
    const uint32 x_end = end_cell.x_coord - x_shift;

    // visit central strip with constant width... | 先访问等宽的中央竖向条带
    for (uint32 x = x_start; x <= x_end; ++x)
    {
        for (uint32 y = begin_cell.y_coord; y <= end_cell.y_coord; ++y)
        {
            CellPair cell_pair(x, y);
            Cell r_zone(cell_pair);
            r_zone.data.Part.nocreate = data.Part.nocreate;
            m.Visit(r_zone, visitor);
        }
    }

    // if x_shift == 0 then we have too small cell area, which were already
    // visited at previous step, so just return from procedure...
    // 若 x_shift == 0 说明区域较小，上一步已全部覆盖，直接返回
    if (x_shift == 0)
        return;

    uint32 y_start = end_cell.y_coord;
    uint32 y_end = begin_cell.y_coord;
    // now we are visiting borders of an octagon... | 随后对称访问左右两侧逐渐收窄的梯形边带
    for (uint32 step = 1; step <= (x_start - begin_cell.x_coord); ++step)
    {
        // each step reduces strip height by 2 cells... | 每向外一步，垂直高度上下各缩减 1 个单元格（共 2 格）
        y_end += 1;
        y_start -= 1;
        for (uint32 y = y_start; y >= y_end; --y)
        {
            // we visit cells symmetrically from both sides, heading from center to sides and from up to bottom
            // e.g. filling 2 trapezoids after filling central cell strip...
            // 从中心向两侧对称填充左右两个梯形区域
            CellPair cell_pair_left(x_start - step, y);
            Cell r_zone_left(cell_pair_left);
            r_zone_left.data.Part.nocreate = data.Part.nocreate;
            m.Visit(r_zone_left, visitor);

            // right trapezoid cell visit | 访问右侧梯形单元格
            CellPair cell_pair_right(x_end + step, y);
            Cell r_zone_right(cell_pair_right);
            r_zone_right.data.Part.nocreate = data.Part.nocreate;
            m.Visit(r_zone_right, visitor);
        }
    }
}

template<class T>
inline void Cell::VisitGridObjects(const WorldObject* center_obj, T& visitor, float radius, bool dont_load)
{
    CellPair p(MaNGOS::ComputeCellPair(center_obj->GetPositionX(), center_obj->GetPositionY()));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();
    TypeContainerVisitor<T, GridTypeMapContainer > gnotifier(visitor);
    cell.Visit(p, gnotifier, *center_obj->GetMap(), *center_obj, radius);
}

template<class T>
inline void Cell::VisitWorldObjects(const WorldObject* center_obj, T& visitor, float radius, bool dont_load)
{
    CellPair p(MaNGOS::ComputeCellPair(center_obj->GetPositionX(), center_obj->GetPositionY()));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();
    TypeContainerVisitor<T, WorldTypeMapContainer > gnotifier(visitor);
    cell.Visit(p, gnotifier, *center_obj->GetMap(), *center_obj, radius);
}

template<class T>
inline void Cell::VisitAllObjects(const WorldObject* center_obj, T& visitor, float radius, bool dont_load)
{
    // The assert below is required for https://github.com/cmangos/mangos-tbc/pull/344 and issue https://github.com/cmangos/issues/issues/2044
    // A nullptr center_obj was passed as parameter leading to a crash. ToDo investigate why a nullptr came here in the first place.
    // 下方断言用于排查传入 nullptr 的 center_obj 导致的崩溃问题（见 issue #2044）
    MANGOS_ASSERT(center_obj != nullptr);
    CellPair p(MaNGOS::ComputeCellPair(center_obj->GetPositionX(), center_obj->GetPositionY()));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();
    TypeContainerVisitor<T, GridTypeMapContainer > gnotifier(visitor);
    TypeContainerVisitor<T, WorldTypeMapContainer > wnotifier(visitor);
    cell.Visit(p, gnotifier, *center_obj->GetMap(), *center_obj, radius);
    cell.Visit(p, wnotifier, *center_obj->GetMap(), *center_obj, radius);
}

template<class T>
inline void Cell::VisitGridObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load)
{
    CellPair p(MaNGOS::ComputeCellPair(x, y));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();
    TypeContainerVisitor<T, GridTypeMapContainer > gnotifier(visitor);
    cell.Visit(p, gnotifier, *map, x, y, radius);
}

template<class T>
inline void Cell::VisitWorldObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load)
{
    CellPair p(MaNGOS::ComputeCellPair(x, y));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();
    TypeContainerVisitor<T, WorldTypeMapContainer > gnotifier(visitor);
    cell.Visit(p, gnotifier, *map, x, y, radius);
}

template<class T>
inline void Cell::VisitAllObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load)
{
    CellPair p(MaNGOS::ComputeCellPair(x, y));
    Cell cell(p);
    if (dont_load)
        cell.SetNoCreate();
    TypeContainerVisitor<T, GridTypeMapContainer > gnotifier(visitor);
    TypeContainerVisitor<T, WorldTypeMapContainer > wnotifier(visitor);
    cell.Visit(p, gnotifier, *map, x, y, radius);
    cell.Visit(p, wnotifier, *map, x, y, radius);
}

#endif
