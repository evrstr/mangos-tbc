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

#include "Chat/ChannelMgr.h"
#include "Policies/Singleton.h"
#include "World/World.h"

INSTANTIATE_SINGLETON_1(AllianceChannelMgr);
INSTANTIATE_SINGLETON_1(HordeChannelMgr);

/// 根据阵营或跨阵营配置获取频道管理器单例
ChannelMgr* channelMgr(Team team)
{
    if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
        return &MaNGOS::Singleton<AllianceChannelMgr>::Instance();        // cross-faction | 跨阵营频道互通模式

    if (team == ALLIANCE)
        return &MaNGOS::Singleton<AllianceChannelMgr>::Instance();
    if (team == HORDE)
        return &MaNGOS::Singleton<HordeChannelMgr>::Instance();

    return nullptr;
}

/// 析构：释放所有已管理的 Channel 对象
ChannelMgr::~ChannelMgr()
{
    for (auto& channel : channels)
        delete channel.second;

    channels.clear();
}

/// 获取或新建聊天频道（统一转换为小写宽字符串实现大小写无关匹配）
Channel* ChannelMgr::GetJoinChannel(const std::string& name, uint32 channel_id)
{
    std::wstring wname;
    Utf8toWStr(name, wname);
    wstrToLower(wname);

    if (channels.find(wname) == channels.end())
    {
        Channel* nchan = new Channel(name, channel_id);
        channels[wname] = nchan;
        return nchan;
    }

    return channels[wname];
}

/// 查询频道；找不到时按 pkt 标志决定是否回发 CHAT_NOT_MEMBER_NOTICE
Channel* ChannelMgr::GetChannel(const std::string& name, Player* p, bool pkt)
{
    std::wstring wname;
    Utf8toWStr(name, wname);
    wstrToLower(wname);

    ChannelMap::const_iterator i = channels.find(wname);

    if (i == channels.end())
    {
        if (pkt)
        {
            WorldPacket data;
            Channel::MakeNotMember(data, name);
            p->GetSession()->SendPacket(data);
        }

        return nullptr;
    }
    return i->second;
}

/// 当频道内无成员且不是 DBC 常驻频道时，销毁空频道释放内存
void ChannelMgr::LeftChannel(const std::string& name)
{
    std::wstring wname;
    Utf8toWStr(name, wname);
    wstrToLower(wname);

    ChannelMap::const_iterator i = channels.find(wname);

    if (i == channels.end())
        return;

    Channel* channel = i->second;

    if (channel->GetNumPlayers() == 0 && !channel->IsConstant())
    {
        channels.erase(wname);
        delete channel;
    }
}
