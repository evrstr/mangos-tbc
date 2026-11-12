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
#ifndef MANGOSSERVER_CHANNELMGR_H
#define MANGOSSERVER_CHANNELMGR_H

#include "Common.h"
#include "Chat/Channel.h"

#include <map>

/// 聊天频道管理器：按频道名称（小写宽字符）索引并管理阵营下的聊天频道实例
class ChannelMgr
{
    public:
        typedef std::map<std::wstring, Channel*> ChannelMap;
        ChannelMgr() {}
        ~ChannelMgr();

        /// 获取当前全部频道映射表
        const ChannelMap& GetChannels() const { return channels; }

        /// 查找或创建指定名称与 ID 的频道用于玩家加入
        Channel* GetJoinChannel(const std::string& name, uint32 channel_id);
        /// 查找已存在的频道（若不存在且 pkt 为 true 则向玩家发送非成员通知）
        Channel* GetChannel(const std::string& name, Player* p, bool pkt = true);
        /// 玩家离开频道后的清理：若为非常驻空频道则销毁并移除
        void LeftChannel(const std::string& name);
    private:
        ChannelMap channels;                                /// 已注册的聊天频道映射（键为小写宽字符串名称）
};

/// 联盟聊天频道管理器（跨阵营开启时亦作为全局共用实例）
class AllianceChannelMgr : public ChannelMgr {};
/// 部落聊天频道管理器
class HordeChannelMgr    : public ChannelMgr {};

/// 根据玩家阵营返回对应的聊天频道管理器单例
ChannelMgr* channelMgr(Team team);

#endif
