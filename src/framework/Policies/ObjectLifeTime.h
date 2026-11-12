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

#ifndef MANGOS_OBJECTLIFETIME_H
#define MANGOS_OBJECTLIFETIME_H

#include <stdexcept>
#include "Platform/Define.h"

typedef void (* Destroyer)(void);

namespace MaNGOS
{
    /// 注册进程退出时执行的清理函数
    void at_exit(void (*func)());

    /// 对象生命周期管理策略模板
    template<class T>
    class ObjectLifeTime
    {
        public:

            /// 调度在程序退出时调用的销毁函数
            static void ScheduleCall(void (*destroyer)())
            {
                at_exit(destroyer);
            }

            /// 当访问已销毁的单例（死引用）时调用
            DECLSPEC_NORETURN static void OnDeadReference() ATTR_NORETURN;
    };

    template <class T>
    void ObjectLifeTime<T>::OnDeadReference()           // We don't handle Dead Reference for now | 暂不处理死引用，直接抛出异常
    {
        throw std::runtime_error("Dead Reference");
    }
}

#endif
