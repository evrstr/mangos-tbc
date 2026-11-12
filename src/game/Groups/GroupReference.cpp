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

#include "Groups/Group.h"
#include "GroupReference.h"

/// 将成员引用挂载到目标 Group 链表头部
void GroupReference::targetObjectBuildLink()
{
    // called from link() (由 Reference::link() 调用)
    getTarget()->LinkMember(this);
}

/// 解除成员引用与目标 Group 的链接
void GroupReference::targetObjectDestroyLink()
{
    // called from unlink() (由 Reference::unlink() 调用)
    getTarget()->DelinkMember(this);
}

/// 源对象（玩家）销毁时通知目标 Group 断开链接
void GroupReference::sourceObjectDestroyLink()
{
    // called from invalidate() (由 Reference::invalidate() 调用)
    getTarget()->DelinkMember(this);
}
