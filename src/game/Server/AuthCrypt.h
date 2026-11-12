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

#ifndef _AUTHCRYPT_H
#define _AUTHCRYPT_H

#include <Common.h>
#include <vector>

class BigNumber;

/// 世界服数据包包头加解密器（基于 HMAC-SHA1 派生密钥的流加密，TBC 协议加密客户端上行前 6 字节与下行前 4 字节）
class AuthCrypt
{
    public:
        AuthCrypt();

        /// 使用 SRP6 会话密钥 K 初始化加解密流密钥与游标
        void Init(BigNumber* K);

        /// 解密从客户端接收到的包头字节
        void DecryptRecv(uint8*, size_t);
        /// 加密待发送给客户端的包头字节
        void EncryptSend(uint8*, size_t);

    private:
        std::vector<uint8> _key;                            ///< 由会话密钥 K 派生的 20 字节加解密密钥
        uint8 _send_i, _send_j, _recv_i, _recv_j;           ///< 发送与接收流密码状态游标
        bool _initialized;                                  ///< 是否已完成密钥初始化
};
#endif
