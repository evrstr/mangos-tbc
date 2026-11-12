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

#ifndef _AUTH_BIGNUMBER_H
#define _AUTH_BIGNUMBER_H

#include "Common.h"
#include <vector>

struct bignum_st;

/// 基于 OpenSSL BIGNUM 的大整数封装类：用于 SRP6 认证与密钥计算中的大数运算
class BigNumber
{
    public:
        /// 构造初始化为 0 的大整数
        BigNumber();
        /// 拷贝构造函数
        BigNumber(const BigNumber& bn);
        /// 从 32 位无符号整数构造大整数
        BigNumber(uint32);
        ~BigNumber();

        /// 设置为 32 位无符号整数值
        void SetDword(uint32);
        /// 设置为 64 位无符号整数值
        void SetQword(uint64);
        /// 从小端序字节数组设置大整数值
        void SetBinary(const uint8* bytes, int len);
        /// 从十六进制字符串解析并设置大整数值
        int SetHexStr(const char* str);

        /// 生成指定比特位数的密码学随机大整数
        void SetRand(int numbits);

        BigNumber& operator=(const BigNumber& bn);

        BigNumber& operator+=(const BigNumber& bn);
        BigNumber operator+(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t += bn;
        }
        BigNumber& operator-=(const BigNumber& bn);
        BigNumber operator-(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t -= bn;
        }
        BigNumber& operator*=(const BigNumber& bn);
        BigNumber operator*(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t *= bn;
        }
        BigNumber& operator/=(const BigNumber& bn);
        BigNumber operator/(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t /= bn;
        }
        BigNumber& operator%=(const BigNumber& bn);
        BigNumber operator%(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t %= bn;
        }

        /// 判断当前大整数是否为 0
        bool isZero() const;

        /// 模幂运算：计算 (this ^ bn1) % bn2
        BigNumber ModExp(const BigNumber& bn1, const BigNumber& bn2);
        /// 幂运算：计算 this ^ bn
        BigNumber Exp(const BigNumber&);

        /// 获取当前大整数占用的有效字节数
        int GetNumBytes(void) const;

        /// 获取底层 OpenSSL bignum_st 指针
        struct bignum_st* BN() { return _bn; }

        /// 转换为 32 位无符号整数
        uint32 AsDword() const;
        /// 导出为字节数组（默认小端序并按 minSize 补齐前导零）
        std::vector<uint8> AsByteArray(int minSize = 0, bool reverse = true) const;

        /// 导出为十六进制字符串（调用方需负责 OPENSSL_free 释放）
        const char* AsHexStr() const;
        /// 导出为十进制字符串（调用方需负责 OPENSSL_free 释放）
        const char* AsDecStr() const;

    private:
        struct bignum_st* _bn;      ///< 底层 OpenSSL BIGNUM 句柄
        uint8* _array;              ///< 保留的字节缓冲区指针（析构时清理）
};
#endif
