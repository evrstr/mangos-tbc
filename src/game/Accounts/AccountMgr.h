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

#ifndef _ACCMGR_H
#define _ACCMGR_H

#include "Common.h"

// 账号操作结果枚举
enum AccountOpResult
{
    AOR_OK,                                                 // 操作成功
    AOR_NAME_TOO_LONG,                                      // 用户名过长
    AOR_PASS_TOO_LONG,                                      // 密码过长
    AOR_NAME_ALREADY_EXIST,                                 // 用户名已存在
    AOR_NAME_NOT_EXIST,                                     // 账号不存在
    AOR_DB_INTERNAL_ERROR                                   // 数据库内部错误
};

#define MAX_ACCOUNT_STR 16                                  // 账号名/密码最大字符长度

/// 账号管理器，负责账号增删改查、SRP6 密码校验及权限等级查询
class AccountMgr
{
    public:
        AccountMgr();
        ~AccountMgr();

        /// 创建账号（默认资料片）
        AccountOpResult CreateAccount(std::string username, std::string password) const;
        /// 创建账号并指定资料片版本
        AccountOpResult CreateAccount(std::string username, std::string password, uint32 expansion) const;
        /// 删除指定账号及其关联角色与教程数据
        AccountOpResult DeleteAccount(uint32 accid) const;
        /// 修改账号用户名及对应密码验证器
        AccountOpResult ChangeUsername(uint32 accid, std::string new_uname, std::string new_passwd) const;
        /// 修改账号密码并重置 SRP6 盐值与验证器
        AccountOpResult ChangePassword(uint32 accid, std::string new_passwd) const;
        /// 校验明文密码是否匹配账号 SRP6 验证器
        bool CheckPassword(uint32 accid, std::string passwd) const;

        /// 根据用户名查询账号 ID
        uint32 GetId(std::string username) const;
        /// 查询账号的 GM 安全权限等级
        static AccountTypes GetSecurity(uint32 acc_id);
        /// 根据账号 ID 获取用户名
        bool GetName(uint32 acc_id, std::string& name) const;
        /// 统计该账号在当前服务器的角色数量
        uint32 GetCharactersCount(uint32 acc_id) const;
        /// 计算 "USERNAME:PASSWORD" 的 SHA1 十六进制摘要
        std::string CalculateShaPassHash(std::string& name, std::string& password) const;
        /// 获取账号标志位（flags）
        uint32 GetFlags(uint32 acc_id) const;

        /// 将 UTF-8 字符串规范化为大写（仅转换拉丁字母）并校验长度
        static bool normalizeString(std::string& utf8str);
};

#define sAccountMgr MaNGOS::Singleton<AccountMgr>::Instance()
#endif
