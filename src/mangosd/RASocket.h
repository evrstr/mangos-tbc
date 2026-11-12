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

#ifndef _RASOCKET_H
#define _RASOCKET_H

#include "Common.h"

#include "Network/AsyncSocket.hpp"

#include <functional>
#include <vector>
#include <string>

/// Remote Administration socket | 远程管理（RA）套接字类，负责远程控制连接的认证与命令分发
class RASocket : public MaNGOS::AsyncSocket<RASocket>
{
    private:
        /// 远程管理会话的认证状态枚举
        enum AuthLevel
        {
            None,                                           ///< 未认证，等待输入用户名
            HaveUsername,                                   ///< 已验证用户名，等待输入密码
            Authenticated                                   ///< 已通过认证，可执行管理命令
        };

        const bool m_secure;                                ///< 认证失败后是否立即断开连接
        bool m_restricted;                                  ///< 是否限制远程管理员使用控制台级命令

        std::string m_input;                                ///< 当前累积的整行输入缓冲区

        AuthLevel m_authLevel;                              ///< 当前连接所处的认证阶段
        AccountTypes m_accountLevel;                        ///< 已登录账号对应的权限等级
        uint32 m_accountId;                                 ///< 已登录账号 ID

        /// 异步读取并按行解析网络输入数据
        virtual bool ProcessIncomingData() override;
        /// 根据当前认证状态处理单行用户输入
        bool HandleInput();
        /// 异步向客户端发送响应文本
        void Send(const std::string& message);

    public:
        RASocket(boost::asio::io_context& context);
        virtual ~RASocket();

        /// 新建连接打开时的回调：发送欢迎信息并提示输入用户名
        bool OnOpen() override;
};
#endif
/// @}
