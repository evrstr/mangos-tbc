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

/// \addtogroup realmd
/// @{
/// \file

#ifndef _REALMLIST_H
#define _REALMLIST_H

#include "Common.h"
#include <array>

/// 客户端构建版本与各平台二进制校验哈希信息
struct RealmBuildInfo
{
    int build;
    int major_version;
    int minor_version;
    int bugfix_version;
    int hotfix_version;
    std::array<uint8, 20> WindowsHash;
    std::array<uint8, 20> MacHash;
};

/// 根据客户端 Build 号查找对应的版本与校验信息
RealmBuildInfo const* FindBuildInfo(uint16 _build);
/// 根据客户端 Build 号和数据库时区 ID 映射到客户端界面显示的服务器分类 ID
uint8 GetRealmCategoryIdByBuildAndZone(uint16 _build, RealmZone _realmZone);

typedef std::set<uint32> RealmBuilds;

/// Storage object for a realm | 单个游戏服务器（Realm）信息存储结构体
struct Realm
{
    std::string address;
    uint8 icon;
    RealmFlags realmflags;                                  // realmflags | 服务器状态标志（离线、推荐、指定版本等）
    uint8 timezone;
    uint32 m_ID;
    AccountTypes allowedSecurityLevel;                      // current allowed join security level (show as locked for not fit accounts) | 允许登录的最低账号权限等级（不满足条件的账号将显示为锁定）
    float populationLevel;
    RealmBuilds realmbuilds;                                // list of supported builds (updated in DB by mangosd) | 支持的客户端版本号集合（由 mangosd 写入数据库）
    RealmBuildInfo realmBuildInfo;                          // build info for show version in list | 用于在服务器列表中显示版本的构建信息
};

/// Storage object for the list of realms on the server | 服务器列表全局管理器
class RealmList
{
    public:
        typedef std::map<std::string, Realm> RealmMap;

        static RealmList& Instance();

        RealmList();
        ~RealmList() {}

        /// 初始化列表并设置定期刷新间隔（秒）
        void Initialize(uint32 updateInterval);

        /// 若到达刷新时间间隔则从数据库重新加载服务器列表
        void UpdateIfNeed();

        RealmMap::const_iterator begin() const { return m_realms.begin(); }
        RealmMap::const_iterator end() const { return m_realms.end(); }
        uint32 size() const { return m_realms.size(); }
    private:
        /// 从数据库查询 realmlist 表更新内存映射
        void UpdateRealms(bool init);
        /// 更新或新增单个服务器条目
        void UpdateRealm(uint32 ID, const std::string& name, const std::string& address, uint32 port, uint8 icon, RealmFlags realmflags, uint8 timezone, AccountTypes allowedSecurityLevel, float popu, const std::string& builds);
    private:
        RealmMap m_realms;                                  ///< Internal map of realms | 内部服务器名称到 Realm 对象的映射表
        uint32   m_UpdateInterval;
        time_t   m_NextUpdateTime;
};

#define sRealmList RealmList::Instance()

#endif
/// @}
