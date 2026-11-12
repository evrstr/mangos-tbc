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

#ifndef _AUTH_SARC4_H
#define _AUTH_SARC4_H

#include "Common.h"
#include <openssl/evp.h>

/// RC4 流加密封装类：基于 OpenSSL EVP_rc4，用于 WoW 客户端与服务端数据包头加解密
class SARC4
{
    public:
        /// 指定密钥长度构造 RC4 上下文（需随后调用 Init 设置密钥）
        SARC4(size_t len);
        /// 使用给定种子密钥与长度构造并初始化 RC4 上下文
        SARC4(const uint8 *seed, size_t len);
        ~SARC4();

        /// 设置或重置 RC4 加密密钥
        void Init(const uint8 *seed);
        /// 对数据缓冲区进行原地流加密/解密
        void UpdateData(uint8 *data, size_t len);

    private:
        EVP_CIPHER_CTX* m_ctx;  ///< OpenSSL 对称加密上下文
};
#endif
