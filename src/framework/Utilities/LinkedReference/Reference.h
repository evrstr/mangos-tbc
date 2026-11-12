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

#ifndef _REFERENCE_H
#define _REFERENCE_H

#include "Utilities/LinkedList.h"

//=====================================================

/// 双向关联引用基类（表示从 FROM 指向 TO 的可追踪弱/观测引用）
template<class TO, class FROM>
class Reference : public LinkedListElement
{
    private:

        TO* iRefTo;
        FROM* iRefFrom;

    protected:

        // Tell our refTo (target) object that we have a link | 通知目标对象 (refTo) 新链接已建立
        virtual void targetObjectBuildLink() = 0;

        // Tell our refTo (taget) object, that the link is cut | 通知目标对象 (refTo) 链接已断开
        virtual void targetObjectDestroyLink() = 0;

        // Tell our refFrom (source) object, that the link is cut (Target destroyed) | 通知源对象 (refFrom) 目标已销毁、链接失效
        virtual void sourceObjectDestroyLink() = 0;

    public:

        Reference()
            : iRefTo(nullptr), iRefFrom(nullptr)
        {
        }

        virtual ~Reference() {}

        // Create new link | 建立从 fromObj 到 toObj 的引用链接
        void link(TO* toObj, FROM* fromObj)
        {
            assert(fromObj);                                // fromObj MUST not be nullptr | fromObj 绝对不能为空指针
            if (isValid())
                unlink();

            if (toObj != nullptr)
            {
                iRefTo = toObj;
                iRefFrom = fromObj;
                targetObjectBuildLink();
            }
        }

        // We don't need the reference anymore. Call comes from the refFrom object
        // Tell our refTo object, that the link is cut
        // 由源对象主动解除引用，通知目标对象断开链接
        void unlink()
        {
            targetObjectDestroyLink();
            delink();
            iRefTo = nullptr;
            iRefFrom = nullptr;
        }

        // Link is invalid due to destruction of referenced target object. Call comes from the refTo object
        // Tell our refFrom object, that the link is cut
        // 目标对象销毁时调用以使引用失效（必须保留 iRefFrom 供通知源对象使用）
        void invalidate()                                   // the iRefFrom MUST remain!! | iRefFrom 必须保留！
        {
            sourceObjectDestroyLink();
            delink();
            iRefTo = nullptr;
        }

        bool isValid() const                                // Only check the iRefTo | 仅检查目标指针 iRefTo 是否有效
        {
            return iRefTo != nullptr;
        }

        Reference<TO, FROM>*       next()       { return ((Reference<TO, FROM>*) LinkedListElement::next()); }
        Reference<TO, FROM> const* next() const { return ((Reference<TO, FROM> const*) LinkedListElement::next()); }
        Reference<TO, FROM>*       prev()       { return ((Reference<TO, FROM>*) LinkedListElement::prev()); }
        Reference<TO, FROM> const* prev() const { return ((Reference<TO, FROM> const*) LinkedListElement::prev()); }

        Reference<TO, FROM>*       nocheck_next()       { return ((Reference<TO, FROM>*) LinkedListElement::nocheck_next()); }
        Reference<TO, FROM> const* nocheck_next() const { return ((Reference<TO, FROM> const*) LinkedListElement::nocheck_next()); }
        Reference<TO, FROM>*       nocheck_prev()       { return ((Reference<TO, FROM>*) LinkedListElement::nocheck_prev()); }
        Reference<TO, FROM> const* nocheck_prev() const { return ((Reference<TO, FROM> const*) LinkedListElement::nocheck_prev()); }

        TO* operator->() const { return iRefTo; }
        TO* getTarget() const { return iRefTo; }

        FROM* getSource() const { return iRefFrom; }
};

//=====================================================

#endif
