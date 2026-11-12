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

#include "Accounts/AccountMgr.h"
#include "Database/DatabaseEnv.h"
#include "Globals/ObjectAccessor.h"
#include "Entities/ObjectGuid.h"
#include "Entities/Player.h"
#include "Policies/Singleton.h"
#include "Util/Util.h"
#include "Auth/CryptoHash.h"
#include "Auth/SRP6.h"

extern DatabaseType LoginDatabase;

INSTANTIATE_SINGLETON_1(AccountMgr);

AccountMgr::AccountMgr()
{}

AccountMgr::~AccountMgr()
{}

/// 创建账号（使用数据库默认资料片等级）
AccountOpResult AccountMgr::CreateAccount(std::string username, std::string password) const
{
    if (utf8length(username) > MAX_ACCOUNT_STR)
        return AOR_NAME_TOO_LONG;                           // username's too long (用户名过长)

    normalizeString(username);
    normalizeString(password);

    if (GetId(username))
    {
        return AOR_NAME_ALREADY_EXIST;                       // username does already exist (用户名已存在)
    }

    // 计算 SRP6 盐值(s)与验证器(v)
    SRP6 srp;

    srp.CalculateVerifier(CalculateShaPassHash(username, password));
    const char* s_hex = srp.GetSalt().AsHexStr();
    const char* v_hex = srp.GetVerifier().AsHexStr();

    std::string safe_username = username;
    LoginDatabase.escape_string(safe_username);

    bool update_sv = LoginDatabase.PExecute(
        "INSERT INTO account(username,v,s,joindate) VALUES('%s','%s','%s'," _NOW_ ")",
            safe_username.c_str(), v_hex, s_hex);
    OPENSSL_free((void*)s_hex);
    OPENSSL_free((void*)v_hex);

    if (!update_sv)
        return AOR_DB_INTERNAL_ERROR;                       // unexpected error (数据库写入失败)
    // 为新账号补充所有服务器分区的 realmcharacters 初始记录
    LoginDatabase.Execute(
        "INSERT INTO realmcharacters (realmid, acctid, numchars) SELECT realmlist.id, account.id, 0 FROM realmlist,account LEFT JOIN realmcharacters ON acctid=account.id WHERE acctid IS NULL");

    return AOR_OK;                                          // everything's fine (创建成功)
}

/// 创建账号并显式指定资料片版本
AccountOpResult AccountMgr::CreateAccount(std::string username, std::string password, uint32 expansion) const
{
    if (utf8length(username) > MAX_ACCOUNT_STR)
        return AOR_NAME_TOO_LONG;                           // username's too long (用户名过长)

    normalizeString(username);
    normalizeString(password);

    if (GetId(username))
    {
        return AOR_NAME_ALREADY_EXIST;                       // username does already exist (用户名已存在)
    }

    SRP6 srp;

    srp.CalculateVerifier(CalculateShaPassHash(username, password));
    const char* s_hex = srp.GetSalt().AsHexStr();
    const char* v_hex = srp.GetVerifier().AsHexStr();

    std::string safe_username = username;
    LoginDatabase.escape_string(safe_username);

    bool update_sv = LoginDatabase.PExecute(
        "INSERT INTO account(username,v,s,joindate,expansion) VALUES('%s','%s','%s'," _NOW_ ",%u)",
            safe_username.c_str(), v_hex, s_hex, expansion);

    OPENSSL_free((void*)s_hex);
    OPENSSL_free((void*)v_hex);

    if (!update_sv)
        return AOR_DB_INTERNAL_ERROR;                       // unexpected error (数据库写入失败)
    LoginDatabase.Execute("INSERT INTO realmcharacters (realmid, acctid, numchars) SELECT realmlist.id, account.id, 0 FROM realmlist,account LEFT JOIN realmcharacters ON acctid=account.id WHERE acctid IS NULL");

    return AOR_OK;                                          // everything's fine (创建成功)
}

/// 删除账号：先踢出并删除其下全部角色，再清理登录库账号记录
AccountOpResult AccountMgr::DeleteAccount(uint32 accid) const
{
    auto queryResult = LoginDatabase.PQuery("SELECT 1 FROM account WHERE id='%u'", accid);
    if (!queryResult)
        return AOR_NAME_NOT_EXIST;                          // account doesn't exist (账号不存在)

    // existing characters list (查询并删除该账号下的现有角色)
    queryResult = CharacterDatabase.PQuery("SELECT guid FROM characters WHERE account='%u'", accid);
    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();
            uint32 guidlo = fields[0].GetUInt32();
            ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidlo);

            // kick if player currently (若角色当前在线则先踢出游戏)
            ObjectAccessor::KickPlayer(guid);
            Player::DeleteFromDB(guid, accid, false);       // no need to update realm characters (即将删除整条账号记录，无需更新角色计数表)
        }
        while (queryResult->NextRow());
    }

    // table realm specific but common for all characters of account for realm (清理该账号在当前角色库的新手教程记录)
    CharacterDatabase.PExecute("DELETE FROM character_tutorial WHERE account = '%u'", accid);

    LoginDatabase.BeginTransaction();

    bool res =
        LoginDatabase.PExecute("DELETE FROM account WHERE id='%u'", accid) &&
        LoginDatabase.PExecute("DELETE FROM realmcharacters WHERE acctid='%u'", accid);

    LoginDatabase.CommitTransaction();

    if (!res)
        return AOR_DB_INTERNAL_ERROR;                       // unexpected error; (事务执行失败)

    return AOR_OK;
}

/// 修改用户名及新密码对应的 SRP6 验证信息
AccountOpResult AccountMgr::ChangeUsername(uint32 accid, std::string new_uname, std::string new_passwd) const
{
    auto queryResult = LoginDatabase.PQuery("SELECT 1 FROM account WHERE id='%u'", accid);
    if (!queryResult)
        return AOR_NAME_NOT_EXIST;                          // account doesn't exist (账号不存在)

    if (utf8length(new_uname) > MAX_ACCOUNT_STR)
        return AOR_NAME_TOO_LONG;

    if (utf8length(new_passwd) > MAX_ACCOUNT_STR)
        return AOR_PASS_TOO_LONG;

    normalizeString(new_uname);
    normalizeString(new_passwd);

    SRP6 srp;

    srp.CalculateVerifier(CalculateShaPassHash(new_uname, new_passwd));

    std::string safe_new_uname = new_uname;
    LoginDatabase.escape_string(safe_new_uname);

    const char* s_hex = srp.GetSalt().AsHexStr();
    const char* v_hex = srp.GetVerifier().AsHexStr();

    bool update_sv = LoginDatabase.PExecute(
        "UPDATE account SET v='%s',s='%s',username='%s' WHERE id='%u'",
            v_hex, s_hex, safe_new_uname.c_str(), accid);

    OPENSSL_free((void*)s_hex);
    OPENSSL_free((void*)v_hex);

    if (!update_sv)
        return AOR_DB_INTERNAL_ERROR;                       // unexpected error (数据库更新失败)

    return AOR_OK;
}

/// 修改账号密码并更新 SRP6 盐值与验证器
AccountOpResult AccountMgr::ChangePassword(uint32 accid, std::string new_passwd) const
{
    std::string username;

    if (!GetName(accid, username))
        return AOR_NAME_NOT_EXIST;                          // account doesn't exist (账号不存在)

    if (utf8length(new_passwd) > MAX_ACCOUNT_STR)
        return AOR_PASS_TOO_LONG;

    normalizeString(username);
    normalizeString(new_passwd);

    SRP6 srp;

    srp.CalculateVerifier(CalculateShaPassHash(username, new_passwd));

    const char* s_hex = srp.GetSalt().AsHexStr();
    const char* v_hex = srp.GetVerifier().AsHexStr();

    bool update_sv = LoginDatabase.PExecute(
        "UPDATE account SET v='%s', s='%s' WHERE id='%u'",
            v_hex, s_hex, accid);

    OPENSSL_free((void*)s_hex);
    OPENSSL_free((void*)v_hex);

    // also reset s and v to force update at next realmd login (同步重置 s 和 v 以在下次 realmd 登录时生效)
    if (!update_sv)
        return AOR_DB_INTERNAL_ERROR;                       // unexpected error (数据库更新失败)

    return AOR_OK;
}

/// 根据用户名查询账号 ID，不存在返回 0
uint32 AccountMgr::GetId(std::string username) const
{
    LoginDatabase.escape_string(username);
    auto queryResult = LoginDatabase.PQuery("SELECT id FROM account WHERE username = '%s'", username.c_str());
    if (!queryResult)
        return 0;
    uint32 id = (*queryResult)[0].GetUInt32();
    return id;
}

/// 查询账号的 GM 安全权限等级，未找到默认返回普通玩家权限 SEC_PLAYER
AccountTypes AccountMgr::GetSecurity(uint32 acc_id)
{
    auto queryResult = LoginDatabase.PQuery("SELECT gmlevel FROM account WHERE id = '%u'", acc_id);
    if (queryResult)
    {
        AccountTypes sec = AccountTypes((*queryResult)[0].GetInt32());
        return sec;
    }

    return SEC_PLAYER;
}

/// 根据账号 ID 查询用户名
bool AccountMgr::GetName(uint32 acc_id, std::string& name) const
{
    auto queryResult = LoginDatabase.PQuery("SELECT username FROM account WHERE id = '%u'", acc_id);
    if (queryResult)
    {
        name = (*queryResult)[0].GetCppString();
        return true;
    }

    return false;
}

/// 查询该账号在当前角色库下的角色总数
uint32 AccountMgr::GetCharactersCount(uint32 acc_id) const
{
    // check character count (统计角色表中的记录数)
    auto queryResult = CharacterDatabase.PQuery("SELECT COUNT(guid) FROM characters WHERE account = '%u'", acc_id);
    if (queryResult)
    {
        Field* fields = queryResult->Fetch();
        uint32 charcount = fields[0].GetUInt32();
        return charcount;
    }
    return 0;
}

/// 使用数据库中的盐值 s 与验证器 v 校验输入密码
bool AccountMgr::CheckPassword(uint32 accid, std::string passwd) const
{
    std::string username;
    if (!GetName(accid, username))
        return false;

    normalizeString(passwd);
    normalizeString(username);

    auto queryResult = LoginDatabase.PQuery("SELECT s, v FROM account WHERE id='%u'", accid);
    if (queryResult)
    {
        Field* fields = queryResult->Fetch();
        SRP6 srp;

        bool calcv = srp.CalculateVerifier(
            CalculateShaPassHash(username, passwd), fields[0].GetCppString().c_str());

        if (calcv && srp.ProofVerifier(fields[1].GetCppString()))
        {
            return true;
        }
    }

    return false;
}

/// 规范化账号名或密码字符串：转为大写（仅转换拉丁字符）并检查最大长度
bool AccountMgr::normalizeString(std::string& utf8str)
{
    std::wstring wstr_buf;
    if (!Utf8toWStr(utf8str, wstr_buf))
        return false;

    if (wstr_buf.size() > MAX_ACCOUNT_STR)
        return false;

    std::transform(wstr_buf.begin(), wstr_buf.end(), wstr_buf.begin(), wcharToUpperOnlyLatin);

    return WStrToUtf8(wstr_buf, utf8str);
}

/// 计算 "用户名:密码" 的 SHA1 摘要并编码为十六进制字符串
std::string AccountMgr::CalculateShaPassHash(std::string& name, std::string& password) const
{
    Sha1Hash sha;
    sha.Initialize();
    sha.UpdateData(name);
    sha.UpdateData(":");
    sha.UpdateData(password);
    sha.Finalize();

    std::string encoded;
    hexEncodeByteArray(sha.GetDigest(), Sha1Hash::GetLength(), encoded);

    return encoded;
}

/// 查询账号标志位（flags）
uint32 AccountMgr::GetFlags(uint32 acc_id) const
{
    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery("SELECT flags FROM account WHERE id = '%u'", acc_id));
    if (result)
        return (*result)[0].GetInt32();

    return 0;
}
