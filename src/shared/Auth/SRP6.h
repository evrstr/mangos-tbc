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

#ifndef _SRP6_H
#define _SRP6_H

#include "Common.h"
#include "Auth/BigNumber.h"
#include "Auth/CryptoHash.h"

#define HMAC_RES_SIZE 20

/*! Secure Remote Password protocol | 安全远程密码协议（SRP6）实现类，用于暴雪客户端登录认证握手 */
class SRP6
{
    public:
        const static int s_BYTE_SIZE = 32; /*!< length of salt | 盐值（s）字节长度 */

        //! Initializes SRP with a predefined prime (N) and generator module (g)
        //! 使用预定义大素数（N）与生成元（g）初始化 SRP6 实例
        SRP6(void);

        //! calculates the host public ephemeral (B)
        /*!
          generates also a random number as host private ephemeral (b)
          计算服务端公开临时值（B），同时随机生成服务端私有临时值（b）
        */
        void CalculateHostPublicEphemeral(void);

        //! calculates proof (M) of the strong session key (K)
        /*!
          \param username the unique identity of the account to authenticate
          计算强会话密钥（K）的认证证明（M）
          \param username 待认证账号的唯一用户名（大写）
        */
        void CalculateProof(std::string username);

        //! calculates a session key (S) based on client public ephemeral (A)
        /*!
          safeguard conditions are (A != 0) and (A % N != 0)
          \param lp_A the client public ephemeral (A)
          \param l the length of client public ephemeral (A)
          \return true on valid safeguard conditions otherwise false
          根据客户端公开临时值（A）计算原始会话密钥（S）；会校验安全前提条件 A != 0 且 A % N != 0
          \param lp_A 客户端公开临时值（A）字节数组
          \param l 客户端公开临时值（A）的字节长度
          \return 若满足安全前提条件返回 true，否则返回 false
        */
        bool CalculateSessionKey(uint8* lp_A, int l);

        //! calculates the password verifier (v)
        /*!
          \param rI a sha1 hash of USERNAME:PASSWORD
          \return true on success otherwise false if s is faulty
          生成随机盐值并计算密码校验子（v）
          \param rI USERNAME:PASSWORD 的 SHA-1 十六进制哈希字符串
          \return 成功返回 true，若盐值异常则返回 false
        */
        bool CalculateVerifier(const std::string& rI);

        //! calculates the password verifier (v) based on a predefined salt (s)
        /*!
          \param rI a sha1 hash of USERNAME:PASSWORD
          \param salt a predefined salt (s)
          \return true on success otherwise false if s is faulty
          基于给定的十六进制盐值（s）计算密码校验子（v）
          \param rI USERNAME:PASSWORD 的 SHA-1 十六进制哈希字符串
          \param salt 预定义的十六进制盐值字符串（s）
          \return 成功返回 true，若盐值无效则返回 false
        */
        bool CalculateVerifier(const std::string& rI, const char* salt);

        //! generates a strong session key (K) of session key (S)
        //! 通过交错哈希从原始会话密钥（S）派生 40 字节强会话密钥（K）
        void HashSessionKey(void);

        //! compares proof (M) of strong session key (K)
        /*!
          \param lp_M client proof (M) of the strong session key (K)
          \param l the length of client proof (M)
          \return true if client and server proof matches otherwise false
          比对客户端提交的证明（M）与服务端计算的 M 是否一致
          \param lp_M 客户端提交的证明字节数组
          \param l 证明数组长度
          \return 一致返回 true，否则返回 false（注意：当前实现若匹配返回 false，见 cpp 逻辑）
        */
        bool Proof(uint8* lp_M, int l);

        //! compare password verifier (v)
        /*!
          verifies if provided password matches the password verifier (v)
          requires to use the same salt (s) which was initially used to compute v.
          \param vC predefined password verifier (v) read from database
          \return true if password verifier matches otherwise false
          比对重新计算的密码校验子（v）与数据库中读取的 vC 是否一致（需使用同一盐值 s）
          \param vC 从数据库读取的预存校验子十六进制字符串
          \return 匹配返回 true，否则返回 false
        */
        bool ProofVerifier(std::string vC);

        //! generate hash for proof of strong session key (K)
        /*!
          this hash has to be send to the client for client-side proof.
          client has to show it's proof first. If the server detects an incorrect proof
          it must abort without showing it's proof.
          \param sha reference to an empty Sha1Hash object
          生成服务端回传给客户端的强会话密钥验证哈希（M2 = H(A, M, K)）；
          须先校验客户端 M 通过后再发送，否则服务端应直接中止连接。
          \param sha 用于接收最终哈希结果的 Sha1Hash 对象引用
        */
        void Finalize(Sha1Hash& sha);

        BigNumber GetHostPublicEphemeral(void) { return B; };
        BigNumber GetGeneratorModulo(void) { return g; };
        BigNumber GetPrime(void) { return N; };
        BigNumber GetProof(void) { return M; };
        BigNumber GetSalt(void) { return s; };
        BigNumber GetStrongSessionKey(void) { return K; };
        BigNumber GetVerifier(void) { return v; };

        /// 设置盐值（十六进制字符串），非零且有效时返回 true
        bool SetSalt(const char* new_s);
        /// 直接设置强会话密钥 K（十六进制字符串）
        void SetStrongSessionKey(const char* new_K) { K.SetHexStr(new_K); };
        /// 设置密码校验子 v（十六进制字符串），非零且有效时返回 true
        bool SetVerifier(const char* new_v);

    private:
        BigNumber A, u, S;      ///< A: 客户端公开临时值, u: 扰乱参数 H(A,B), S: 原始会话密钥
        BigNumber N, s, g, v;   ///< N: 大素数模数, s: 盐值, g: 生成元, v: 密码校验子
        BigNumber b, B;         ///< b: 服务端私有临时值, B: 服务端公开临时值
        BigNumber K;            ///< 40 字节交错哈希强会话密钥
        BigNumber M;            ///< 20 字节认证证明摘要
};
#endif
