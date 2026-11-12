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

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "World/World.h"
#include "Entities/Player.h"
#include "Server/Opcodes.h"
#include "Chat/Chat.h"
#include "Globals/ObjectAccessor.h"
#include "Tools/Language.h"
#include "Accounts/AccountMgr.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "SystemConfig.h"
#include "revision.h"
#include "Util/Util.h"

/// 处理 `.help` 命令：显示指定命令或顶层命令帮助
bool ChatHandler::HandleHelpCommand(char* args)
{
    if (!*args)
    {
        ShowHelpForCommand(getCommandTable(), "help");
        ShowHelpForCommand(getCommandTable(), "");
    }
    else
    {
        if (!ShowHelpForCommand(getCommandTable(), args))
            SendSysMessage(LANG_NO_CMD);
    }

    return true;
}

/// 处理 `.commands` 命令：列出当前权限可用的全部命令
bool ChatHandler::HandleCommandsCommand(char* /*args*/)
{
    ShowHelpForCommand(getCommandTable(), "");
    return true;
}

/// 处理 `.account` 命令：显示当前账号权限等级
bool ChatHandler::HandleAccountCommand(char* args)
{
    // let show subcommands at unexpected data in args | 附带多余参数时返回 false 以显示子命令帮助
    if (*args)
        return false;

    AccountTypes gmlevel = GetAccessLevel();
    PSendSysMessage(LANG_ACCOUNT_LEVEL, uint32(gmlevel));
    return true;
}

/// 处理 `.start` 命令：脱困传送回绑定出生点
bool ChatHandler::HandleStartCommand(char* /*args*/)
{
    Player* chr = m_session->GetPlayer();

    if (chr->IsTaxiFlying())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        SetSentErrorMessage(true);
        return false;
    }

    if (chr->IsInCombat())
    {
        SendSysMessage(LANG_YOU_IN_COMBAT);
        SetSentErrorMessage(true);
        return false;
    }

    // cast spell Stuck | 施放“角色卡死”法术（7355）
    chr->CastSpell(chr, 7355, TRIGGERED_NONE);
    return true;
}

/// 处理 `.server info` 命令：显示版本、数据库版本、在线与排队人数及运行时间
bool ChatHandler::HandleServerInfoCommand(char* /*args*/)
{
    uint32 activeClientsNum = sWorld.GetActiveSessionCount();
    uint32 queuedClientsNum = sWorld.GetQueuedSessionCount();
    uint32 maxActiveClientsNum = sWorld.GetMaxActiveSessionCount();
    uint32 maxQueuedClientsNum = sWorld.GetMaxQueuedSessionCount();
    std::string str = secsToTimeString(sWorld.GetUptime());

    char const* full;
    if (m_session)
        full = _FULLVERSION(REVISION_DATE, "|cffffffff|Hurl:" REVISION_ID "|h" REVISION_ID "|h|r");
    else
        full = _FULLVERSION(REVISION_DATE, REVISION_ID);
    SendSysMessage(full);

    PSendSysMessage(LANG_USING_WORLD_DB, sWorld.GetDBVersion());
    PSendSysMessage(LANG_USING_EVENT_AI, sWorld.GetCreatureEventAIVersion());
    PSendSysMessage(LANG_CONNECTED_USERS, activeClientsNum, maxActiveClientsNum, queuedClientsNum, maxQueuedClientsNum);
    PSendSysMessage(LANG_UPTIME, str.c_str());

    return true;
}

/// 处理 `.dismount` 命令：强制解除坐骑状态
bool ChatHandler::HandleDismountCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();

    // If player is not mounted, so go out :) | 若未处于骑乘状态则提示并返回
    if (!player->IsMounted())
    {
        SendSysMessage(LANG_CHAR_NON_MOUNTED);
        SetSentErrorMessage(true);
        return false;
    }

    if (player->IsTaxiFlying())
    {
        SendSysMessage(LANG_YOU_IN_FLIGHT);
        SetSentErrorMessage(true);
        return false;
    }

    player->Unmount();
    player->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
    return true;
}

/// 处理 `.save` 命令：保存角色数据到数据库（普通玩家受存盘冷却限制以防刷档）
bool ChatHandler::HandleSaveCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();

    // save GM account without delay and output message (testing, etc) | GM 账号无延迟立即存盘并输出提示
    if (GetAccessLevel() > SEC_PLAYER)
    {
        player->SaveToDB();
        SendSysMessage(LANG_PLAYER_SAVED);
        return true;
    }

    // save or plan save after 20 sec (logout delay) if current next save time more this value and _not_ output any messages to prevent cheat planning | 普通玩家距下次自动存盘超过 20 秒才允许手动存盘且不回显提示
    uint32 save_interval = sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE);
    if (save_interval == 0 || (save_interval > 20 * IN_MILLISECONDS && player->GetSaveTimer() <= save_interval - 20 * IN_MILLISECONDS))
        player->SaveToDB();

    return true;
}

/// 处理 `.gmlist` 命令：列出当前在线的 GM 及其密语接收状态
bool ChatHandler::HandleGMListIngameCommand(char* /*args*/)
{
    std::list< std::pair<std::string, bool> > names;

    {
        HashMapHolder<Player>::ReadGuard g(HashMapHolder<Player>::GetLock());
        HashMapHolder<Player>::MapType& m = sObjectAccessor.GetPlayers();
        for (HashMapHolder<Player>::MapType::const_iterator itr = m.begin(); itr != m.end(); ++itr)
        {
            Player* player = itr->second;
            AccountTypes security = player->GetSession()->GetSecurity();
            if ((player->IsGameMaster() || (security > SEC_PLAYER && security <= (AccountTypes)sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_IN_GM_LIST))) &&
                (!m_session || player->IsVisibleGloballyFor(m_session->GetPlayer())))
                names.push_back(std::make_pair<std::string, bool>(GetNameLink(player), player->isAcceptWhispers()));
        }
    }

    if (!names.empty())
    {
        SendSysMessage(LANG_GMS_ON_SRV);

        char const* accepts = GetMangosString(LANG_GM_ACCEPTS_WHISPER);
        char const* not_accept = GetMangosString(LANG_GM_NO_WHISPER);
        for (std::list<std::pair< std::string, bool> >::const_iterator iter = names.begin(); iter != names.end(); ++iter)
            PSendSysMessage("%s - %s", iter->first.c_str(), iter->second ? accepts : not_accept);
    }
    else
        SendSysMessage(LANG_GMS_NOT_LOGGED);

    return true;
}

/// 处理 `.account password` 命令：修改当前登录账号的密码
bool ChatHandler::HandleAccountPasswordCommand(char* args)
{
    // allow use from RA, but not from console (not have associated account id) | 允许远程管理使用，但控制台无关联账号不可用
    if (!GetAccountId())
    {
        SendSysMessage(LANG_RA_ONLY_COMMAND);
        SetSentErrorMessage(true);
        return false;
    }

    // allow or quoted string with possible spaces or literal without spaces | 支持带引号（可含空格）或不带引号的密码参数
    char* old_pass = ExtractQuotedOrLiteralArg(&args);
    char* new_pass = ExtractQuotedOrLiteralArg(&args);
    char* new_pass_c = ExtractQuotedOrLiteralArg(&args);

    if (!old_pass || !new_pass || !new_pass_c)
        return false;

    std::string password_old = old_pass;
    std::string password_new = new_pass;
    std::string password_new_c = new_pass_c;

    if (password_new != password_new_c)
    {
        SendSysMessage(LANG_NEW_PASSWORDS_NOT_MATCH);
        SetSentErrorMessage(true);
        return false;
    }

    if (!sAccountMgr.CheckPassword(GetAccountId(), password_old))
    {
        SendSysMessage(LANG_COMMAND_WRONGOLDPASSWORD);
        SetSentErrorMessage(true);
        return false;
    }

    AccountOpResult result = sAccountMgr.ChangePassword(GetAccountId(), password_new);

    switch (result)
    {
        case AOR_OK:
            SendSysMessage(LANG_COMMAND_PASSWORD);
            break;
        case AOR_PASS_TOO_LONG:
            SendSysMessage(LANG_PASSWORD_TOO_LONG);
            SetSentErrorMessage(true);
            return false;
        case AOR_NAME_NOT_EXIST:                            // not possible case, don't want get account name for output | 理论不可达分支，避免泄露账号名
        default:
            SendSysMessage(LANG_COMMAND_NOTCHANGEPASSWORD);
            SetSentErrorMessage(true);
            return false;
    }

    // OK, but avoid normal report for hide passwords, but log use command for anyone | 脱敏记录命令日志并抑制默认命令回显，防止日志泄露明文密码
    LogCommand(".account password *** *** ***");
    SetSentErrorMessage(true);
    return false;
}

/// 处理 `.account lock` 命令：开启或关闭账号 IP 锁定
bool ChatHandler::HandleAccountLockCommand(char* args)
{
    // allow use from RA, but not from console (not have associated account id) | 允许远程管理使用，但控制台无关联账号不可用
    if (!GetAccountId())
    {
        SendSysMessage(LANG_RA_ONLY_COMMAND);
        SetSentErrorMessage(true);
        return false;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
    {
        LoginDatabase.PExecute("UPDATE account SET locked = '1' WHERE id = '%u'", GetAccountId());
        PSendSysMessage(LANG_COMMAND_ACCLOCKLOCKED);
    }
    else
    {
        LoginDatabase.PExecute("UPDATE account SET locked = '0' WHERE id = '%u'", GetAccountId());
        PSendSysMessage(LANG_COMMAND_ACCLOCKUNLOCKED);
    }

    return true;
}

/// Display the 'Message of the day' for the realm | 显示服务器今日公告 (MOTD)
bool ChatHandler::HandleServerMotdCommand(char* /*args*/)
{
    PSendSysMessage(LANG_MOTD_CURRENT, sWorld.GetMotd());
    return true;
}

/// 处理 `.whisperrestriction` 命令：查询或切换密语限制开关（仅限好友/队友/公会成员）
bool ChatHandler::HandleWhisperRestrictionCommand(char* args)
{
    if (!*args)
    {
        PSendSysMessage("Whisper restriction is %s.", m_session->GetPlayer()->isEnabledWhisperRestriction() ? "ON" : "OFF");
        return true;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    m_session->GetPlayer()->SetWhisperRestriction(value);
    PSendSysMessage("Whisper restriction is now %s.", value ? "ON. Only friends, group members, or guildmates may whisper you." : "OFF");

    return true;
}
