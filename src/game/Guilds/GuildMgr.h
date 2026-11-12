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

#ifndef _GUILDMGR_H
#define _GUILDMGR_H

#include "Common.h"
#include "Util/UniqueTrackablePtr.h"

class Guild;
class ObjectGuid;

/// 全局公会管理器：负责从数据库加载全部公会并提供按 ID、名称与会长查找接口
class GuildMgr
{
        typedef std::unordered_map<uint32, MaNGOS::unique_trackable_ptr<Guild>> GuildMap;

        GuildMap m_GuildMap;
    public:
        GuildMgr();
        ~GuildMgr();

        /// 注册公会实例到全局哈希表
        void AddGuild(Guild* guild);
        /// 从全局哈希表移除并释放公会实例
        void RemoveGuild(uint32 guildId);

        Guild* GetGuildById(uint32 guildId) const;
        Guild* GetGuildByName(std::string const& name) const;
        Guild* GetGuildByLeader(ObjectGuid const& guid) const;
        std::string GetGuildNameById(uint32 guildId) const;

        /// 启动时从数据库批量加载公会、职级、成员、权限、事件日志及公会银行数据
        void LoadGuilds();
};

#define sGuildMgr MaNGOS::Singleton<GuildMgr>::Instance()

#endif // _GUILDMGR_H
