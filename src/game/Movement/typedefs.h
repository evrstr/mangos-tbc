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

#ifndef MANGOSSERVER_TYPEDEFS_H
#define MANGOSSERVER_TYPEDEFS_H

#include "Common.h"

namespace G3D
{
    class Vector2;
    class Vector3;
    class Vector4;
}

namespace Movement
{
    using G3D::Vector2;
    using G3D::Vector3;
    using G3D::Vector4;

    /// 秒转毫秒
    inline uint32 SecToMS(float sec)
    {
        return static_cast<uint32>(sec * 1000.f);
    }

    /// 毫秒转秒
    inline float MSToSec(uint32 ms)
    {
        return ms / 1000.f;
    }

    /// 循环递增计数器，达到上限后归零重置（用于生成 Spline ID 等）
    template<class T, T limit>
    class counter
    {
        public:
            counter() { init();}

            /// 递增计数，达到上限时回绕归零
            void Increase()
            {
                if (m_counter == limit)
                    init();
                else
                    ++m_counter;
            }

            /// 生成并返回一个新的自增 ID
            T NewId() { Increase(); return m_counter;}
            /// 获取当前计数值
            T getCurrent() const { return m_counter;}

        private:
            void init() { m_counter = 0; }
            T m_counter;
    };

    typedef counter<uint32, 0xFFFFFFFF> UInt32Counter; // 32 位无符号整型循环计数器
}

#endif // MANGOSSERVER_TYPEDEFS_H
