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

#ifndef _MANGOSSOAP_H
#define _MANGOSSOAP_H

#include "Common.h"
#include "World/World.h"
#include "Accounts/AccountMgr.h"
#include "Log/Log.h"

#include "soapH.h"
#include "soapStub.h"

#include <memory>
#include <string>
#include <thread>

/// SOAP 远程管理服务线程类，监听并处理 HTTP/SOAP 管理命令请求
class SOAPThread
{
    private:
        static const int WorkerThreads = 5;                 ///< 预留工作线程数上限
        static const int AcceptTimeout = 3;                 ///< accept 接收连接超时时间（秒）
        static const int DataTimeout = 5;                   ///< 套接字收发数据超时时间（秒）
        static const int BackLogSize = 100;                 ///< 监听套接字等待队列长度

        const std::string m_host;                           ///< 绑定的监听 IP 地址
        const int m_port;                                   ///< 绑定的监听端口号

        soap m_soap;                                        ///< gSOAP 运行时上下文
        std::thread m_workerThread;                         ///< 后台服务工作线程

        SOAPThread(const std::string& host, int port);
        /// 工作线程主循环：接收并分发 SOAP 请求
        void Work();

    public:
        static const AccountTypes MinLevel = AccountTypes::SEC_ADMINISTRATOR; ///< 允许调用 SOAP 接口的最低账号权限等级
        static const int CommandOutputBufferSize = 256;     ///< 命令输出缓冲区初始预留大小（字节）

        // nullptr, with the reason in the error log, when the address cannot be bound | 创建 SOAPThread 实例；若无法绑定指定地址则记录错误日志并返回 nullptr
        static std::unique_ptr<SOAPThread> Create(const std::string& host, int port);
        ~SOAPThread();

        // serves the requests in a worker thread until the world stops | 启动工作线程并持续处理请求，直到世界停止运行
        void Start();
};

#endif
