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

#ifndef MANGOS_FACTORY_HOLDER
#define MANGOS_FACTORY_HOLDER

#include "Platform/Define.h"
#include "ObjectRegistry.h"
#include "Policies/Singleton.h"

/** FactoryHolder holds a factory object of a specific type
 * FactoryHolder 持有特定类型的工厂对象
 */
template<class T, class Key = std::string>
class FactoryHolder
{
    public:
        typedef ObjectRegistry<FactoryHolder<T, Key >, Key > FactoryHolderRegistry;
        typedef MaNGOS::Singleton<FactoryHolderRegistry > FactoryHolderRepository;

        FactoryHolder(Key k) : i_key(k) {}
        virtual ~FactoryHolder() {}
        /// 获取工厂键值
        inline Key key() const { return i_key; }

        /// 将自身注册到全局工厂仓库
        void RegisterSelf(void) { FactoryHolderRepository::Instance().InsertItem(this, i_key); }
        /// 从全局工厂仓库注销自身
        void DeregisterSelf(void) { FactoryHolderRepository::Instance().RemoveItem(this, false); }

        /// Abstract Factory create method | 抽象工厂创建方法
        virtual T* Create(void* data = nullptr) const = 0;
    private:
        Key i_key;
};

/** Permissible is a classic way of letting the object decide
 * whether how good they handle things.  This is not retricted
 * to factory selectors.
 * Permissible 是一种经典模式，让对象自行决定其对特定事物的处理适配程度（不仅限于工厂选择器）。
 */
template<class T>
class Permissible
{
    public:
        virtual ~Permissible() {}
        /// 判断对给定目标对象的支持/许可程度
        virtual int Permit(const T*) const = 0;
};
#endif
