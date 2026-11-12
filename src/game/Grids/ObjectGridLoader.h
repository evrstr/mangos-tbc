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

#ifndef MANGOS_OBJECTGRIDLOADER_H
#define MANGOS_OBJECTGRIDLOADER_H

#include "Common.h"
#include "Platform/Define.h"
#include "GameSystem/GridLoader.h"
#include "Maps/GridDefines.h"
#include "Grids/Cell.h"

class ObjectWorldLoader;

/// 网格对象加载器：从数据库与持久化状态向 NGrid 内的各个 Cell 加载生物、游戏对象和尸体
class ObjectGridLoader
{
        friend class ObjectWorldLoader;

    public:
        ObjectGridLoader(NGridType& grid, Map* map, const Cell& cell)
            : i_cell(cell), i_grid(grid), i_map(map), i_gameObjects(0), i_creatures(0), i_corpses(0)
        {}

        /// 加载单个 Cell 内的静态网格对象与世界尸体对象
        void Load(GridType& grid);
        /// 访问并加载单元格内的 GameObject 列表
        void Visit(GameObjectMapType& m);
        /// 访问并加载单元格内的 Creature 列表
        void Visit(CreatureMapType& m);
        void Visit(CorpseMapType&) {}

        void Visit(DynamicObjectMapType&) { }

        /// 批量加载整个 NGrid（MAX_NUMBER_OF_CELLS x MAX_NUMBER_OF_CELLS 个 Cell）
        void LoadN(void);

    private:
        Cell i_cell;
        NGridType& i_grid;
        Map* i_map;
        uint32 i_gameObjects;                               ///< 本次加载的游戏对象计数
        uint32 i_creatures;                                 ///< 本次加载的生物计数
        uint32 i_corpses;                                   ///< 本次加载的尸体计数
};

/// 网格对象卸载器：卸载前迁徙跨网格重生点生物并清理网格内的对象
class ObjectGridUnloader
{
    public:
        ObjectGridUnloader(NGridType& grid) : i_grid(grid) {}

        /// 将重生点位于其他网格的生物迁回其重生点所在网格
        void MoveToRespawnN();
        /// 逐个 Cell 卸载整个 NGrid 内的静态网格对象
        void UnloadN()
        {
            for (unsigned int x = 0; x < MAX_NUMBER_OF_CELLS; ++x)
            {
                for (unsigned int y = 0; y < MAX_NUMBER_OF_CELLS; ++y)
                {
                    GridLoader<Player, AllWorldObjectTypes, AllGridObjectTypes> loader;
                    loader.Unload(i_grid(x, y), *this);
                }
            }
        }

        /// 卸载单个 Cell 内的网格对象
        void Unload(GridType& grid);
        /// 遍历并从地图移除容器中的所有对象
        template<class T> void Visit(GridRefManager<T>& m);
    private:
        NGridType& i_grid;
};

/// 网格对象停止器：在网格停用前停止生物战斗并清理其释放的动态对象
class ObjectGridStoper
{
    public:
        ObjectGridStoper(NGridType& grid) : i_grid(grid) {}

        /// 遍历整个 NGrid 内的全部 Cell 停止活跃状态
        void StopN()
        {
            for (unsigned int x = 0; x < MAX_NUMBER_OF_CELLS; ++x)
            {
                for (unsigned int y = 0; y < MAX_NUMBER_OF_CELLS; ++y)
                {
                    GridLoader<Player, AllWorldObjectTypes, AllGridObjectTypes> loader;
                    loader.Stop(i_grid(x, y), *this);
                }
            }
        }

        /// 停止单个 Cell 内网格对象的战斗与动态法术对象
        void Stop(GridType& grid);
        /// 停止容器中所有生物的战斗并移除其关联的 DynamicObject
        void Visit(CreatureMapType& m);

        template<class NONACTIVE> void Visit(GridRefManager<NONACTIVE>&) {}
    private:
        NGridType& i_grid;
};

typedef GridLoader<Player, AllWorldObjectTypes, AllGridObjectTypes> GridLoaderType;

#endif
