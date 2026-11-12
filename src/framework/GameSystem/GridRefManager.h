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

#ifndef _GRIDREFMANAGER
#define _GRIDREFMANAGER

#include "Utilities/LinkedReference/RefManager.h"

template<class OBJECT> class GridReference;

/// 网格引用管理器：管理指向同一目标类型（如网格或世界对象列表）的 GridReference 双向链表
template<class OBJECT>
class GridRefManager : public RefManager<GridRefManager<OBJECT>, OBJECT>
{
    public:

        typedef LinkedListHead::Iterator< GridReference<OBJECT> > iterator;

        /// 获取链表中的第一个网格引用
        GridReference<OBJECT>* getFirst()
        {
            return (GridReference<OBJECT>*)RefManager<GridRefManager<OBJECT>, OBJECT>::getFirst();
        }

        /// 获取链表中的最后一个网格引用
        GridReference<OBJECT>* getLast()
        {
            return (GridReference<OBJECT>*)RefManager<GridRefManager<OBJECT>, OBJECT>::getLast();
        }

        iterator begin() { return iterator(getFirst()); }
        iterator end() { return iterator(nullptr); }
        iterator rbegin() { return iterator(getLast()); }
        iterator rend() { return iterator(nullptr); }
};
#endif
