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

#ifndef MANGOSSERVER_PACKET_BUILDER_H
#define MANGOSSERVER_PACKET_BUILDER_H

class ByteBuffer;
class WorldPacket;

namespace Movement
{
    class MoveSpline;
    /// 样条移动网络封包序列化构建器
    class PacketBuilder
    {
            /// 写入 SMSG_MONSTER_MOVE 公共头部（起点、样条 ID、朝向模式、标志与总耗时）
            static void WriteCommonMonsterMovePart(const MoveSpline& move_spline, WorldPacket& data);
        public:

            /// 构造完整的怪物移动更新封包（SMSG_MONSTER_MOVE）
            static void WriteMonsterMove(const MoveSpline& move_spline, WorldPacket& data);
            /// 构造对象创建封包（SMSG_UPDATE_OBJECT）中的样条移动状态块
            static void WriteCreate(const MoveSpline& move_spline, ByteBuffer& data);
    };
}
#endif // MANGOSSERVER_PACKET_BUILDER_H
