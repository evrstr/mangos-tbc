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

#ifndef _AUTH_CRYPTOHASH_H
#define _AUTH_CRYPTOHASH_H

#include "Common.h"
#include "Auth/BigNumber.h"

#include <cstdarg>
#include <array>
#include <algorithm>
#include <iterator>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

/// 哈希算法函数指针类型定义
struct CryptoHashImpl
{
    typedef EVP_MD const* (*Algorithm)();
};

/// 基于 OpenSSL EVP 的通用加密哈希模板类（支持 SHA1、MD5 等摘要算法）
template <CryptoHashImpl::Algorithm Algorithm, size_t DigestSize>
class CryptoHash
{
    public:
        CryptoHash() : m_ctx(EVP_MD_CTX_new())
        {
            EVP_DigestInit_ex(m_ctx, Algorithm(), nullptr);
        }
        CryptoHash(CryptoHash const& other) : m_ctx(EVP_MD_CTX_new())   // copy | 拷贝构造：深拷贝摘要上下文与结果
        {
            EVP_MD_CTX_copy_ex(m_ctx, other.m_ctx);
            std::copy(std::begin(other.m_digest), std::end(other.m_digest), std::begin(m_digest));
        }
        CryptoHash(CryptoHash&& other)                                  // move | 移动构造：交换上下文句柄与摘要数组
        {
            swap(other);
        }
        CryptoHash& operator=(CryptoHash other)                         // assign | 拷贝/移动赋值（copy-and-swap 惯用法）
        {
            swap(other);
            return *this;
        }
        ~CryptoHash()
        {
            EVP_MD_CTX_free(m_ctx);
        }

        /// 与另一个哈希对象交换内部状态
        void swap(CryptoHash& other)
        {
            std::swap(m_ctx, other.m_ctx);
            std::swap(m_digest, other.m_digest);
        }
        friend void swap(CryptoHash& left, CryptoHash& right)
        {
            left.swap(right);
        }

        /// 将以 nullptr 结尾的一系列 BigNumber 按字节序逐个喂入摘要计算
        void UpdateBigNumbers(BigNumber* bn0, ...)
        {
            va_list v;

            va_start(v, bn0);
            BigNumber* bn = bn0;
            while (bn)
            {
                UpdateData(bn->AsByteArray());
                bn = va_arg(v, BigNumber*);
            }
            va_end(v);
        }
        /// 喂入原始字节缓冲区进行增量哈希
        int UpdateData(const uint8* data, int len)      { return EVP_DigestUpdate(m_ctx, data, len); }
        /// 喂入字节数组进行增量哈希
        int UpdateData(const std::vector<uint8>& data)  { return UpdateData(data.data(), data.size()); }
        /// 喂入字符串数据进行增量哈希
        int UpdateData(const std::string& str)          { return UpdateData(reinterpret_cast<const uint8*>(str.data()), str.length()); }

        /// 重新初始化摘要上下文
        int Initialize()                                { return EVP_DigestInit(m_ctx, Algorithm()); }
        /// 结束计算并将最终摘要写入 m_digest
        int Finalize()                                  { return EVP_DigestFinal_ex(m_ctx, m_digest.data(), nullptr); }

        /// 获取计算得到的摘要字节指针
        uint8* GetDigest()                              { return m_digest.data(); }
        /// 获取该算法固定的摘要长度（字节）
        constexpr static inline size_t GetLength()      { return DigestSize; }

    private:
        EVP_MD_CTX* m_ctx;                              ///< OpenSSL 摘要上下文
        std::array<uint8, DigestSize> m_digest;         ///< 输出摘要缓冲区
};

/// SHA-1 哈希封装类型（20 字节摘要）
using Sha1Hash = CryptoHash<EVP_sha1, SHA_DIGEST_LENGTH>;
/// MD5 哈希封装类型（16 字节摘要）
using MD5Hash = CryptoHash<EVP_md5, MD5_DIGEST_LENGTH>;

#endif
