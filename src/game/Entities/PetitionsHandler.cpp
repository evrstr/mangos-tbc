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
#include "Tools/Language.h"
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"
#include "Log/Log.h"
#include "Server/Opcodes.h"
#include "Guilds/Guild.h"
#include "Guilds/GuildMgr.h"
#include "Arena/ArenaTeam.h"
#include "Anticheat/Anticheat.hpp"

/*enum PetitionType // dbc data
{
    PETITION_TYPE_GUILD      = 1,
    PETITION_TYPE_ARENA_TEAM = 3
};*/

// Charters ID in item_template (公会与竞技场登记表物品 ID、价格及图标显示常量)
#define GUILD_CHARTER               5863
#define GUILD_CHARTER_COST          1000                    // 10 S (10 银币)
#define ARENA_TEAM_CHARTER_2v2      23560
#define ARENA_TEAM_CHARTER_2v2_COST 800000                  // 80 G (80 金币)
#define ARENA_TEAM_CHARTER_3v3      23561
#define ARENA_TEAM_CHARTER_3v3_COST 1200000                 // 120 G (120 金币)
#define ARENA_TEAM_CHARTER_5v5      23562
#define ARENA_TEAM_CHARTER_5v5_COST 2000000                 // 200 G (200 金币)
#define CHARTER_DISPLAY_ID          16161

// 处理购买公会或竞技场战队登记表请求（CMSG_PETITION_BUY）
void WorldSession::HandlePetitionBuyOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_BUY");
    recv_data.hexlike();

    ObjectGuid guidNPC;
    uint32 clientIndex;                                     // 1 for guild and arenaslot+1 for arenas in client (公会为 1，竞技场为 arenaslot+1)
    std::string name;

    recv_data >> guidNPC;                                   // NPC GUID
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint64>();                          // 0
    recv_data >> name;                                      // name (拟创建的公会或战队名称)
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint16>();                          // 0
    recv_data.read_skip<uint8>();                           // 0

    recv_data >> clientIndex;                               // index
    recv_data.read_skip<uint32>();                          // 0

    DEBUG_LOG("Petitioner %s tried sell petition: name %s", guidNPC.GetString().c_str(), name.c_str());

    // prevent cheating (校验 NPC 交互权限)
    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guidNPC, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionBuyOpcode - %s not found or you can't interact with him.", guidNPC.GetString().c_str());
        return;
    }

    uint32 charterid;
    uint32 cost;
    uint32 type;
    if (pCreature->isTabardDesigner())
    {
        // if tabard designer, then trying to buy a guild charter.
        // do not let if already in guild.
        // 徽章设计师出售公会登记表（type=9）；已有公会的玩家禁止购买
        if (_player->GetGuildId())
            return;

        charterid = GUILD_CHARTER;
        cost = GUILD_CHARTER_COST;
        type = 9;
    }
    else
    {
        // TODO: find correct opcode (竞技场组织者需满级方可购买对应赛制登记表)
        if (_player->GetLevel() < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            SendNotification(LANG_ARENA_ONE_TOOLOW, sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
            return;
        }

        switch (clientIndex)                                // arenaSlot+1 as received from client (1 from 3 case) (客户端传入 1/2/3 对应 2v2/3v3/5v5)
        {
            case 1:
                charterid = ARENA_TEAM_CHARTER_2v2;
                cost = ARENA_TEAM_CHARTER_2v2_COST;
                type = 2;                                   // 2v2
                break;
            case 2:
                charterid = ARENA_TEAM_CHARTER_3v3;
                cost = ARENA_TEAM_CHARTER_3v3_COST;
                type = 3;                                   // 3v3
                break;
            case 3:
                charterid = ARENA_TEAM_CHARTER_5v5;
                cost = ARENA_TEAM_CHARTER_5v5_COST;
                type = 5;                                   // 5v5
                break;
            default:
                DEBUG_LOG("unknown selection at buy arena petition: %u", clientIndex);
                return;
        }

        if (_player->GetArenaTeamId(clientIndex - 1))       // arenaSlot+1 as received from client (若已在该赛制战队中则拒绝)
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ALREADY_IN_ARENA_TEAM);
            return;
        }
    }

    // Check guild petition name (use whisper type - 6) (反作弊/反垃圾广告名称检测)
    if (sAnticheatLib->ValidateGuildName(name))
    {
        sLog.outBasic("Attempt to create guild petition with spam name \"%s\"", name.c_str());
        SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_INVALID);
        return;
    }

    // 校验公会或竞技场战队名称是否重复、保留或非法
    if (type == 9)
    {
        if (sGuildMgr.GetGuildByName(name))
        {
            SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
            return;
        }
        if (sObjectMgr.IsReservedName(name) || !ObjectMgr::IsValidCharterName(name))
        {
            SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_INVALID);
            return;
        }
    }
    else
    {
        if (sObjectMgr.GetArenaTeamByName(name))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ARENA_TEAM_NAME_EXISTS_S);
            return;
        }
        if (sObjectMgr.IsReservedName(name) || !ObjectMgr::IsValidCharterName(name))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ARENA_TEAM_NAME_INVALID);
            return;
        }
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(charterid);
    if (!pProto)
    {
        _player->SendBuyError(BuyResult::CANT_FIND_ITEM, nullptr, 0);
        return;
    }

    if (_player->GetMoney() < cost)
    {
        // player hasn't got enough money (金币不足)
        _player->SendBuyError(BuyResult::NOT_ENOUGHT_MONEY, pCreature, 0);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, charterid, pProto->BuyCount);
    if (msg != EQUIP_ERR_OK)
    {
        _player->SendEquipError(msg, nullptr, nullptr, charterid);
        return;
    }

    _player->ModifyMoney(-(int32)cost);
    Item* charter = _player->StoreNewItem(dest, charterid, true);
    if (!charter)
        return;

    charter->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1, charter->GetGUIDLow());
    // ITEM_FIELD_ENCHANTMENT_1_1 is guild/arenateam id (物品附魔槽 1 存储登记表低位 GUID)
    // ITEM_FIELD_ENCHANTMENT_1_1+1 is current signatures count (showed on item)
    charter->SetState(ITEM_CHANGED, _player);
    _player->SendNewItem(charter, 1, true, false);

    // a petition is invalid, if both the owner and the type matches
    // we checked above, if this player is in an arenateam, so this must be data corruption
    // 清理该玩家同类型残留的无效登记表记录并写入新登记表
    auto queryResult = CharacterDatabase.PQuery("SELECT petitionguid FROM petition WHERE ownerguid = '%u'  AND type = '%u'", _player->GetGUIDLow(), type);

    std::ostringstream ssInvalidPetitionGUIDs;

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();
            ssInvalidPetitionGUIDs << "'" << fields[0].GetUInt32() << "' , ";
        }
        while (queryResult->NextRow());
    }

    // delete petitions with the same guid as this one (同时删除同 GUID 的旧残留登记表及签名)
    ssInvalidPetitionGUIDs << "'" << charter->GetGUIDLow() << "'";

    DEBUG_LOG("Invalid petition GUIDs: %s", ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.escape_string(name);
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM petition WHERE petitionguid IN ( %s )",  ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.PExecute("DELETE FROM petition_sign WHERE petitionguid IN ( %s )", ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.PExecute("INSERT INTO petition (ownerguid, petitionguid, name, type) VALUES ('%u', '%u', '%s', '%u')",
                               _player->GetGUIDLow(), charter->GetGUIDLow(), name.c_str(), type);
    CharacterDatabase.CommitTransaction();
}

// 处理查看登记表当前签名列表请求（CMSG_PETITION_SHOW_SIGNATURES）
void WorldSession::HandlePetitionShowSignOpcode(WorldPacket& recv_data)
{
    // ok
    DEBUG_LOG("Received opcode CMSG_PETITION_SHOW_SIGNATURES");
    // recv_data.hexlike();

    uint8 signs = 0;
    ObjectGuid petitionguid;
    recv_data >> petitionguid;                              // petition guid (登记表物品 GUID)

    // solve (possible) some strange compile problems with explicit use GUID_LOPART(petitionguid) at some GCC versions (wrong code optimization in compiler?)
    uint32 petitionguid_low = petitionguid.GetCounter();

    auto queryResult = CharacterDatabase.PQuery("SELECT type FROM petition WHERE petitionguid = '%u'", petitionguid_low);
    if (!queryResult)
    {
        sLog.outError("any petition on server...");
        return;
    }
    Field* fields = queryResult->Fetch();
    uint32 type = fields[0].GetUInt32();

    // if guild petition and has guild => error, return; (已是公会成员时不可查看公会登记表签名)
    if (type == 9 && _player->GetGuildId())
        return;

    queryResult = CharacterDatabase.PQuery("SELECT playerguid FROM petition_sign WHERE petitionguid = '%u'", petitionguid_low);

    // queryResult==nullptr also correct in case no sign yet (尚无签名时 queryResult 为空亦属正常)
    if (queryResult)
        signs = (uint8)queryResult->GetRowCount();

    DEBUG_LOG("CMSG_PETITION_SHOW_SIGNATURES petition: %s", petitionguid.GetString().c_str());

    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + 1 + signs * 12));
    data << ObjectGuid(petitionguid);                       // petition guid
    data << _player->GetObjectGuid();                       // owner guid (持有者 GUID)
    data << uint32(petitionguid_low);                       // guild guid (in mangos always same as GUID_LOPART(petitionguid)
    data << uint8(signs);                                   // sign's count (已签名人数)

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = queryResult->Fetch();
        ObjectGuid signerGuid = ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << ObjectGuid(signerGuid);                     // Player GUID (签名者 GUID)
        data << uint32(0);                                  // there 0 ...

        queryResult->NextRow();
    }
    SendPacket(data);
}

// 处理客户端查询登记表详情请求（CMSG_PETITION_QUERY）
void WorldSession::HandlePetitionQueryOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_QUERY");
    // recv_data.hexlike();

    uint32 guildguid;
    ObjectGuid petitionguid;
    recv_data >> guildguid;                                 // in mangos always same as GUID_LOPART(petitionguid)
    recv_data >> petitionguid;                              // petition guid
    DEBUG_LOG("CMSG_PETITION_QUERY Petition %s Guild GUID %u", petitionguid.GetString().c_str(), guildguid);

    SendPetitionQueryOpcode(petitionguid);
}

// 从数据库查询登记表所有者、名称与类型并回复 SMSG_PETITION_QUERY_RESPONSE
void WorldSession::SendPetitionQueryOpcode(ObjectGuid petitionguid) const
{
    uint32 petitionLowGuid = petitionguid.GetCounter();

    auto queryResult = CharacterDatabase.PQuery(
                              "SELECT ownerguid, name, "
                              "  (SELECT COUNT(playerguid) FROM petition_sign WHERE petition_sign.petitionguid = '%u') AS signs, "
                              "  type "
                              "FROM petition WHERE petitionguid = '%u'", petitionLowGuid, petitionLowGuid);

    if (!queryResult)
    {
        DEBUG_LOG("CMSG_PETITION_QUERY failed for petition (GUID: %u)", petitionLowGuid);
        return;
    }

    Field* fields = queryResult->Fetch();
    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
    std::string name = fields[1].GetCppString();
    //uint8 signs = fields[2].GetUInt8();
    uint32 type = fields[3].GetUInt32();

    WorldPacket data(SMSG_PETITION_QUERY_RESPONSE, (4 + 8 + name.size() + 1 + 1 + 4 * 13 + 2));
    data << uint32(petitionLowGuid);                        // guild/team guid (in mangos always same as GUID_LOPART(petition guid)
    data << ObjectGuid(ownerGuid);                          // charter owner guid (登记表创建者 GUID)
    data << name;                                           // name (guild/arena team) (公会或战队名称)
    data << uint8(0);                                       // 1
    if (type == 9)
    {
        data << uint32(9);
        data << uint32(9);
        data << uint32(0);                                  // bypass client - side limitation, a different value is needed here for each petition (公会所需签名数 9)
    }
    else
    {
        data << uint32(type - 1);
        data << uint32(type - 1);
        data << uint32(type);                               // bypass client - side limitation, a different value is needed here for each petition (竞技场不同赛制需传入各自 type 以绕过客户端限制)
    }
    data << uint32(0);                                      // 5
    data << uint32(0);                                      // 6
    data << uint32(0);                                      // 7
    data << uint32(0);                                      // 8
    data << uint16(0);                                      // 9 2 bytes field
    data << uint32(0);                                      // 10
    data << uint32(0);                                      // 11
    data << uint32(0);                                      // 13 count of next strings?
    data << uint32(0);                                      // 14

    if (type == 9)
        data << uint32(0);                                  // 15 0 - guild, 1 - arena team (0 为公会，1 为竞技场战队)
    else
        data << uint32(1);

    SendPacket(data);
}

// 处理登记表重命名请求（MSG_PETITION_RENAME）
void WorldSession::HandlePetitionRenameOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_RENAME");   // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    uint32 type;
    std::string newname;

    recv_data >> petitionGuid;                              // guid
    recv_data >> newname;                                   // new name

    Item* item = _player->GetItemByGuid(petitionGuid);
    if (!item)
        return;

    auto queryResult = CharacterDatabase.PQuery("SELECT type FROM petition WHERE petitionguid = '%u'", petitionGuid.GetCounter());

    if (queryResult)
    {
        Field* fields = queryResult->Fetch();
        type = fields[0].GetUInt32();
    }
    else
    {
        DEBUG_LOG("CMSG_PETITION_QUERY failed for petition: %s", petitionGuid.GetString().c_str());
        return;
    }

    // 校验新名称是否重名或违反命名规则
    if (type == 9)
    {
        if (sGuildMgr.GetGuildByName(newname))
        {
            SendGuildCommandResult(GUILD_CREATE_S, newname, ERR_GUILD_NAME_EXISTS_S);
            return;
        }
        if (sObjectMgr.IsReservedName(newname) || !ObjectMgr::IsValidCharterName(newname))
        {
            SendGuildCommandResult(GUILD_CREATE_S, newname, ERR_GUILD_NAME_INVALID);
            return;
        }
    }
    else
    {
        if (sObjectMgr.GetArenaTeamByName(newname))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, newname, "", ERR_ARENA_TEAM_NAME_EXISTS_S);
            return;
        }
        if (sObjectMgr.IsReservedName(newname) || !ObjectMgr::IsValidCharterName(newname))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, newname, "", ERR_ARENA_TEAM_NAME_INVALID);
            return;
        }
    }

    std::string db_newname = newname;
    CharacterDatabase.escape_string(db_newname);
    CharacterDatabase.PExecute("UPDATE petition SET name = '%s' WHERE petitionguid = '%u'",
                               db_newname.c_str(), petitionGuid.GetCounter());

    DEBUG_LOG("Petition %s renamed to '%s'", petitionGuid.GetString().c_str(), newname.c_str());

    WorldPacket data(MSG_PETITION_RENAME, (8 + newname.size() + 1));
    data << ObjectGuid(petitionGuid);
    data << newname;
    SendPacket(data);
}

// 处理玩家在他人登记表上签名请求（CMSG_PETITION_SIGN）
void WorldSession::HandlePetitionSignOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_SIGN");    // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    uint8 unk;
    recv_data >> petitionGuid;                              // petition guid
    recv_data >> unk;

    uint32 petitionLowGuid = petitionGuid.GetCounter();

    auto queryResult = CharacterDatabase.PQuery(
                              "SELECT ownerguid, "
                              "  (SELECT COUNT(playerguid) FROM petition_sign WHERE petition_sign.petitionguid = '%u') AS signs, "
                              "  type "
                              "FROM petition WHERE petitionguid = '%u'", petitionLowGuid, petitionLowGuid);

    if (!queryResult)
    {
        sLog.outError("any petition on server...");
        return;
    }

    Field* fields = queryResult->Fetch();
    uint32 ownerLowGuid = fields[0].GetUInt32();
    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, ownerLowGuid);
    uint8 signs = fields[1].GetUInt8();
    uint32 type = fields[2].GetUInt32();

    if (ownerGuid == _player->GetObjectGuid())
        return;

    // not let enemies sign guild charter (未开启跨阵营公会交互时，敌对阵营不可签名)
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
            GetPlayer()->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(ownerGuid))
    {
        if (type != 9)
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", "", ERR_ARENA_TEAM_NOT_ALLIED);
        else
            SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    // 校验签名者等级、现有战队/公会及待处理邀请状态
    if (type != 9)
    {
        if (_player->GetLevel() < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", _player->GetName(), ERR_ARENA_TEAM_TARGET_TOO_LOW_S);
            return;
        }

        if (!IsArenaTypeValid(ArenaType(type)))
            return;

        uint8 slot = ArenaTeam::GetSlotByType(ArenaType(type));
        if (slot >= MAX_ARENA_SLOT)
            return;

        if (_player->GetArenaTeamId(slot))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", _player->GetName(), ERR_ALREADY_IN_ARENA_TEAM_S);
            return;
        }

        if (_player->GetArenaTeamIdInvited())
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", _player->GetName(), ERR_ALREADY_INVITED_TO_ARENA_TEAM_S);
            return;
        }
    }
    else
    {
        if (_player->GetGuildId())
        {
            SendGuildCommandResult(GUILD_INVITE_S, _player->GetName(), ERR_ALREADY_IN_GUILD_S);
            return;
        }
        if (_player->GetGuildIdInvited())
        {
            SendGuildCommandResult(GUILD_INVITE_S, _player->GetName(), ERR_ALREADY_INVITED_TO_GUILD_S);
            return;
        }
    }

    if (++signs > type)                                     // client signs maximum (超出签名上限)
        return;

    // client doesn't allow to sign petition two times by one character, but not check sign by another character from same account
    // not allow sign another player from already sign player account
    // 禁止同一账号下的多个角色重复签署同一份登记表（机器人除外）
#ifdef ENABLE_PLAYERBOTS
    if (!_player->GetPlayerbotAI())
    {
#endif
    queryResult = CharacterDatabase.PQuery("SELECT playerguid FROM petition_sign WHERE player_account = '%u' AND petitionguid = '%u'", GetAccountId(), petitionLowGuid);

    if (queryResult)
    {
        WorldPacket data(SMSG_PETITION_SIGN_RESULTS, (8 + 8 + 4));
        data << ObjectGuid(petitionGuid);
        data << ObjectGuid(_player->GetObjectGuid());
        data << uint32(PETITION_SIGN_ALREADY_SIGNED);

        // close at signer side (通知签名方关闭界面)
        SendPacket(data);

        // update for owner if online (若发起者在线则同步通知)
        if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
            owner->GetSession()->SendPacket(data);
        return;
    }
#ifdef ENABLE_PLAYERBOTS
    }
#endif

    CharacterDatabase.PExecute("INSERT INTO petition_sign (ownerguid,petitionguid, playerguid, player_account) VALUES ('%u', '%u', '%u','%u')",
                               ownerLowGuid, petitionLowGuid, _player->GetGUIDLow(), GetAccountId());

    DEBUG_LOG("PETITION SIGN: %s by %s", petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    WorldPacket data(SMSG_PETITION_SIGN_RESULTS, (8 + 8 + 4));
    data << ObjectGuid(petitionGuid);
    data << ObjectGuid(_player->GetObjectGuid());
    data << uint32(PETITION_SIGN_OK);

    // close at signer side (通知签名方签名成功)
    SendPacket(data);

    // update signs count on charter, required testing...
    // Item *item = _player->GetItemByGuid(petitionguid));
    // if(item)
    //    item->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1+1, signs);

    // update for owner if online (若发起者在线则通知更新签名状态)
    if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
        owner->GetSession()->SendPacket(data);
}

// 处理玩家拒绝签名请求（MSG_PETITION_DECLINE）并通知发起者
void WorldSession::HandlePetitionDeclineOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_DECLINE");  // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    recv_data >> petitionGuid;                              // petition guid

    DEBUG_LOG("Petition %s declined by %s", petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    uint32 petitionLowGuid = petitionGuid.GetCounter();

    auto queryResult = CharacterDatabase.PQuery("SELECT ownerguid FROM petition WHERE petitionguid = '%u'", petitionLowGuid);
    if (!queryResult)
        return;

    Field* fields = queryResult->Fetch();
    ObjectGuid ownerguid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());

    Player* owner = sObjectMgr.GetPlayer(ownerguid);
    if (owner)                                              // petition owner online (若发起者在线则回发拒绝通知)
    {
        WorldPacket data(MSG_PETITION_DECLINE, 8);
        data << _player->GetObjectGuid();
        owner->GetSession()->SendPacket(data);
    }
}

// 处理向目标玩家递交登记表邀请签名请求（CMSG_OFFER_PETITION）
void WorldSession::HandleOfferPetitionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_OFFER_PETITION");   // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    ObjectGuid playerGuid;
    uint32 junk;
    recv_data >> junk;                                      // this is not petition type! (客户端填充占位值，非登记表类型)
    recv_data >> petitionGuid;                              // petition guid
    recv_data >> playerGuid;                                // player guid

    Player* player = ObjectAccessor::FindPlayer(playerGuid);
    if (!player)
        return;

    /// Get petition type and check (查询登记表类型并校验双方阵营与目标资格)
    auto queryResult = CharacterDatabase.PQuery("SELECT type FROM petition WHERE petitionguid = '%u'", petitionGuid.GetCounter());
    if (!queryResult)
        return;

    Field* fields = queryResult->Fetch();
    uint32 type = fields[0].GetUInt32();

    DEBUG_LOG("OFFER PETITION: type %u petition %s to %s", type, petitionGuid.GetString().c_str(), playerGuid.GetString().c_str());

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && GetPlayer()->GetTeam() != player->GetTeam())
    {
        if (type != 9)
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", "", ERR_ARENA_TEAM_NOT_ALLIED);
        else
            SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (type != 9)
    {
        if (player->GetLevel() < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        {
            // player is too low level to join an arena team (目标等级未达到竞技场要求)
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", player->GetName(), ERR_ARENA_TEAM_TARGET_TOO_LOW_S);
            return;
        }

        if (!IsArenaTypeValid(ArenaType(type)))
            return;

        uint8 slot = ArenaTeam::GetSlotByType(ArenaType(type));
        if (slot >= MAX_ARENA_SLOT)
            return;

        if (player->GetArenaTeamId(slot))
        {
            // player is already in an arena team (目标已在同赛制竞技场战队中)
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, "", player->GetName(), ERR_ALREADY_IN_ARENA_TEAM_S);
            return;
        }

        if (player->GetArenaTeamIdInvited())
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_INVITE_SS, "", _player->GetName(), ERR_ALREADY_INVITED_TO_ARENA_TEAM_S);
            return;
        }
    }
    else
    {
        if (player->GetGuildId())
        {
            SendGuildCommandResult(GUILD_INVITE_S, _player->GetName(), ERR_ALREADY_IN_GUILD_S);
            return;
        }

        if (player->GetGuildIdInvited())
        {
            SendGuildCommandResult(GUILD_INVITE_S, _player->GetName(), ERR_ALREADY_INVITED_TO_GUILD_S);
            return;
        }
    }

    /// Get petition signs count (读取当前已收集的签名列表)
    uint8 signs = 0;
    queryResult = CharacterDatabase.PQuery("SELECT playerguid FROM petition_sign WHERE petitionguid = '%u'", petitionGuid.GetCounter());
    // queryResult==nullptr also correct charter without signs
    if (queryResult)
        signs = (uint8)queryResult->GetRowCount();

    /// Send response (向目标玩家发送签名窗口数据包)
    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + signs + signs * 12));
    data << ObjectGuid(petitionGuid);                       // petition guid
    data << ObjectGuid(_player->GetObjectGuid());           // owner guid
    data << uint32(petitionGuid.GetCounter());              // guild guid (in mangos always same as low part of petition guid)
    data << uint8(signs);                                   // sign's count

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = queryResult->Fetch();
        ObjectGuid signerGuid = ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << ObjectGuid(signerGuid);                     // Player GUID
        data << uint32(0);                                  // there 0 ...

        queryResult->NextRow();
    }

    player->GetSession()->SendPacket(data);
}

// 处理上交已签满的登记表以正式创建公会或竞技场战队（CMSG_TURN_IN_PETITION）
void WorldSession::HandleTurnInPetitionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_TURN_IN_PETITION"); // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;

    recv_data >> petitionGuid;

    DEBUG_LOG("Petition %s turned in by %s", petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    /// Collect petition info data (从数据库读取登记表基本信息)
    ObjectGuid ownerGuid;
    uint32 type;
    std::string name;

    // data
    auto queryResult = CharacterDatabase.PQuery("SELECT ownerguid, name, type FROM petition WHERE petitionguid = '%u'", petitionGuid.GetCounter());
    if (queryResult)
    {
        Field* fields = queryResult->Fetch();
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name = fields[1].GetCppString();
        type = fields[2].GetUInt32();
    }
    else
    {
        sLog.outError("CMSG_TURN_IN_PETITION: petition table not have data for guid %u!", petitionGuid.GetCounter());
        return;
    }

    if (type == 9)
    {
        if (_player->GetGuildId())
        {
            WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
            data << uint32(PETITION_TURN_ALREADY_IN_GUILD); // already in guild (已有公会)
            _player->GetSession()->SendPacket(data);
            return;
        }
    }
    else
    {
        if (!IsArenaTypeValid(ArenaType(type)))
            return;

        uint8 slot = ArenaTeam::GetSlotByType(ArenaType(type));
        if (slot >= MAX_ARENA_SLOT)
            return;

        if (_player->GetArenaTeamId(slot))
        {
            // data.Initialize(SMSG_TURN_IN_PETITION_RESULTS, 4);
            // data << (uint32)PETITION_TURN_ALREADY_IN_GUILD;   // already in guild
            //_player->GetSession()->SendPacket(data);
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ALREADY_IN_ARENA_TEAM);
            return;
        }
    }

    if (_player->GetObjectGuid() != ownerGuid)
        return;

    // signs (校验已收集签名数是否达到要求)
    queryResult = CharacterDatabase.PQuery("SELECT playerguid FROM petition_sign WHERE petitionguid = '%u'", petitionGuid.GetCounter());
    uint8 signs = queryResult ? (uint8)queryResult->GetRowCount() : 0;

    uint32 count = type == 9 ? sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS) : type - 1;
    if (signs < count)
    {
        WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
        data << uint32(PETITION_TURN_NEED_MORE_SIGNATURES); // need more signatures... (签名数量不足)
        SendPacket(data);
        return;
    }

    if (type == 9)
    {
        if (sGuildMgr.GetGuildByName(name))
        {
            SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
            return;
        }
    }
    else
    {
        if (sObjectMgr.GetArenaTeamByName(name))
        {
            SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, name, "", ERR_ARENA_TEAM_NAME_EXISTS_S);
            return;
        }
    }

    // and at last charter item check (确认背包中仍持有该登记表物品)
    Item* item = _player->GetItemByGuid(petitionGuid);
    if (!item)
    {
        return;
    }

    // OK!

    // delete charter item (销毁登记表物品)
    _player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

    if (type == 9)                                          // create guild (创建公会并加入所有签名成员)
    {
        Guild* guild = new Guild;
        if (!guild->Create(_player, name))
        {
            delete guild;
            return;
        }

        // register guild and add guildmaster (注册公会到管理器)
        sGuildMgr.AddGuild(guild);

        // Send result to GM (通知会长创建成功)
        if (WorldSession* session = _player->GetSession())
            session->SendGuildCommandResult(GUILD_CREATE_S, name, 0);

        // add members (将所有签名玩家添加为公会初始成员并记录事件日志)
        for (uint8 i = 0; i < signs; ++i)
        {
            Field* fields = queryResult->Fetch();

            ObjectGuid signGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            if (!signGuid)
                continue;

            guild->AddMember(signGuid, guild->GetLowestRank());

            // Put record into guild log (写入公会加入日志)
            guild->LogGuildEvent(GUILD_EVENT_LOG_JOIN_GUILD, signGuid);

            // Send result to online signees (通知在线签名者成为创始成员)
            if (Player* signee = sObjectMgr.GetPlayer(signGuid))
            {
                if (WorldSession* session = signee->GetSession())
                    session->SendGuildCommandResult(GUILD_FOUNDER_S, name, 0);
            }

            queryResult->NextRow();
        }
    }
    else                                                    // or arena team (创建竞技场战队并设置队徽与初始成员)
    {
        ArenaTeam* at = new ArenaTeam;
        if (!at->Create(_player->GetObjectGuid(), ArenaType(type), name))
        {
            sLog.outError("PetitionsHandler: arena team create failed.");
            delete at;
            return;
        }

        uint32 icon, iconcolor, border, bordercolor, backgroud;
        recv_data >> backgroud >> icon >> iconcolor >> border >> bordercolor;

        at->SetEmblem(backgroud, icon, iconcolor, border, bordercolor);

        // register team and add captain (注册竞技场战队并设置队长)
        sObjectMgr.AddArenaTeam(at);
        DEBUG_LOG("PetitonsHandler: arena team added to objmrg");

        // add members (添加所有签名玩家为战队成员)
        for (uint8 i = 0; i < signs; ++i)
        {
            Field* fields = queryResult->Fetch();
            ObjectGuid memberGUID = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            if (!memberGUID)
                continue;

            DEBUG_LOG("PetitionsHandler: adding arena member %s", memberGUID.GetString().c_str());
            at->AddMember(memberGUID);
            queryResult->NextRow();
        }
    }

    // 清理数据库中已完成的登记表与签名记录
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM petition WHERE petitionguid = '%u'", petitionGuid.GetCounter());
    CharacterDatabase.PExecute("DELETE FROM petition_sign WHERE petitionguid = '%u'", petitionGuid.GetCounter());
    CharacterDatabase.CommitTransaction();

    // created
    DEBUG_LOG("TURN IN PETITION %s", petitionGuid.GetString().c_str());

    WorldPacket data(SMSG_TURN_IN_PETITION_RESULTS, 4);
    data << uint32(PETITION_TURN_OK);
    SendPacket(data);
}

// 处理打开登记表购买列表请求（CMSG_PETITION_SHOWLIST）
void WorldSession::HandlePetitionShowListOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received CMSG_PETITION_SHOWLIST");
    // recv_data.hexlike();

    ObjectGuid guid;
    recv_data >> guid;

    SendPetitionShowList(guid);
}

// 根据 NPC 类型（公会徽章商人或竞技场组织者）下发可购买登记表列表（SMSG_PETITION_SHOWLIST）
void WorldSession::SendPetitionShowList(ObjectGuid guid) const
{
    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionShowListOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    uint8 count;
    if (pCreature->isTabardDesigner())
        count = 1;
    else
        count = 3;

    WorldPacket data(SMSG_PETITION_SHOWLIST, 8 + 1 + 4 * 6);
    data << ObjectGuid(guid);                               // npc guid
    data << uint8(count);                                   // count (可购条目数：公会 1 项，竞技场 3 项)
    if (count == 1)
    {
        data << uint32(1);                                  // index
        data << uint32(GUILD_CHARTER);                      // charter entry (公会登记表物品 ID)
        data << uint32(CHARTER_DISPLAY_ID);                 // charter display id (图标显示 ID)
        data << uint32(GUILD_CHARTER_COST);                 // charter cost (价格)
        data << uint32(0);                                  // unknown
        data << uint32(9);                                  // required signs? (所需签名数)
    }
    else
    {
        // 2v2
        data << uint32(1);                                  // index
        data << uint32(ARENA_TEAM_CHARTER_2v2);             // charter entry
        data << uint32(CHARTER_DISPLAY_ID);                 // charter display id
        data << uint32(ARENA_TEAM_CHARTER_2v2_COST);        // charter cost
        data << uint32(2);                                  // unknown
        data << uint32(2);                                  // required signs?
        // 3v3
        data << uint32(2);                                  // index
        data << uint32(ARENA_TEAM_CHARTER_3v3);             // charter entry
        data << uint32(CHARTER_DISPLAY_ID);                 // charter display id
        data << uint32(ARENA_TEAM_CHARTER_3v3_COST);        // charter cost
        data << uint32(3);                                  // unknown
        data << uint32(3);                                  // required signs?
        // 5v5
        data << uint32(3);                                  // index
        data << uint32(ARENA_TEAM_CHARTER_5v5);             // charter entry
        data << uint32(CHARTER_DISPLAY_ID);                 // charter display id
        data << uint32(ARENA_TEAM_CHARTER_5v5_COST);        // charter cost
        data << uint32(5);                                  // unknown
        data << uint32(5);                                  // required signs?
    }
    // for(uint8 i = 0; i < count; ++i)
    //{
    //    data << uint32(i);                        // index
    //    data << uint32(GUILD_CHARTER);            // charter entry
    //    data << uint32(CHARTER_DISPLAY_ID);       // charter display id
    //    data << uint32(GUILD_CHARTER_COST+i);     // charter cost
    //    data << uint32(0);                        // unknown
    //    data << uint32(9);                        // required signs?
    //}
    SendPacket(data);
    DEBUG_LOG("Sent SMSG_PETITION_SHOWLIST");
}
