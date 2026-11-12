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

#ifndef _AUTHSOCKET_H
#define _AUTHSOCKET_H



#define BOOST_BIND_NO_PLACEHOLDERS 1

#include "Common.h"
#include "Auth/BigNumber.h"
#include "Auth/CryptoHash.h"
#include "Auth/SRP6.h"
#include "Util/ByteBuffer.h"

#include "Network/AsyncSocket.hpp"

#include <boost/asio.hpp>

#include <functional>

#define HMAC_RES_SIZE 20

struct sAuthLogonProof_C;
struct sAuthLogonPinData_C;

/// 登录认证会话套接字类：处理 SRP6 握手、重连验证、PIN/TOTP 令牌校验与服务器列表下发
class AuthSocket : public MaNGOS::AsyncSocket<AuthSocket>
{
    public:
        const static int s_BYTE_SIZE = 32;

        AuthSocket(boost::asio::io_context& context);

        /// 连接建立回调：初始化超时计时器
        bool OnOpen() override;

        /// 发送重连或登录证明响应包
        void SendProof(Sha1Hash sha);
        /// 组装服务器列表数据包并写入缓冲区
        void LoadRealmlist(ByteBuffer& pkt, uint32 acctid, uint8 accountSecurityLevel = 0);
        /// 校验客户端上传的图形 PIN 码哈希
        bool VerifyPinData(uint32 pin, const sAuthLogonPinData_C& clientData);
        void RegisterFailedLogin();           // counts a wrong password, PIN or authenticator code towards WrongPass.MaxCount | 计入错误登录次数（密码/PIN/动态口令错误均计入 WrongPass.MaxCount）
        /// 构造登录挑战成功应答包（包含 SRP6 公共参数、安全盐及安全输入提示）
        void BuildLogonChallengeSuccess(ByteBuffer& pkt, const char* v_hex, const char* s_hex, uint8 secLevel, const std::string& token);
        /// 根据 Base32 密钥生成当前时间窗口的 6 位 TOTP 动态口令
        int32 generateToken(char const* b32key);

        /// 统计指定安全等级账号可见的服务器数量
        uint8 getEligibleRealmCount(uint8 accountSecurityLevel);

        /// 校验客户端版本 CRC 摘要是否匹配预期构建版本
        bool VerifyVersion(uint8 const* a, int32 aLength, uint8 const* versionProof, bool isReconnect);
        bool _HandleLogonChallenge();
        bool _HandleLogonProof();
        bool _HandleReconnectChallenge();
        bool _HandleReconnectProof();
        bool _HandleRealmList();
        // data transfer handle for patch | 补丁文件传输相关指令处理

        bool _HandleXferResume();
        bool _HandleXferCancel();
        bool _HandleXferAccept();

    private:
        /// 校验客户端版本并完成登录认证（向客户端返回 Proof 并更新数据库会话密钥）
        void verifyVersionAndFinalizeAuthentication(std::shared_ptr<sAuthLogonProof_C> lp);
        /// 重置或启动连接超时定时器
        void ArmTimeoutTimer();

        enum eStatus
        {
            STATUS_CHALLENGE,
            STATUS_LOGON_PROOF,
            STATUS_RECON_PROOF,
            STATUS_PATCH,      // unused in CMaNGOS | CMaNGOS 中未使用补丁下发状态
            STATUS_AUTHED,
            STATUS_CLOSED
        };

        SRP6 srp;
        BigNumber _reconnectProof;

        eStatus _status;

        std::string _login;
        std::string _safelogin;
        std::string _token;
        std::string m_os;
        std::string m_platform;
        std::string m_locale;
        std::string _safelocale;
        uint16 _build;
        AccountTypes _accountSecurityLevel;

        BigNumber m_serverSecuritySalt;
        uint32 m_gridSeed = 0;
        bool m_promptPin = false;

        boost::asio::system_timer m_timeoutTimer;

        virtual bool ProcessIncomingData() override;
};
#endif
/// @}
