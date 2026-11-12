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

#ifndef _AUTH_HMACSHA1_H
#define _AUTH_HMACSHA1_H

#include "Common.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>

class BigNumber;

#define SEED_KEY_SIZE 16

/// 基于 OpenSSL EVP_MAC 的 HMAC-SHA1 消息认证码计算类（用于数据包加密密钥派生与 TOTP 校验）
class HMACSHA1
{
    public:
        // len can be more than SEED_KEY_SIZE (m_key only ever holds the first SEED_KEY_SIZE bytes of it, for
        // Initialize() below); do not call Initialize() afterwards on such an instance, it would re-key with those
        // truncated bytes instead of the original, longer key.
        // | len 可以大于 SEED_KEY_SIZE（m_key 仅保存前 SEED_KEY_SIZE 字节供后续 Initialize() 使用）；
        // | 若传入了更长密钥，切勿再调用 Initialize()，否则会改用截断后的 m_key 重新初始化密钥。
        HMACSHA1(uint32 len, const uint8* seed);
        /// 重载构造函数：直接使用外部传入密钥初始化 MAC，不拷贝到 m_key
        HMACSHA1(uint32 len, const uint8* seed, bool);
        ~HMACSHA1();
        /// 喂入大整数参与 HMAC 计算
        void UpdateBigNumber(BigNumber* bn, int minSize = 0);
        /// 喂入原始字节数据参与 HMAC 计算
        void UpdateData(const uint8* data, int length);
        /// 喂入字符串数据参与 HMAC 计算
        void UpdateData(const std::string& str);
        /// 使用构造函数保留的前 SEED_KEY_SIZE 字节密钥重新初始化上下文
        void Initialize();           // re-keys with the first SEED_KEY_SIZE bytes of the constructor's key, see above | 使用构造函数密钥的前 SEED_KEY_SIZE 字节重新置入密钥，见上说明
        /// 结束计算并将 20 字节结果写入 m_digest
        void Finalize();
        /// 便捷方法：追加单个大整数并完成计算，返回摘要指针
        uint8* ComputeHash(BigNumber* bn);
        /// 获取计算结果摘要指针
        uint8* GetDigest() { return m_digest; }
        /// 获取 HMAC-SHA1 摘要固定长度（20 字节）
        static int GetLength() { return SHA_DIGEST_LENGTH; }
    private:
        EVP_MAC* m_mac;                     ///< OpenSSL HMAC 算法对象
        EVP_MAC_CTX* m_ctx;                 ///< OpenSSL MAC 上下文
        uint8 m_key[SEED_KEY_SIZE];         ///< 缓存的前 16 字节种子密钥
        uint8 m_digest[SHA_DIGEST_LENGTH];  ///< 输出摘要缓冲区
};
#endif
