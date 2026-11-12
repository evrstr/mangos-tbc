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

/// \addtogroup mangosd
/// @{
/// \file

#ifndef _MASTER_H
#define _MASTER_H

#define BOOST_BIND_NO_PLACEHOLDERS 1

#include "Common.h"
#include "Policies/Singleton.h"

#include <boost/asio.hpp>

/// Start the server | 服务端主控单例类（负责初始化数据库、网络监听、核心线程与系统信号）
class Master
{
    public:
        /// 运行服务端主流程并返回进程退出码
        int Run();
        /// 标记主线程是否已清理完毕、允许终止进程
        static volatile bool m_canBeKilled;

    private:
        /// 初始化并校验各个数据库连接与版本
        bool _StartDB();

        /// 注册系统终止信号处理函数
        void _HookSignals();
        /// 取消系统终止信号钩子
        void _UnhookSignals();
        /// 系统信号回调处理函数
        static void _OnSignal(int s);

        /// 清理当前分区所有账号与角色的在线状态
        void clearOnlineAccounts();

        boost::asio::io_context m_context;                  ///< 世界网络服务 Asio I/O 上下文
        boost::asio::io_context m_raContext;                ///< 远程管理（RA）网络服务 Asio I/O 上下文
};

#define sMaster MaNGOS::Singleton<Master>::Instance()
#endif
/// @}
