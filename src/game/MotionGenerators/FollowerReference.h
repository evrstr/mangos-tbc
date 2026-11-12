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

#ifndef _FOLLOWERREFERENCE_H
#define _FOLLOWERREFERENCE_H

#include "Utilities/LinkedReference/Reference.h"

class TargetedMovementGeneratorBase;
class Unit;

/// 跟随关系引用节点：维护 TargetedMovementGeneratorBase 与被跟随 Unit 之间的双向生命周期联动
class FollowerReference : public Reference<Unit, TargetedMovementGeneratorBase>
{
    protected:
        /// 建立引用时将自身注册到目标 Unit 的跟随者列表
        void targetObjectBuildLink() override;
        /// 解除引用时从目标 Unit 的跟随者列表移除
        void targetObjectDestroyLink() override;
        /// 当跟随生成器销毁或目标失效时触发停止跟随回调
        void sourceObjectDestroyLink() override;
};
#endif
