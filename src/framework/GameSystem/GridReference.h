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

#ifndef _GRIDREFERENCE_H
#define _GRIDREFERENCE_H

#include "Utilities/LinkedReference/Reference.h"

template<class OBJECT> class GridRefManager;

/// 网格引用节点：将单个 OBJECT 链接到对应的 GridRefManager 链表并维护计数
template<class OBJECT>
class GridReference : public Reference<GridRefManager<OBJECT>, OBJECT>
{
    protected:

        void targetObjectBuildLink() override
        {
            // called from link() | 由 link() 调用，将自身插入目标链表头部并递增计数
            this->getTarget()->insertFirst(this);
            this->getTarget()->incSize();
        }

        void targetObjectDestroyLink() override
        {
            // called from unlink() | 由 unlink() 调用，若引用有效则递减目标计数
            if (this->isValid())
                this->getTarget()->decSize();
        }

        void sourceObjectDestroyLink() override
        {
            // called from invalidate() | 由 invalidate() 调用，递减目标计数
            this->getTarget()->decSize();
        }

    public:

        GridReference()
            : Reference<GridRefManager<OBJECT>, OBJECT>()
        {
        }

        ~GridReference()
        {
            this->unlink();
        }

        /// 获取链表中的下一个 GridReference 节点
        GridReference* next()
        {
            return (GridReference*)Reference<GridRefManager<OBJECT>, OBJECT>::next();
        }
};

#endif
