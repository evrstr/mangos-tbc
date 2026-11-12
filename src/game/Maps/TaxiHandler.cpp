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
#include "Server/WorldPacket.h"
#include "Server/WorldSession.h"
#include "Server/Opcodes.h"
#include "Log/Log.h"
#include "Globals/ObjectMgr.h"
#include "Entities/Player.h"
#include "MotionGenerators/Path.h"
#include "MotionGenerators/WaypointMovementGenerator.h"

// 处理客户端查询飞行点状态请求（CMSG_TAXINODE_STATUS_QUERY）
void WorldSession::HandleTaxiNodeStatusQueryOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_TAXINODE_STATUS_QUERY");

    ObjectGuid guid;

    recv_data >> guid;
    SendTaxiStatus(guid);
}

// 查询玩家当前交互的飞行管理员对应飞行点是否已解锁，并下发 SMSG_TAXINODE_STATUS
void WorldSession::SendTaxiStatus(ObjectGuid guid) const
{
    // cheating checks (防作弊检查：确认目标生物存在)
    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
    {
        DEBUG_LOG("WorldSession::SendTaxiStatus - %s not found or you can't interact with it.", guid.GetString().c_str());
        return;
    }

    uint32 curloc = sObjectMgr.GetNearestTaxiNode(unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(), unit->GetMapId(), GetPlayer()->GetTeam());

    // not found nearest (未找到最近飞行点则忽略)
    if (curloc == 0)
        return;

    DEBUG_LOG("WORLD: current location %u ", curloc);

    WorldPacket data(SMSG_TAXINODE_STATUS, 9);
    data << ObjectGuid(guid);
    data << uint8(GetPlayer()->m_taxi.IsTaximaskNodeKnown(curloc) ? 1 : 0);
    SendPacket(data);

    DEBUG_LOG("WORLD: Sent SMSG_TAXINODE_STATUS");
}

// 处理客户端请求可飞往的飞行点列表（CMSG_TAXIQUERYAVAILABLENODES）：若为未学习站点则先学习，否则打开飞行地图
void WorldSession::HandleTaxiQueryAvailableNodes(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_TAXIQUERYAVAILABLENODES");

    ObjectGuid guid;
    recv_data >> guid;

    // cheating checks (交互距离与飞行管理员 NPC 标志校验)
    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTaxiQueryAvailableNodes - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // unknown taxi node case (若该飞行点尚未解锁，先触发学习并提示)
    if (SendLearnNewTaxiNode(unit))
        return;

    // known taxi node case (已解锁则下发飞行网络界面)
    SendTaxiMenu(unit);
}

// 发送已知飞行点掩码及当前起点，打开飞行管理员航线选择面板
void WorldSession::SendTaxiMenu(Creature* unit) const
{
    // find current node (查找飞行管理员对应的当前节点 ID)
    uint32 curloc = sObjectMgr.GetNearestTaxiNode(unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(), unit->GetMapId(), GetPlayer()->GetTeam());

    if (curloc == 0)
        return;

    DEBUG_LOG("WORLD: CMSG_TAXINODE_STATUS_QUERY %u ", curloc);

    WorldPacket data(SMSG_SHOWTAXINODES, (4 + 8 + 4 + 8 * 4));
    data << uint32(1);
    data << unit->GetObjectGuid();
    data << uint32(curloc);
    GetPlayer()->m_taxi.AppendTaximaskTo(data, GetPlayer()->isTaxiCheater());
    SendPacket(data);

    DEBUG_LOG("WORLD: Sent SMSG_SHOWTAXINODES");
}

// 尝试为玩家解锁飞行管理员所在的飞行点，成功解锁则返回 true
bool WorldSession::SendLearnNewTaxiNode(Creature* unit) const
{
    // find current node (查找当前位置最近的阵营飞行点)
    uint32 curloc = sObjectMgr.GetNearestTaxiNode(unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(), unit->GetMapId(), GetPlayer()->GetTeam());

    if (curloc == 0)
        return true;                                        // `true` send to avoid WorldSession::SendTaxiMenu call with one more curlock seartch with same false result. (返回 true 以避免上层再调用 SendTaxiMenu 重复搜索一次)

    if (GetPlayer()->m_taxi.SetTaximaskNode(curloc))
    {
        WorldPacket msg(SMSG_NEW_TAXI_PATH, 0);
        SendPacket(msg);

        WorldPacket update(SMSG_TAXINODE_STATUS, 9);
        update << ObjectGuid(unit->GetObjectGuid());
        update << uint8(1);
        SendPacket(update);

        return true;
    }
    return false;
}

// 下发激活飞行路线请求的响应结果（成功或错误码）
void WorldSession::SendActivateTaxiReply(ActivateTaxiReply reply) const
{
    WorldPacket data(SMSG_ACTIVATETAXIREPLY, 4);
    data << uint32(reply);
    SendPacket(data);

    DEBUG_LOG("WORLD: Sent SMSG_ACTIVATETAXIREPLY");
}

// 处理多段联程飞行请求（CMSG_ACTIVATETAXIEXPRESS）
void WorldSession::HandleActivateTaxiExpressOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXIEXPRESS");

    ObjectGuid guid;
    uint32 node_count, _totalcost;

    recv_data >> guid >> _totalcost >> node_count;

    Creature* npc = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        DEBUG_LOG("WORLD: HandleActivateTaxiExpressOpcode - %s not found or you can't interact with it.", guid.GetString().c_str());
        return;
    }
    std::vector<uint32> nodes;

    for (uint32 i = 0; i < node_count; ++i)
    {
        uint32 node;
        recv_data >> node;

        if (!_player->m_taxi.IsTaximaskNodeKnown(node) && !_player->isTaxiCheater())
        {
            SendActivateTaxiReply(ERR_TAXINOTVISITED);
            recv_data.rpos(recv_data.wpos()); // prevent additional spam at rejected packet (丢弃包内剩余字节，防止拒绝后的多余警告)
            return;
        }

        nodes.push_back(node);
    }

    if (nodes.empty())
        return;

    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXIEXPRESS from %d to %d", nodes.front(), nodes.back());

    GetPlayer()->ActivateTaxiPathTo(nodes, npc);
}

// 处理客户端样条移动完成确认包（CMSG_MOVE_SPLINE_DONE）
void WorldSession::HandleMoveSplineDoneOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_SPLINE_DONE");

    MovementInfo movementInfo;                              // used only for proper packet read (仅用于按协议顺序读取数据包)
    uint32 movementCounter;                                 // spline counter (样条计数器)

    recv_data >> movementInfo;
    recv_data >> movementCounter;

    // TODO: Add checking for correct point end for correct spline (待办：校验样条终点与对应计数器是否合法)
}

// 处理客户端单段点对点飞行请求（CMSG_ACTIVATETAXI）
void WorldSession::HandleActivateTaxiOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXI");

    ObjectGuid guid;
    std::vector<uint32> nodes;
    nodes.resize(2);

    recv_data >> guid >> nodes[0] >> nodes[1];
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXI from %d to %d", nodes[0], nodes[1]);
    Creature* npc = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        DEBUG_LOG("WORLD: HandleActivateTaxiOpcode - %s not found or you can't interact with it.", guid.GetString().c_str());
        return;
    }

    if (!_player->isTaxiCheater())
    {
        if (!_player->m_taxi.IsTaximaskNodeKnown(nodes[0]) || !_player->m_taxi.IsTaximaskNodeKnown(nodes[1]))
        {
            SendActivateTaxiReply(ERR_TAXINOTVISITED);
            return;
        }
    }

    GetPlayer()->ActivateTaxiPathTo(nodes, npc);
}
