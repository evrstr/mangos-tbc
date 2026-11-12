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

#ifndef MANGOS_CELL_H
#define MANGOS_CELL_H

#include "GameSystem/TypeContainerVisitor.h"
#include "Maps/GridDefines.h"

class Map;
class WorldObject;

/// 单元格查询区域范围，由左下与右上单元格坐标界定
struct CellArea
{
    CellArea() {}
    CellArea(const CellPair& low, const CellPair& high) : low_bound(low), high_bound(high) {}

    bool operator!() const { return low_bound == high_bound; }

    /// 根据当前区域调整起止单元格边界
    void ResizeBorders(CellPair& begin_cell, CellPair& end_cell) const
    {
        begin_cell = low_bound;
        end_cell = high_bound;
    }

    CellPair low_bound;                                     ///< 区域最小边界单元格坐标
    CellPair high_bound;                                    ///< 区域最大边界单元格坐标
};

/// 地图逻辑单元格句柄，将 Grid 索引与 Cell 索引压缩编码为 32 位位域
struct Cell
{
        Cell() { data.All = 0; }
        Cell(const Cell& cell) { data.All = cell.data.All; }
        explicit Cell(CellPair const& p);

        /// 计算全局单元格坐标 (x, y)
        void Compute(uint32& x, uint32& y) const
        {
            x = data.Part.grid_x * MAX_NUMBER_OF_CELLS + data.Part.cell_x;
            y = data.Part.grid_y * MAX_NUMBER_OF_CELLS + data.Part.cell_y;
        }

        /// 判断当前单元格与目标单元格在局部 Cell 坐标上是否不同
        bool DiffCell(const Cell& cell) const
        {
            return (data.Part.cell_x != cell.data.Part.cell_x ||
                    data.Part.cell_y != cell.data.Part.cell_y);
        }

        /// 判断当前单元格与目标单元格所属的 Grid 网格是否不同
        bool DiffGrid(const Cell& cell) const
        {
            return (data.Part.grid_x != cell.data.Part.grid_x ||
                    data.Part.grid_y != cell.data.Part.grid_y);
        }

        uint32 CellX() const { return data.Part.cell_x; }
        uint32 CellY() const { return data.Part.cell_y; }
        uint32 GridX() const { return data.Part.grid_x; }
        uint32 GridY() const { return data.Part.grid_y; }
        bool NoCreate() const { return data.Part.nocreate; }
        /// 设置只读访问标记（遍历时不自动创建/加载未激活的 Grid）
        void SetNoCreate() { data.Part.nocreate = 1; }

        GridPair gridPair() const { return GridPair(GridX(), GridY()); }

        /// 转换为全局二维单元格坐标对
        CellPair cellPair() const
        {
            return CellPair(
                       data.Part.grid_x * MAX_NUMBER_OF_CELLS + data.Part.cell_x,
                       data.Part.grid_y * MAX_NUMBER_OF_CELLS + data.Part.cell_y);
        }

        Cell& operator=(const Cell& cell)
        {
            data.All = cell.data.All;
            return *this;
        }

        bool operator==(const Cell& cell) const { return (data.All == cell.data.All); }
        bool operator!=(const Cell& cell) const { return !operator==(cell); }
        union
        {
            struct
            {
                unsigned grid_x : 6;                        ///< 所属 Grid 的 X 索引
                unsigned grid_y : 6;                        ///< 所属 Grid 的 Y 索引
                unsigned cell_x : 6;                        ///< Grid 内局部 Cell 的 X 索引
                unsigned cell_y : 6;                        ///< Grid 内局部 Cell 的 Y 索引
                unsigned nocreate : 1;                      ///< 访问时是否禁止隐式创建网格
                unsigned reserved : 7;                      ///< 保留位
            } Part;
            uint32 All;                                     ///< 完整 32 位压缩表示
        } data;

        /// 以世界坐标 (x, y) 及半径遍历周围单元格
        template<class T, class CONTAINER> void Visit(const CellPair& cellPair, TypeContainerVisitor<T, CONTAINER>& visitor, Map& m, float x, float y, float radius) const;
        /// 以世界对象为圆心并叠加对象包围半径遍历周围单元格
        template<class T, class CONTAINER> void Visit(const CellPair& cellPair, TypeContainerVisitor<T, CONTAINER>& visitor, Map& m, const WorldObject& obj, float radius) const;

        /// 计算给定中心点与搜索半径覆盖的单元格矩形区域
        static CellArea CalculateCellArea(float x, float y, float radius);

        /// 遍历指定世界对象周围的网格静态对象（Creature/GameObject 等）
        template<class T> static void VisitGridObjects(const WorldObject* obj, T& visitor, float radius, bool dont_load = true);
        /// 遍历指定世界对象周围的世界动态对象（Player/Corpse/DynamicObject 等）
        template<class T> static void VisitWorldObjects(const WorldObject* obj, T& visitor, float radius, bool dont_load = true);
        /// 遍历指定世界对象周围的所有网格与世界对象
        template<class T> static void VisitAllObjects(const WorldObject* obj, T& visitor, float radius, bool dont_load = true);

        /// 遍历指定地图坐标周围的网格静态对象
        template<class T> static void VisitGridObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load = true);
        /// 遍历指定地图坐标周围的世界动态对象
        template<class T> static void VisitWorldObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load = true);
        /// 遍历指定地图坐标周围的所有网格与世界对象
        template<class T> static void VisitAllObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load = true);

    private:
        /// 大范围搜索时按八边形近似圆形遍历单元格，减少角落多余格子访问
        template<class T, class CONTAINER> void VisitCircle(TypeContainerVisitor<T, CONTAINER>&, Map&, const CellPair&, const CellPair&) const;
};

#endif
