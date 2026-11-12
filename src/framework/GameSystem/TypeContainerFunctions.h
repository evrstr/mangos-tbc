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

#ifndef TYPECONTAINER_FUNCTIONS_H
#define TYPECONTAINER_FUNCTIONS_H

/*
 * Here you'll find a list of helper functions to make
 * the TypeContainer usefull.  Without it, its hard
 * to access or mutate the container.
 * 本文件提供一系列模板辅助函数，用于对 TypeContainer / ContainerMapList 进行计数、插入和移除操作。
 */

#include "Platform/Define.h"
#include "Utilities/TypeList.h"
#include <map>

namespace MaNGOS
{
    /* ContainerMapList Helpers | ContainerMapList 辅助函数 */
    // count functions | 计数函数重载集
    template<class SPECIFIC_TYPE>
    size_t Count(const ContainerMapList<SPECIFIC_TYPE>& elements, SPECIFIC_TYPE* /*fake*/)
    {
        return elements._element.getSize();
    }

    template<class SPECIFIC_TYPE>
    size_t Count(const ContainerMapList<TypeNull>& /*elements*/, SPECIFIC_TYPE* /*fake*/)
    {
        return 0;
    }

    template<class SPECIFIC_TYPE, class T>
    size_t Count(const ContainerMapList<T>& /*elements*/, SPECIFIC_TYPE* /*fake*/)
    {
        return 0;
    }

    template<class SPECIFIC_TYPE, class T>
    size_t Count(const ContainerMapList<TypeList<SPECIFIC_TYPE, T> >& elements, SPECIFIC_TYPE* fake)
    {
        return Count(elements._elements, fake);
    }

    template<class SPECIFIC_TYPE, class H, class T>
    size_t Count(const ContainerMapList<TypeList<H, T> >& elements, SPECIFIC_TYPE* fake)
    {
        return Count(elements._TailElements, fake);
    }

    // non-const insert functions | 非常量插入函数重载集
    template<class SPECIFIC_TYPE>
    SPECIFIC_TYPE* Insert(ContainerMapList<SPECIFIC_TYPE>& elements, SPECIFIC_TYPE* obj)
    {
        // elements._element[hdl] = obj;
        obj->GetGridRef().link(&elements._element, obj);
        return obj;
    }

    template<class SPECIFIC_TYPE>
    SPECIFIC_TYPE* Insert(ContainerMapList<TypeNull>& /*elements*/, SPECIFIC_TYPE* /*obj*/)
    {
        return nullptr;
    }

    // this is a missed | 类型不匹配时的兜底分支
    template<class SPECIFIC_TYPE, class T>
    SPECIFIC_TYPE* Insert(ContainerMapList<T>& /*elements*/, SPECIFIC_TYPE* /*obj*/)
    {
        return nullptr;                                        // a missed | 未命中目标类型
    }

    // Recursion | 递归遍历 TypeList 插入
    template<class SPECIFIC_TYPE, class H, class T>
    SPECIFIC_TYPE* Insert(ContainerMapList<TypeList<H, T> >& elements, SPECIFIC_TYPE* obj)
    {
        SPECIFIC_TYPE* t = Insert(elements._elements, obj);
        return (t != nullptr ? t : Insert(elements._TailElements, obj));
    }

    // non-const remove method | 非常量移除函数重载集
    template<class SPECIFIC_TYPE>
    SPECIFIC_TYPE* Remove(ContainerMapList<SPECIFIC_TYPE>& /*elements*/, SPECIFIC_TYPE* obj)
    {
        obj->GetGridRef().unlink();
        return obj;
    }

    template<class SPECIFIC_TYPE>
    SPECIFIC_TYPE* Remove(ContainerMapList<TypeNull>& /*elements*/, SPECIFIC_TYPE* /*obj*/)
    {
        return nullptr;
    }

    // this is a missed | 类型不匹配时的兜底分支
    template<class SPECIFIC_TYPE, class T>
    SPECIFIC_TYPE* Remove(ContainerMapList<T>& /*elements*/, SPECIFIC_TYPE* /*obj*/)
    {
        return nullptr;                                        // a missed | 未命中目标类型
    }

    template<class SPECIFIC_TYPE, class T, class H>
    SPECIFIC_TYPE* Remove(ContainerMapList<TypeList<H, T> >& elements, SPECIFIC_TYPE* obj)
    {
        // The head element is bad | 先尝试头部类型，不匹配则递归尾部列表
        SPECIFIC_TYPE* t = Remove(elements._elements, obj);
        return (t != nullptr ? t : Remove(elements._TailElements, obj));
    }
}

#endif
