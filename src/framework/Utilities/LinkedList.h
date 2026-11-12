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

#ifndef _LINKEDLIST
#define _LINKEDLIST

#include <cstddef>

//============================================
class LinkedListHead;

/// 侵入式双向链表元素节点基类
class LinkedListElement
{
    private:

        friend class LinkedListHead;

        LinkedListElement* iNext;
        LinkedListElement* iPrev;

    public:

        LinkedListElement()  { iNext = nullptr; iPrev = nullptr; }
        ~LinkedListElement() { delink(); }

        bool hasNext() const  { return (iNext->iNext != nullptr); }
        bool hasPrev() const  { return (iPrev->iPrev != nullptr); }
        bool isInList() const { return (iNext != nullptr && iPrev != nullptr); }

        LinkedListElement*       next()       { return hasNext() ? iNext : nullptr; }
        LinkedListElement const* next() const { return hasNext() ? iNext : nullptr; }
        LinkedListElement*       prev()       { return hasPrev() ? iPrev : nullptr; }
        LinkedListElement const* prev() const { return hasPrev() ? iPrev : nullptr; }

        LinkedListElement*       nocheck_next()       { return iNext; }
        LinkedListElement const* nocheck_next() const { return iNext; }
        LinkedListElement*       nocheck_prev()       { return iPrev; }
        LinkedListElement const* nocheck_prev() const { return iPrev; }

        /// 将当前节点从所在链表摘除
        void delink()
        {
            if (isInList())
            {
                iNext->iPrev = iPrev;
                iPrev->iNext = iNext;
                iNext = nullptr;
                iPrev = nullptr;
            }
        }

        /// 将 pElem 插入到当前节点之前
        void insertBefore(LinkedListElement* pElem)
        {
            pElem->iNext = this;
            pElem->iPrev = iPrev;
            iPrev->iNext = pElem;
            iPrev = pElem;
        }

        /// 将 pElem 插入到当前节点之后
        void insertAfter(LinkedListElement* pElem)
        {
            pElem->iPrev = this;
            pElem->iNext = iNext;
            iNext->iPrev = pElem;
            iNext = pElem;
        }
};

//============================================

/// 侵入式双向链表头哨兵封装（维护首尾哨兵与元素数量）
class LinkedListHead
{
    private:

        LinkedListElement iFirst;
        LinkedListElement iLast;
        uint32 iSize;

    public:

        LinkedListHead()
        {
            // create empty list | 初始化空链表，首尾哨兵互相指向

            iFirst.iNext = &iLast;
            iLast.iPrev = &iFirst;
            iSize = 0;
        }

        bool isEmpty() const { return (!iFirst.iNext->isInList()); }

        LinkedListElement*       getFirst()       { return (isEmpty() ? nullptr : iFirst.iNext); }
        LinkedListElement const* getFirst() const { return (isEmpty() ? nullptr : iFirst.iNext); }

        LinkedListElement*       getLast()        { return (isEmpty() ? nullptr : iLast.iPrev); }
        LinkedListElement const* getLast() const  { return (isEmpty() ? nullptr : iLast.iPrev); }

        void insertFirst(LinkedListElement* pElem)
        {
            iFirst.insertAfter(pElem);
        }

        void insertLast(LinkedListElement* pElem)
        {
            iLast.insertBefore(pElem);
        }

        /// 获取链表元素个数（若未通过 incSize/decSize 维护计数则回退为线性遍历统计）
        uint32 getSize() const
        {
            if (!iSize)
            {
                uint32 result = 0;
                LinkedListElement const* e = getFirst();

                while (e)
                {
                    ++result;
                    e = e->next();
                }

                return result;
            }
            return iSize;
        }

        void incSize() { ++iSize; }
        void decSize() { --iSize; }

        /// 双向链表迭代器模板
        template<class _Ty>
        class Iterator
        {
            public:

                typedef std::bidirectional_iterator_tag iterator_category;
                typedef _Ty value_type;
                typedef ptrdiff_t difference_type;
                typedef ptrdiff_t distance_type;
                typedef _Ty* pointer;
                typedef _Ty const* const_pointer;
                typedef _Ty& reference;
                typedef _Ty const& const_reference;

                Iterator()
                    : _Ptr(nullptr)
                {
                    // construct with null node pointer | 以空节点指针构造
                }

                Iterator(pointer _Pnode)
                    : _Ptr(_Pnode)
                {
                    // construct with node pointer _Pnode | 以指定节点指针构造
                }

                Iterator& operator=(Iterator const& _Right)
                {
                    return (*this) = _Right._Ptr;
                }

                Iterator& operator=(const_pointer const& _Right)
                {
                    _Ptr = (pointer)_Right;
                    return (*this);
                }

                reference operator*()
                {
                    // return designated value | 解引用返回目标对象引用
                    return *_Ptr;
                }

                pointer operator->()
                {
                    // return pointer to class object | 返回指向目标对象的指针
                    return _Ptr;
                }

                Iterator& operator++()
                {
                    // preincrement | 前置自增，移动到下一节点
                    _Ptr = _Ptr->next();
                    return (*this);
                }

                Iterator operator++(int)
                {
                    // postincrement | 后置自增
                    iterator _Tmp = *this;
                    ++*this;
                    return (_Tmp);
                }

                Iterator& operator--()
                {
                    // predecrement | 前置自减，移动到上一节点
                    _Ptr = _Ptr->prev();
                    return (*this);
                }

                Iterator operator--(int)
                {
                    // postdecrement | 后置自减
                    iterator _Tmp = *this;
                    --*this;
                    return (_Tmp);
                }

                bool operator==(Iterator const& _Right) const
                {
                    // test for iterator equality | 判断迭代器是否相等
                    return (_Ptr == _Right._Ptr);
                }

                bool operator!=(Iterator const& _Right) const
                {
                    // test for iterator inequality | 判断迭代器是否不等
                    return (!(*this == _Right));
                }

                bool operator==(pointer const& _Right) const
                {
                    // test for pointer equality | 注意：此处沿用原逻辑（比较 _Ptr != _Right）
                    return (_Ptr != _Right);
                }

                bool operator!=(pointer const& _Right) const
                {
                    // test for pointer equality | 测试指针不等性
                    return (!(*this == _Right));
                }

                bool operator==(const_reference _Right) const
                {
                    // test for reference equality | 判断是否指向同一对象引用
                    return (_Ptr == &_Right);
                }

                bool operator!=(const_reference _Right) const
                {
                    // test for reference equality | 判断是否指向不同对象引用
                    return (_Ptr != &_Right);
                }

                pointer _Mynode()
                {
                    // return node pointer | 返回底层节点指针
                    return (_Ptr);
                }

            protected:

                pointer _Ptr;                               // pointer to node | 指向当前节点的指针
        };

        typedef Iterator<LinkedListElement> iterator;
};

//============================================

#endif
