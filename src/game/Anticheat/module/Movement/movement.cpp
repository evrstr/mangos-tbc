/*
 * Copyright (C) 2010-2013 Anathema Engine project <http://valkyrie-wow.com/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2016-2017 Elysium Project <https://www.elysium-project.org>
 * Copyright (C) 2017 Light's Hope Project <http://www.lightshope.org>
 * Copyright (C) 2017-2020 namreeb (legal@namreeb.org)
 */

#include "movement.hpp"
#include "../config.hpp"
#include "../libanticheat.hpp"
#include "../cyclic.hpp"

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "Chat/Chat.h"
#include "Entities/Unit.h"
#include "Entities/Player.h"
#include "Entities/GameObject.h"
#include "Server/WorldSession.h"
#include "Server/DBCStructure.h"
#include "World/World.h"
#include "Tools/Language.h"
#include "Movement/MoveSpline.h"
#include "Log/Log.h"

namespace
{
// 将服务端下发的移动指令操作码映射为客户端应回传的确认（ACK）操作码
uint16 GetOrderResponse(uint16 opcode)
{
    switch (opcode)
    {
        case SMSG_FORCE_WALK_SPEED_CHANGE:
            return CMSG_FORCE_WALK_SPEED_CHANGE_ACK;
        case SMSG_FORCE_RUN_SPEED_CHANGE:
            return CMSG_FORCE_RUN_SPEED_CHANGE_ACK;
        case SMSG_FORCE_RUN_BACK_SPEED_CHANGE:
            return CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK;
        case SMSG_FORCE_SWIM_SPEED_CHANGE:
            return CMSG_FORCE_SWIM_SPEED_CHANGE_ACK;
        case SMSG_FORCE_SWIM_BACK_SPEED_CHANGE:
            return CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK;
        case SMSG_FORCE_TURN_RATE_CHANGE:
            return CMSG_FORCE_TURN_RATE_CHANGE_ACK;
        case SMSG_FORCE_MOVE_ROOT:
            return CMSG_FORCE_MOVE_ROOT_ACK;
        case SMSG_FORCE_MOVE_UNROOT:
            return CMSG_FORCE_MOVE_UNROOT_ACK;
        case SMSG_MOVE_FEATHER_FALL:
        case SMSG_MOVE_NORMAL_FALL:
            return CMSG_MOVE_FEATHER_FALL_ACK;
        case SMSG_MOVE_SET_HOVER:
        case SMSG_MOVE_UNSET_HOVER:
            return CMSG_MOVE_HOVER_ACK;
        case SMSG_MOVE_SET_FLIGHT:
        case SMSG_MOVE_UNSET_FLIGHT:
            return CMSG_MOVE_FLIGHT_ACK;
        case SMSG_MOVE_WATER_WALK:
        case SMSG_MOVE_LAND_WALK:
            return CMSG_MOVE_WATER_WALK_ACK;
        default:
            return 0;
    }
}

// 计算两点间距离的平方（可选是否包含 Z 轴高度差）
float DistanceSquared(const Position &a, const Position &b, bool includeZ)
{
    float ret = pow(a.x - b.x, 2);
    ret += pow(a.y - b.y, 2);

    if (includeZ)
        ret += pow(a.z - b.z, 2);

    return ret;
}

// 根据移动标志位推断当前使用的 UnitMoveType 速度类型
uint32 GetMoveType(uint32 moveFlags)
{
    if (moveFlags & MOVEFLAG_SWIMMING)
        return moveFlags & MOVEFLAG_BACKWARD ? MOVE_SWIM_BACK : MOVE_SWIM;
    else if (moveFlags & MOVEFLAG_WALK_MODE)
        return MOVE_WALK;
    else if (moveFlags & MOVEFLAG_MASK_MOVING)
        return moveFlags & MOVEFLAG_BACKWARD ? MOVE_RUN_BACK : MOVE_RUN;

    return MOVE_RUN;
}

constexpr float extrapolationEpsilon = 0.0002f;             // 移动外推浮点误差容限
}

namespace Movement
{
extern float computeFallElevation(float time, bool slowFall, float initialSpeed);
}

namespace NamreebAnticheat
{
// 构造玩家移动反作弊追踪器
Movement::Movement(Player* me) :
    _anticheat(reinterpret_cast<SessionAnticheat *>(me->GetSession()->GetAnticheat())),
    _me(me), _totalDistanceTraveled(0.f), _wasMovingOther(false), _justTeleported(false),
    _serverInitTime(0), _clientInitTime(0), _jumpInitialSpeed(0.f), _inKnockBack(false),
    overSpeedDistanceTick(0.f), overSpeedDistanceTotal(0.f)
{
    memset(clientSpeeds, 0, sizeof(clientSpeeds));

    MANGOS_ASSERT(!!_anticheat);
}

// 记录服务端击退动作参数并进入击退状态（避免击退期间误报超速或跳速异常）
void Movement::KnockBack(float speedxy, float speedz, float cos, float sin)
{
    GetLastMovementInfo().jump.startClientTime = WorldTimer::getMSTime() - GetLastMovementInfo().stime + GetLastMovementInfo().ctime;
    GetLastMovementInfo().jump.start.x = _me->GetPositionX();
    GetLastMovementInfo().jump.start.y = _me->GetPositionY();
    GetLastMovementInfo().jump.start.z = _me->GetPositionZ();
    GetLastMovementInfo().jump.cosAngle = cos;
    GetLastMovementInfo().jump.sinAngle = sin;
    GetLastMovementInfo().jump.xyspeed = speedxy;
    GetLastMovementInfo().jump.zspeed = speedz;
    GetLastMovementInfo().moveFlags = MOVEFLAG_FALLING | (GetLastMovementInfo().moveFlags & ~MOVEFLAG_MASK_MOVING_OR_TURN);
    _jumpInitialSpeed = speedz;
    _inKnockBack = true;
}

// 校验并清理移动标志位中的非法状态（飞行、水上行走、缓落、定身脱离等）
void Movement::VerifyMovementFlags(uint32 flags, uint32 &removeFlags, bool strict) const
{
    removeFlags = 0;

    // do not apply to gms for now (暂不对 GM 账号执行移动标志检测)
    if (_me->GetSession()->GetSecurity() != SEC_PLAYER)
        return;

    // if under spline movement, do nothing (服务端样条路径控制期间跳过检测)
    if (!_me->movespline->Finalized())
        return;

    if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_FLY_HACK))
    {
        static constexpr uint32 flyHackFlags1 = (MOVEFLAG_WALK_MODE | MOVEFLAG_SWIMMING | MOVEFLAG_HOVER | MOVEFLAG_FALLINGFAR | MOVEFLAG_CAN_FLY | MOVEFLAG_ONTRANSPORT);
        static constexpr uint32 flyHackFlags2 = (MOVEFLAG_SWIMMING | MOVEFLAG_CAN_FLY);

        if ((flags & flyHackFlags1) == flyHackFlags1)
        {
            _anticheat->RecordCheatInternal(CHEAT_TYPE_FLY_HACK, "MovementFlags hack: Player %s had old flyhack moveFlags mask (flags: 0x%lx)",
                _me->GetName(), flags);
            removeFlags |= flags;
        }

        // detect flyhack (these two flags should never happen at the same time) (游泳与飞行标志绝不应同时存在)
        if ((flags & flyHackFlags2) == flyHackFlags2)
        {
            _anticheat->RecordCheatInternal(CHEAT_TYPE_FLY_HACK, "MovementFlags hack: Player %s had MOVE_SWIMMING and MOVE_FLYING (flags: 0x%lx)",
                _me->GetName(), flags);
            removeFlags |= flyHackFlags2;
        }

        // no need to check pending orders.  players should never have this. (玩家绝不可能拥有仅生物使用的悬浮标志)
        if (flags & MOVEFLAG_LEVITATING)
        {
            _anticheat->RecordCheatInternal(CHEAT_TYPE_FLY_HACK, "MovementFlags hack: Player %s had MOVE_LEVITATING (only for creatures!) (flags: 0x%lx)",
                _me->GetName(), flags);
            removeFlags |= MOVEFLAG_LEVITATING;
        }

        // no need to check pending orders.  players should never have this. (玩家不应在未受控状态下携带下降飞行标志)
        if (flags & MOVEFLAG_DESCENDING)
        {
            _anticheat->RecordCheatInternal(CHEAT_TYPE_FLY_HACK, "MovementFlags hack: Player %s had MOVE_CAN_FLY (flags: 0x%lx)",
                _me->GetName(), flags);
            removeFlags |= MOVEFLAG_DESCENDING;
        }
    }

    // if swimming, but not in water...
    // XXX TODO FIXME -- finish this after gathering some empirical data
    //if (!!(flags & MOVE_SWIMMING))
    //{
    //    float ground = -1234.5678f;
    //    auto const waterLevel = _me->GetTerrain()->GetWaterLevel(_me->GetPositionX(), _me->GetPositionY(), _me->GetPositionZ(), &ground);

    //    sLog.out(LOG_ANTICHEAT_DEBUG, "z: %f ground: %f water level: %f", _me->GetPositionZ(), ground, waterLevel);
    //}

    // if we are not performing a strict check, go no further (非严格模式下跳过后续待确认指令与光环校验)
    if (!strict)
        return;

    bool pendingRoot = false, pendingWaterWalkRemoval = false, pendingSlowFallRemoval = false;
    for (auto const &order : _orders)
    {
        if (order.opcode == SMSG_FORCE_MOVE_ROOT)
            pendingRoot = true;

        if (order.opcode == SMSG_MOVE_LAND_WALK)
            pendingWaterWalkRemoval = true;

        if (order.opcode == SMSG_MOVE_NORMAL_FALL)
            pendingSlowFallRemoval = true;
    }

    // if the client is claiming to not be rooted, but the player actually is rooted, and there is
    // no pending root order (in case the order had not arrived yet), assume the order was ignored
    // 玩家处于定身/昏迷状态且无待确认定身指令，但客户端上报缺少 MOVEFLAG_ROOT 时，判定为无视定身外挂
    if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_ROOT_MOVE) &&
        !(flags & MOVEFLAG_ROOT) &&
        _me->hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_ROOT) &&
        !pendingRoot)
        _anticheat->RecordCheatInternal(CHEAT_TYPE_ROOT_MOVE, "MovementFlags hack: Player %s lacks MOVEFLAG_ROOT but is stunned and/or rooted with no pending removal", _me->GetName());

    // if the client is claiming to be water walking, but the player has no aura for it, and no
    // pending cancellation order, assume it is illegitimate
    // 无水上行走光环（且非幽灵状态）也无待取消指令却携带水上行走标志，判定为水上行走外挂
    if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_WATER_WALK) &&
        !!(flags & MOVEFLAG_WATERWALKING) &&
        !_me->HasAuraType(SPELL_AURA_WATER_WALK) &&
        !_me->HasAuraType(SPELL_AURA_GHOST) &&
        !pendingWaterWalkRemoval)
    {
        _anticheat->RecordCheatInternal(CHEAT_TYPE_WATER_WALK, "MovementFlags hack: Player %s had MOVEFLAG_WATERWALKING but no water walk aura with no pending removal", _me->GetName());
        removeFlags |= MOVEFLAG_WATERWALKING;
    }

    // XXX check correct movement flag on slow fall

    // if safe falling with no aura and no pending removal order, cheater (无缓落光环也无待取消指令却携带安全坠落标志)
    if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_SLOW_FALL) &&
        !!(flags & MOVEFLAG_SAFE_FALL) &&
        !_me->HasAuraType(SPELL_AURA_FEATHER_FALL) &&
        !pendingSlowFallRemoval)
    {
        _anticheat->RecordCheatInternal(CHEAT_TYPE_SLOW_FALL, "MovementFlags hack: Player %s had MOVEFLAG_SAFE_FALL with no slow fall aura and no pending orders", _me->GetName());
        removeFlags |= MOVEFLAG_SAFE_FALL;
    }

    // things to check: hover
}

// 记录服务端下发的强制移动状态指令及序号
void Movement::OrderSent(uint16 opcode, uint32 counter)
{
    _orderHistory.push_back({ opcode, WorldTimer::getMSTime() });

    _orders.emplace_back<PendingOrder>({ opcode, counter, WorldTimer::getMSTime() });

    sLog.outDebug("ORDER: %s (%u) sent to %s (counter = %u)", LookupOpcodeName(opcode), opcode, _me->GetName(), counter);
}

// 校验客户端回传的移动指令确认包（ACK）是否与待确认队列中的操作码及序号匹配
void Movement::OrderAck(uint16 opcode, uint32 counter)
{
    _ackHistory.push_back({ opcode, WorldTimer::getMSTime() });

    sLog.outDebug("ORDER: %s (%u) ACK %s (counter = %u)", LookupOpcodeName(opcode), opcode, _me->GetName(), counter);

    if (_orders.empty())
    {
        if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_BAD_ORDER_ACK))
            _anticheat->RecordCheatInternal(CHEAT_TYPE_BAD_ORDER_ACK, "Received ACK %s (%u) but no orders are pending",
                LookupOpcodeName(opcode), opcode);

        return;
    }

    // if this detection method is disabled, just empty the results and stop (未启用 ACK 校验时直接清空队列)
    if (!sAnticheatConfig.IsEnabled(CHEAT_TYPE_BAD_ORDER_ACK))
    {
        _orders.clear();
        return;
    }

    // FIXME: there is currently a bug in the core whereby SMSG_FORCE_* opcodes are sent to a world session for
    // units other than the one they are controlling (i.e. a pet), prompting multiple consecutive ACKs, which
    // are not guaranteed to arrive in the same order they were sent.  once that bug is fixed, the below code
    // can be replaced with one which is both faster and more strict, where only the front of the queue is
    // checked against the data received from the client.
    // 遍历待确认队列匹配响应操作码与序号（兼容核心向宠物等非直接控制单位发送指令导致的乱序 ACK）
    for (auto i = _orders.begin(); i != _orders.end(); ++i)
    {
        auto const response = GetOrderResponse(i->opcode);

        // found? remove from the queue and return
        if (response == opcode && counter == i->counter)
        {
            _orders.erase(i);
            return;
        }
    }

    // if execution reaches here, it means that the acknowledgement we received was not in the queue to be received.
    // this should never happen for legitimate players.
    // 收到队列中不存在的指令确认，判定为伪造 ACK 包
    _anticheat->RecordCheatInternal(CHEAT_TYPE_BAD_ORDER_ACK, "Received ACK %s (%u) counter = %u which was not pending",
        LookupOpcodeName(opcode), opcode, counter);
}

// 清理超过 2.5 倍网络延迟仍未收到回执的过期移动指令
void Movement::CheckExpiredOrders(uint32 latency)
{
    auto const now = WorldTimer::getMSTime();
    const uint32 expirationDelay = 2.5 * latency;

    for (auto i = _orders.begin(); i != _orders.end(); ++i)
    {
        auto const expected = i->time + expirationDelay;

        if (expected < now)
        {
            //m_anticheat->RecordCheatInternal(CHEAT_TYPE_BAD_ORDER_ACK, "Expired order received no ACK for %s (%u) counter %u sent %u expected %u latency %u now %u",
            //    LookupOpcodeName(i->opcode), i->opcode, i->counter, i->time, expected, latency, now);
            i = _orders.erase(i);

            if (i == _orders.end())
                break;
        }
    }
}

// Movement processing anticheat main routine (移动包反作弊检测主入口：连跳、时钟加速/倒流、战歌高度/出界、坠落重置及外推/限速检测)
bool Movement::HandleAnticheatTests(MovementInfo& movementInfo, WorldSession* session, const WorldPacket& packet)
{
    MANGOS_ASSERT(!!_me);
    MANGOS_ASSERT(session);

    if (!sAnticheatConfig.EnableAnticheat())
        return true;

    // TODO: Currently anticheat is disabled with Mind Controlled players! (控制其他单位如精神控制期间暂不检测)
    if (!_me->IsSelfMover())
    {
        _wasMovingOther = true;
        return true;
    }
    // if we were moving someone else, but are no longer, skip one more tick (刚结束控制他人的首帧放行一次以同步状态)
    else if (_wasMovingOther)
    {
        _wasMovingOther = false;
        return true;
    }

    auto const opcode = packet.GetOpcode();

    // Dont accept movement packets while movement is controlled by server (fear, charge, etc..) (样条运动控制期间跳过)
    if (!_me->movespline->Finalized())
        return false;

    // two dimensional distance traveled (累加二维平面移动距离供反垃圾与统计使用)
    auto const dist2d =
        (movementInfo.pos.x - GetLastMovementInfo().pos.x) * (movementInfo.pos.x - GetLastMovementInfo().pos.x) +
        (movementInfo.pos.y - GetLastMovementInfo().pos.y) * (movementInfo.pos.y - GetLastMovementInfo().pos.y);

    _totalDistanceTraveled += sqrtf(dist2d);

    if (_inKnockBack && opcode != MSG_MOVE_FALL_LAND)
        movementInfo.jump = GetLastMovementInfo().jump;

    if (opcode == CMSG_MOVE_FEATHER_FALL_ACK)
    {
        GetLastMovementInfo().jump.startClientTime = movementInfo.jump.startClientTime = movementInfo.ctime;
        //GetLastMovementInfo().jump.start = movementInfo.jump.start = movementInfo.pos;
        _jumpInitialSpeed = std::max(_jumpInitialSpeed, 7.f);
    }

    /*if ((movementInfo.moveFlags & MOVE_ROOT) && (movementInfo.moveFlags & MOVE_MASK_MOVING_OR_TURN))
        APPEND_CHEAT(CHEAT_TYPE_ROOT_MOVE);*/

    // 连跳检测：下落过程中再次发出起跳包
    if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_MULTIJUMP) &&
        opcode == MSG_MOVE_JUMP &&
        GetLastMovementInfo().moveFlags & MOVEFLAG_FALLING)
    {
        _anticheat->RecordCheatInternal(CHEAT_TYPE_MULTIJUMP, "Jump start %u", movementInfo.jump.startClientTime);

        std::string str;
        DumpMovement(str);

        sLog.outBasic("Multijump debug output:\n%s", str.c_str());
    }

    if (opcode == MSG_MOVE_STOP_SWIM && (movementInfo.moveFlags & MOVEFLAG_SWIMMING))
        _anticheat->RecordCheatInternal(CHEAT_TYPE_FLY_HACK);

    if (!movementInfo.ctime && sAnticheatConfig.IsEnabled(CHEAT_TYPE_NULL_CLIENT_TIME))
        _anticheat->RecordCheatInternal(CHEAT_TYPE_NULL_CLIENT_TIME);

    // This can happen if the player is knocked back while jumping (非击退状态下的起跳水平速度超限检测)
    if (opcode == MSG_MOVE_JUMP && sAnticheatConfig.IsEnabled(CHEAT_TYPE_OVERSPEED_JUMP) && !IsInKnockBack())
    {
        // if the player is jumping, use the movement speed based on flags from the last movement.
        // otherwise, use this one.  note that if the player was swimming, we should also use the
        // old movement flags.  this is because the client will use the swimming speed for this
        // case, rather than the ground speed.
        // 起跳瞬间沿用上一帧（若处于下落或游泳状态）或当前帧的速度类型作为期望上限
        auto const expected = !!(GetLastMovementInfo().moveFlags & (MOVEFLAG_FALLING | MOVEFLAG_SWIMMING)) ?
            GetXYFlagBasedClientSpeed(GetLastMovementInfo().moveFlags) :
            GetXYFlagBasedClientSpeed(movementInfo.moveFlags);

        // XXX: these should all be temporary until the above logic is perfected
        auto const oldServerSpeed = _me->GetXYFlagBasedSpeed(GetLastMovementInfo().moveFlags);
        auto const oldClientSpeed = GetXYFlagBasedClientSpeed(GetLastMovementInfo().moveFlags);
        auto const serverSpeed = _me->GetXYFlagBasedSpeed(movementInfo.moveFlags);
        auto const clientSpeed = GetXYFlagBasedClientSpeed(movementInfo.moveFlags);

        if (movementInfo.jump.xyspeed > expected)
        {
            _anticheat->RecordCheatInternal(CHEAT_TYPE_OVERSPEED_JUMP,
                "Lateral jump speed is too high (%f > %f) flags 0x%lx -> 0x%lx old server: %f old client: %f new server: %f new client: %f opcode: %s",
                movementInfo.jump.xyspeed, expected, GetLastMovementInfo().moveFlags, movementInfo.moveFlags,
                oldServerSpeed, oldClientSpeed, serverSpeed, clientSpeed, LookupOpcodeName(opcode));

            std::string str;

            _anticheat->GetMovementDebugString(str);

            sLog.outBasic("%s", str.c_str());
        }
    }

    // Not allowed to change horizontal speed while jumping (unless it was zero) (跳跃空中禁止私自提高水平速度，入水瞬间水上行走除外)
    if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_JUMP_SPEED_CHANGE) &&
        !IsInKnockBack() &&
        opcode != CMSG_MOVE_KNOCK_BACK_ACK && opcode != MSG_MOVE_STOP &&
        (movementInfo.moveFlags & (MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)) &&
        (GetLastMovementInfo().moveFlags & (MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)) &&
        movementInfo.jump.xyspeed > GetLastMovementInfo().jump.xyspeed + 0.0001f        &&
        GetLastMovementInfo().jump.xyspeed > 0.0001f &&
        !(!!((movementInfo.moveFlags | GetLastMovementInfo().moveFlags) & MOVEFLAG_WATERWALKING)
            && GetLastMovementInfo().jump.xyspeed < 0.001f))   // true when entering water with waterwalking
    {
        _anticheat->RecordCheatInternal(CHEAT_TYPE_JUMP_SPEED_CHANGE,
            "Lateral jump speed changed from %f to %f flags 0x%lx -> 0x%lx opcode %s jump zspeed: %f cos: %f sin: %f fall time: %d",
            GetLastMovementInfo().jump.xyspeed, movementInfo.jump.xyspeed, GetLastMovementInfo().moveFlags,
            movementInfo.moveFlags, LookupOpcodeName(opcode), movementInfo.jump.zspeed,
            movementInfo.jump.cosAngle, movementInfo.jump.sinAngle, movementInfo.fallTime);

        std::string str;
        DumpMovement(str);
        sLog.outBasic("Lateral check recent movements:\n%s", str.c_str());
    }

    // use the first processed movement as a client/server clock sync (以首次有效移动包同步客户端与服务端时钟基准)
    if (!_serverInitTime || !_clientInitTime)
    {
        _serverInitTime = movementInfo.stime;
        _clientInitTime = movementInfo.ctime;
    }
    else
    {
        if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_NUM_DESYNC) && !_anticheat->IsKickTimerActive() && !_anticheat->IsBanTimerActive())
        {
            constexpr float msPerMinuteTolerance = 500.f;
            auto constexpr toleranceMultiplier = msPerMinuteTolerance / (MINUTE * IN_MILLISECONDS);

            // some computers clocks move at slightly different speeds.  experimentally, the maximum
            // tolerance observed is about 140ms per minute.  however there is little gain for a speedhack
            // which accelerates the clock by merely 500ms per minute as this would resemble an increase
            // of only 0.8% movement speed.  therefore we will apply a tolerance of 500ms per minute since
            // the clock has synced.
            // 允许每分钟最多 500ms（约 0.8% 增速）的晶振时钟漂移容限；客户端时间推进显著快于服务端则判定变速齿轮

            auto const serverDiff = WorldTimer::getMSTimeDiff(movementInfo.stime, _serverInitTime);
            auto const clientDiff = WorldTimer::getMSTimeDiff(movementInfo.ctime, _clientInitTime);
            auto const tolerance = std::max(5000.f, serverDiff * toleranceMultiplier);

            // if more time passed for us than it did for them, this is not useful as a cheat, but can happen due to
            // fluctuations in network latency
            if (clientDiff > (serverDiff + tolerance))
            {
                _anticheat->RecordCheatInternal(CHEAT_TYPE_NUM_DESYNC,
                    "Client desync exceeded tolerance.  Desync: %d server diff: %d client diff: %d tolerance: %f",
                    clientDiff - serverDiff, serverDiff, clientDiff, tolerance);
            }
        }

        // Client going back in time ... ?! (客户端时间戳倒流检测)
        if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_TIME_BACK) && movementInfo.ctime < GetLastMovementInfo().ctime)
        {
            // temporary measure to remove false positives
            if (GetLastMovementInfo().ctime - movementInfo.ctime < 1000)
            {
                _anticheat->RecordCheatInternal(CHEAT_TYPE_TIME_BACK, "Clock moved in reverse from %u to %u",
                    GetLastMovementInfo().ctime, movementInfo.ctime);
            }
            else
            {
                std::string log;
                _anticheat->GetMovementDebugString(log);

                sLog.outBasic("TimeBack debug output.\n%s", log.c_str());
            }
        }
    }

    // Warsong Battleground - specific checks (战歌峡谷战场专用限制：禁高空及开战前出出生区域)
    if (_me->GetMapId() == 489 && sAnticheatConfig.IsEnabled(CHEAT_TYPE_FORBIDDEN))
    {
        // Too high - not allowed (but possible with some engineering items malfunction) (高度异常检测)
        if (!(movementInfo.moveFlags & (MOVEFLAG_FALLINGFAR | MOVEFLAG_FALLING)) && movementInfo.pos.z > 380.0f)
            _anticheat->RecordCheatInternal(CHEAT_TYPE_FORBIDDEN, "Too high in Warsong Gulch (z = %f)", movementInfo.pos.z);

        if (BattleGround* bg = _me->GetBattleGround())
        {
            if (bg->GetStatus() == STATUS_WAIT_JOIN)
            {
                // Battleground not started. Players should be in their starting areas. (开战前禁止提前离开各自准备区)
                if (_me->GetTeam() == ALLIANCE && movementInfo.pos.x < 1490.0f)
                    _anticheat->RecordCheatInternal(CHEAT_TYPE_FORBIDDEN, "Outside alliance starting area (%f, %f, %f)",
                        movementInfo.pos.x, movementInfo.pos.y, movementInfo.pos.z);
                if (_me->GetTeam() == HORDE && movementInfo.pos.x > 957.0f)
                    _anticheat->RecordCheatInternal(CHEAT_TYPE_FORBIDDEN, "Outside horde starting area (%f, %f, %f)",
                        movementInfo.pos.x, movementInfo.pos.y, movementInfo.pos.z);
            }
        }
    }

    // Minimal checks on transports (瞬间远距离登上载具/交通工具的瞬移检测)
    if (movementInfo.moveFlags & MOVEFLAG_ONTRANSPORT &&
        sAnticheatConfig.IsEnabled(CHEAT_TYPE_TELE_TO_TRANSPORT) &&
        GetLastMovementInfo().ctime &&
        !(GetLastMovementInfo().moveFlags & MOVEFLAG_ONTRANSPORT))
    {
        auto constexpr maxDist2d = 100.f * 100.f;

        if (dist2d > maxDist2d)
            _anticheat->RecordCheatInternal(CHEAT_TYPE_TELE_TO_TRANSPORT, "Movement distance %f", sqrtf(dist2d));
    }

    if (opcode == CMSG_MOVE_FALL_RESET && CheckFallReset(movementInfo))
        _anticheat->RecordCheatInternal(CHEAT_TYPE_BAD_FALL_RESET, "Bad fall reset");

    // Distance computation related (非飞行点、非载具、非刚传送时进行位移外推或速度上限校验)
    if (!_me->IsTaxiFlying() &&
        !(movementInfo.moveFlags & MOVEFLAG_ONTRANSPORT) &&
        sAnticheatConfig.EnableAntiSpeedHack() &&
        !_justTeleported)
    {
        float allowedDXY = 0.0f;
        float allowedDZ = 0.0f;
        float realDistance2D_sq = 0.0f;

        auto const dt = movementInfo.ctime - GetLastMovementInfo().ctime;

        // Check vs extrapolation (基于上一帧位置与速度外推预期位置并累计超速偏移距离)
        if (sAnticheatConfig.EnableExtrapolation())
        {
            Position extrap;

            // predict destination given the last movement position, direction, and flags, and compare to value reported by the client
            if (ExtrapolateMovement(GetLastMovementInfo(), dt, extrap))
            {
                auto const includeZ = !!((movementInfo.moveFlags | GetLastMovementInfo().moveFlags) & MOVEFLAG_FALLING);

                // how far our extrapolation predicted they will go (squared) (外推理论移动距离平方)
                auto const extrapDist2 = DistanceSquared(GetLastMovementInfo().pos, extrap, includeZ);

                // how far they actually went (squared) (客户端实际上报移动距离平方)
                auto const theirDist2 = DistanceSquared(GetLastMovementInfo().pos, movementInfo.pos, includeZ);

                // if they went further than we expected, record it (超出外推距离则计入超速距离累计)
                if (theirDist2 > extrapDist2)
                {
                    constexpr float minErr = 1.f;

                    auto const extrapDist = sqrt(extrapDist2);
                    auto const theirDist = sqrt(theirDist2);

                    auto const delta = theirDist - extrapDist;

                    if (delta > extrapolationEpsilon)
                    {
                        overSpeedDistanceTick += delta;
                        overSpeedDistanceTotal += delta;
                    }

                    if (delta >= minErr)
                        if (auto const anticheat = dynamic_cast<AnticheatLib *>(GetAnticheatLib()))
                            anticheat->OfferExtrapolationData(
                                GetLastMovementInfo(),
                                clientSpeeds[GetMoveType(GetLastMovementInfo().moveFlags)],
                                clientSpeeds[GetMoveType(movementInfo.moveFlags)],
                                movementInfo, extrap, delta);
                }
            }
        }
        else if (GetMaxAllowedDist(GetLastMovementInfo(), dt, allowedDXY, allowedDZ))
        {
            // Allow some margin (常规最大允许位移检测，预留 0.5 码容错)
            allowedDXY += 0.5f;
            allowedDZ += 0.5f;

            realDistance2D_sq = DistanceSquared(movementInfo.pos, GetLastMovementInfo().pos, false);

            if (realDistance2D_sq > allowedDXY * allowedDXY * 1.1f)
            {
                auto const dist = sqrt(realDistance2D_sq) - allowedDXY;

                if (dist > extrapolationEpsilon)
                {
                    overSpeedDistanceTick += dist;
                    overSpeedDistanceTotal += dist;
                }
            }

            if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_OVERSPEED_Z) && fabs(movementInfo.pos.z - GetLastMovementInfo().pos.z) > allowedDZ)
                _anticheat->RecordCheatInternal(CHEAT_TYPE_OVERSPEED_Z, "z changed from %f to %f, difference of %f, max allowed: %f",
                    GetLastMovementInfo().pos.z, movementInfo.pos.z, fabs(movementInfo.pos.z - GetLastMovementInfo().pos.z), allowedDZ);
        }

        // Client should send heartbeats every 500ms at the most (it can send them sooner
        // if movement flags change, i.e. entering or exiting a transport)
        // 移动期间客户端最迟应每 500ms 发送心跳包，超过 1000ms 无移动包则记录心跳丢失异常
        if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_SKIPPED_HEARTBEATS) &&
            dt > 1000 &&
            GetLastMovementInfo().ctime &&
            GetLastMovementInfo().moveFlags & MOVEFLAG_MASK_MOVING)
            _anticheat->RecordCheatInternal(CHEAT_TYPE_SKIPPED_HEARTBEATS, "Time since last movement: %d", dt);
    }

    //GetLastMovementInfo() = movementInfo;
    //GetLastMovementInfo().UpdateTime(WorldTimer::getMSTime());
    // This is required for proper movement interpolation
    if (opcode == MSG_MOVE_JUMP)
        _jumpInitialSpeed = 7.95797334f;
    else if (opcode == MSG_MOVE_FALL_LAND)
    {
        _jumpInitialSpeed = -9.645f;
        _inKnockBack = false;
    }

    if (_justTeleported)
    {
        _moveHistory.clear();
        _justTeleported = false;
    }

    _moveHistory.push_back({ static_cast<uint16>(packet.GetOpcode()), movementInfo });

    return true;
}

// 处理客户端速度变更确认包，比对服务端速度并在更新速度前基于旧速度执行反作弊检查
bool Movement::HandleSpeedChangeAck(MovementInfo& movementInfo, WorldSession* session, const WorldPacket& packet, float newSpeed)
{
    static char const* move_type_name[MAX_MOVE_TYPE] = {"Walk", "Run", "RunBack", "Swim", "SwimBack", "TurnRate"};
    UnitMoveType moveType;
    switch (packet.GetOpcode())
    {
        case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:
            moveType = MOVE_WALK;
            break;
        case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:
            moveType = MOVE_RUN;
            break;
        case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:
            moveType = MOVE_RUN_BACK;
            break;
        case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:
            moveType = MOVE_SWIM;
            break;
        case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:
            moveType = MOVE_SWIM_BACK;
            break;
        case CMSG_FORCE_TURN_RATE_CHANGE_ACK:
            moveType = MOVE_TURN_RATE;
            break;
        case CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK:
            moveType = MOVE_FLIGHT;
            break;
        case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:
            moveType = MOVE_FLIGHT_BACK;
            break;
        default:
            return false;
    }

    // Compare to server side speed. (若确认速度与服务端记录不一致则重新下发服务端速度同步)
    if (!_me->GetTransport() && fabs(_me->GetSpeed(moveType) - newSpeed) > 0.01f)
    {
        DETAIL_LOG("%sSpeedChange player %s incorrect. %f -> %f (instead of %f)",
                       move_type_name[moveType], _me->GetName(), clientSpeeds[moveType], newSpeed, _me->GetSpeed(moveType));
        _me->SetSpeedRate(moveType, _me->GetSpeedRate(moveType), true);
    }

    // Compute anticheat generic checks - with old speed. (先按变更前的旧速度校验本次附带的移动信息，再更新 clientSpeeds)
    HandleAnticheatTests(movementInfo, session, packet);
    clientSpeeds[moveType] = newSpeed;
    return true;
}

// 进入世界时记录切换历史并初始化客户端各移动模式速度表
void Movement::HandleEnterWorld()
{
    _worldChangeHistory.push_back({ true, WorldTimer::getMSTime() });

    for (int i = 0; i < MAX_MOVE_TYPE; ++i)
        clientSpeeds[i] = _me->GetSpeed(static_cast<UnitMoveType>(i));
}

// 离开世界时记录切换历史并重置时钟同步基准
void Movement::HandleLeaveWorld()
{
    _worldChangeHistory.push_back({ false, WorldTimer::getMSTime() });

    _serverInitTime = _clientInitTime = 0;
}

// 记录客户端时间跳变并将客户端初始时间基准前移对应毫秒数
void Movement::TimeSkipped(const ObjectGuid &mover, uint32 ms)
{
    _timeSkipHistory.push_back({ mover, ms, WorldTimer::getMSTime() });

    _clientInitTime += ms;

    sLog.outDebug("Time skipped: Player %s (GUID 0x%x) mover 0x%lx ms %u",
        _me ? _me->GetName() : "<none>", _me ? _me->GetGUIDLow() : 0, mover.GetRawValue(), ms);
}

MovementInfo& Movement::GetLastMovementInfo() const
{
    return _me->m_movementInfo;
}

// 根据当前移动标志位返回客户端应使用的水平移动速度
float Movement::GetXYFlagBasedClientSpeed(uint32 moveFlags) const
{
    // swimming?
    if (!!(moveFlags & MOVEFLAG_SWIMMING))
    {
        if (!!(moveFlags & MOVEFLAG_BACKWARD))
            return clientSpeeds[MOVE_SWIM_BACK];

        return clientSpeeds[MOVE_SWIM];
    }

    // not moving laterally? zero!
    if (!(moveFlags & MOVEFLAG_MASK_XY))
        return 0.f;

    // walking?
    if (!!(moveFlags & MOVEFLAG_WALK_MODE))
    {
        // Seems to always be same speed forward and backward when walking (步行前后速度一致)
        return clientSpeeds[MOVE_WALK];
    }

    // Presumably only running left when IsMoving is true
    if (!!(moveFlags & MOVEFLAG_BACKWARD))
        return clientSpeeds[MOVE_RUN_BACK];

    return clientSpeeds[MOVE_RUN];
}

// 基于上一移动状态与时间差外推玩家理论位置（涵盖静止、下落抛物线、圆弧转向及直线移动，并校验视线阻挡）
bool Movement::ExtrapolateMovement(MovementInfo const& mi, uint32 diffMs, Position &pos) const
{
    // TODO: These cases are not handled in movement extrapolation
    // - Transports (载具/俯仰移动暂不外推)
    if (mi.moveFlags & (MOVEFLAG_PITCH_UP | MOVEFLAG_PITCH_DOWN | MOVEFLAG_ONTRANSPORT))
        return false;
    // - Not correctly handled yet (issues regarding feather fall auras) (长距离坠落暂不外推)
    if (mi.moveFlags & MOVEFLAG_FALLINGFAR)
        return false;
    // - Server side movement (should be easy to extrapolate actually) (服务端样条移动中不外推)
    if (!_me->movespline->Finalized())
        return false;
    // Dernier paquet pas a jour (connexion, TP autre map ...) (上一数据包时间戳尚未更新，如刚登录或跨图传送)
    if (mi.ctime == 0)
        return false;

    // if we are not moving, position shouldn't change (未处于移动状态时位置应保持不变)
    if (!(mi.moveFlags & MOVEFLAG_MASK_MOVING))
    {
        pos = mi.pos;
        return true;
    }

    pos = mi.pos;

    float o = pos.o;

    // Not allowed to move
    /*if (mi.moveFlags & MOVE_ROOT)
        return true;*/

    auto const speed = clientSpeeds[GetMoveType(mi.moveFlags)];

    if (mi.moveFlags & MOVEFLAG_BACKWARD)
        o += M_PI_F;
    else if (mi.moveFlags & MOVEFLAG_STRAFE_LEFT)
    {
        if (mi.moveFlags & MOVEFLAG_FORWARD)
            o += M_PI_F / 4;
        else
            o += M_PI_F / 2;
    }
    else if (mi.moveFlags & MOVEFLAG_STRAFE_RIGHT)
    {
        if (mi.moveFlags & MOVEFLAG_FORWARD)
            o -= M_PI_F / 4;
        else
            o -= M_PI_F / 2;
    }

    if (mi.moveFlags & MOVEFLAG_FALLING)
    {
        float diffT = WorldTimer::getMSTimeDiff(mi.jump.startClientTime, diffMs + mi.ctime) / 1000.f;
        pos.x = mi.jump.start.x;
        pos.y = mi.jump.start.y;
        pos.z = mi.jump.start.z;
        // Fatal error. Avoid crashing here ... (起跳坐标无效或时间差异常时放弃外推)
        if (!pos.x || !pos.y || !pos.z || diffT > 10000.f)
            return false;
        pos.x += mi.jump.cosAngle * mi.jump.xyspeed * diffT;
        pos.y += mi.jump.sinAngle * mi.jump.xyspeed * diffT;
        pos.z -= ::Movement::computeFallElevation(diffT, mi.moveFlags & MOVEFLAG_SAFE_FALL, -_jumpInitialSpeed);
    }
    else if (mi.moveFlags & (MOVEFLAG_TURN_LEFT | MOVEFLAG_TURN_RIGHT))
    {
        if (mi.moveFlags & MOVEFLAG_MASK_MOVING)
        {
            // Every 2 sec (边移动边转向：沿圆弧轨迹计算位置变化)
            float T = 0.75f * (clientSpeeds[MOVE_TURN_RATE]) * (diffMs / 1000.f);
            float R = 1.295f * speed / M_PI * cos(mi.s_pitch);
            pos.z += diffMs * speed / 1000.0f * sin(mi.s_pitch);
            // Find the center of the circle we are moving on (计算圆弧运动圆心并沿切线弧度推进)
            if (mi.moveFlags & MOVEFLAG_TURN_LEFT)
            {
                pos.x += R * cos(o + M_PI / 2);
                pos.y += R * sin(o + M_PI / 2);
                pos.o += T;
                T = T - M_PI / 2.0f;
            }
            else
            {
                pos.x += R * cos(o - M_PI / 2);
                pos.y += R * sin(o - M_PI / 2);
                pos.o -= T;
                T = -T + M_PI / 2.0f;
            }
            pos.x += R * cos(o + T);
            pos.y += R * sin(o + T);
        }
        else
        {
            float diffO = clientSpeeds[MOVE_TURN_RATE] * diffMs / 1000.f;
            if (mi.moveFlags & MOVEFLAG_TURN_LEFT)
                pos.o += diffO;
            else
                pos.o -= diffO;
            return true;
        }
    }
    else if (mi.moveFlags & MOVEFLAG_MASK_MOVING)
    {
        auto const dist = speed * diffMs / 1000.f;

        pos.x += dist * cos(o) * cos(mi.s_pitch);
        pos.y += dist * sin(o) * cos(mi.s_pitch);
        pos.z += dist * sin(mi.s_pitch);
    }
    else // If we reach here, we did not move
        return true;

    if (!MaNGOS::IsValidMapCoord(pos.x, pos.y, pos.z, pos.o))
        return false;

    if (!(mi.moveFlags & (MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR | MOVEFLAG_SWIMMING | MOVEFLAG_WATERWALKING)))
        pos.z = _me->GetMap()->GetHeight(pos.x, pos.y, pos.z);

    return _me->GetMap()->IsInLineOfSight(mi.pos.x, mi.pos.y, mi.pos.z + 0.5f, pos.x, pos.y, pos.z + 0.5f, false);
}

// 计算在给定时间间隔内允许的水平最大位移 dxy 与垂直最大位移 dz
bool Movement::GetMaxAllowedDist(MovementInfo const& mi, uint32 diffMs, float &dxy, float &dz) const
{
    dxy = dz = 0.001f; // Epsilon

    if (mi.moveFlags & MOVEFLAG_ONTRANSPORT)
        return false;

    if (!_me->movespline->Finalized())
        return false;

    // Dernier paquet pas a jour (connexion, TP autre map ...) (上一数据包时间戳尚未初始化)
    if (!mi.ctime)
        return false;

    if (!(mi.moveFlags & MOVEFLAG_MASK_MOVING))
    {
        dxy = dz = 0.f;
        return true;
    }

    // No mvt allowed
    /*if (mi.moveFlags & MOVE_ROOT || !(mi.moveFlags & MOVE_MASK_MOVING))
        return true;*/

    auto const speed = clientSpeeds[GetMoveType(mi.moveFlags)];

    if (mi.moveFlags & (MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR))
    {
        dxy = mi.jump.xyspeed / 1000 * diffMs;
        static constexpr float terminalVelocity = 60.148003f;
        static constexpr float terminalSlowFallVelocity = 7.f;
        dz = (mi.moveFlags & MOVEFLAG_SAFE_FALL) ? terminalSlowFallVelocity : terminalVelocity;
        dz = dz / 1000 * diffMs;
        return true;
    }

    // TODO: Maximum dyx/dz (max climb angle) if not swimming.
    dxy = speed / 1000 * diffMs;
    dz = speed / 1000 * diffMs;

    return true;
}

// 玩家探索区域回调：按配置记录普通/高等级异常探索作弊
void Movement::OnExplore(AreaTableEntry const* p)
{
    if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_EXPLORE))
        _anticheat->RecordCheatInternal(CHEAT_TYPE_EXPLORE);

    if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_EXPLORE_HIGH_LEVEL) && static_cast<int32>(_me->GetLevel() + 10) < p->area_level)
        _anticheat->RecordCheatInternal(CHEAT_TYPE_EXPLORE_HIGH_LEVEL);
}

// 标记刚刚发生合法传送，下一帧清空移动历史避免误报瞬移
void Movement::HandleTeleport(const Position &)
{
    _justTeleported = true;
}

// 校验玩家声称所在的交通工具/载具是否真实存在且处于合法距离内
void Movement::OnTransport(Player* plMover, ObjectGuid transportGuid)
{
    // The anticheat is disabled on transports, so we need to be sure that the player is indeed on a transport.
    // 乘坐交通工具期间豁免常规位移检测，因此必须校验目标载具确实存在且玩家位于其附近
    GameObject* transportGobj = plMover->GetMap()->GetGameObject(transportGuid);

    auto const maxDist2d = plMover->GetMapId() == 369 ? 3000.f : 70.f; // Transports usually dont go far away (exception is deeprun tram) (矿道地铁 369 地图放宽距离上限)

    if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_FAKE_TRANSPORT) &&
        (!transportGobj || !transportGobj->IsTransport() || !transportGobj->IsWithinDist(plMover, maxDist2d, false)))
        _anticheat->RecordCheatInternal(CHEAT_TYPE_FAKE_TRANSPORT);
}

// 判断本次位置跃迁是否属于合法范围（如电梯区域、击退、服务端传送或短距离移动）
bool Movement::IsTeleportAllowed(MovementInfo const& movementInfo, float& distance)
{
    // check valid source coordinates (忽略起点零坐标)
    if (_me->GetPositionX() == 0.0f || _me->GetPositionY() == 0.0f || _me->GetPositionZ() == 0.0f)
        return true;

    // check valid destination coordinates (忽略终点零坐标)
    if (movementInfo.GetPos().x == 0.0f || movementInfo.GetPos().y == 0.0f || movementInfo.GetPos().z == 0.0f)
        return true;

    // if knockbacked (处于击退发射状态)
    if (_me->IsLaunched())
        return true;

    // ignore valid teleport state (处于服务端合法传送状态)
    if (_me->IsBeingTeleported())
        return true;

    // some exclude zones - lifts and other, but.. (豁免特定升降梯区域，如千针石林大升降梯、幽暗城电梯)
    uint32 destZoneId = 0;
    uint32 destAreaId = 0;

    _me->GetTerrain()->GetZoneAndAreaId(destZoneId, destAreaId, movementInfo.pos.x, movementInfo.pos.y, movementInfo.pos.z);

    // checks far teleports
    if (destZoneId == _me->GetZoneId() && destAreaId == _me->GetAreaId())
    {
        // Thousand Needles - Great Lift (千针石林大升降梯 / 矿道地铁)
        if (_me->GetZoneId() == 2257 || (_me->GetZoneId() == 400 && _me->GetAreaId() == 485))
            return true;

        // Undercity Lift (幽暗城电梯)
        if (_me->GetZoneId() == 1497 && _me->GetAreaId() == 1497)
            return true;
    }

    float deltaX = _me->GetPositionX() - movementInfo.pos.x;
    float deltaY = _me->GetPositionY() - movementInfo.pos.y;
    float deltaZ = _me->GetPositionZ() - movementInfo.pos.z;
    distance = sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);

    if (distance < 40.0f)
        return true;

    return false;
}

// 检查 CMSG_MOVE_FALL_RESET 是否存在异常（非坠落状态重置或参数非零）
bool Movement::CheckFallReset(MovementInfo const& movementInfo) const
{
    if (!sAnticheatConfig.getConfig(CONFIG_BOOL_AC_MOVEMENT_CHEAT_BAD_FALL_RESET_ENABLED))
        return false;

    if (GetLastMovementInfo().ctime)
    {
        if (!GetLastMovementInfo().HasMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)))
            return true;
    }

    return movementInfo.fallTime != 0 || movementInfo.jump.zspeed != 0.0f;
}

// 检查当前移动包是否存在非法瞬移、擅自进入 GM 岛及非法移动标志位
bool Movement::CheckTeleport(uint16 opcode, MovementInfo& movementInfo)
{
    if (_me->GetMover()->GetTypeId() != TYPEID_PLAYER)
        return true;

    auto const mover = reinterpret_cast<const Player *>(_me->GetMover());

    // teleport hack check (非载具、非飞行路径状态下检查瞬移)
    if (!mover->GetTransport() && !mover->HasMovementFlag(MOVEFLAG_ONTRANSPORT) && !mover->IsTaxiFlying())
    {
        float distance;

        if (!IsTeleportAllowed(movementInfo, distance))
        {
            uint32 destZoneId = 0;
            uint32 destAreaId = 0;

            mover->GetTerrain()->GetZoneAndAreaId(destZoneId, destAreaId,
                movementInfo.pos.x,
                movementInfo.pos.y,
                movementInfo.pos.z);

            // get zone and area info
            MapEntry const* mapEntry = sMapStore.LookupEntry(mover->GetMapId());
            auto const srcZoneEntry = GetAreaEntryByAreaID(mover->GetZoneId());
            auto const srcAreaEntry = GetAreaEntryByAreaID(mover->GetAreaId());
            auto const destZoneEntry = GetAreaEntryByAreaID(destZoneId);
            auto const destAreaEntry = GetAreaEntryByAreaID(destAreaId);

            const char *mapName = mapEntry ? reinterpret_cast<const char *>(mapEntry->name) : "<unknown>";
            const char *srcZoneName = srcZoneEntry ? reinterpret_cast<const char *>(srcZoneEntry->area_name) : "<unknown>";
            const char *srcAreaName = srcAreaEntry ? reinterpret_cast<const char *>(srcAreaEntry->area_name) : "<unknown>";
            const char *destZoneName = destZoneEntry ? reinterpret_cast<const char *>(destZoneEntry->area_name) : "<unknown>";
            const char *destAreaName = destAreaEntry ? reinterpret_cast<const char *>(destAreaEntry->area_name) : "<unknown>";
            
            auto const cheatType = distance > 150.f ? CHEAT_TYPE_TELEPORT_FAR : CHEAT_TYPE_TELEPORT;

            if (sAnticheatConfig.IsEnabled(cheatType))
                _anticheat->RecordCheatInternal(
                    cheatType,
                    "Teleport hack: player %s, %s, %.2f yd\n"
                    "    map %u \"%s\"\n"
                    "    source: zone %u \"%s\" area %u \"%s\" %.2f, %.2f, %.2f\n"
                    "    dest:   zone %u \"%s\" area %u \"%s\" %.2f, %.2f, %.2f",
                    mover->GetName(), LookupOpcodeName(opcode), distance,
                    mover->GetMapId(), mapName,
                    mover->GetZoneId(), srcZoneName, mover->GetAreaId(), srcAreaName,
                    mover->GetPositionX(), mover->GetPositionY(), mover->GetPositionZ(),
                    destZoneId, destZoneName, destAreaId, destAreaName,
                    movementInfo.pos.x, movementInfo.pos.y, movementInfo.pos.z);

            // on gm island? (普通玩家进入 GM 岛区域 876 触发禁区告警)
            if (sAnticheatConfig.IsEnabled(CHEAT_TYPE_FORBIDDEN) && _me->GetSession()->GetSecurity() == SEC_PLAYER && destZoneId == 876 && destAreaId == 876)
                _anticheat->RecordCheatInternal(CHEAT_TYPE_FORBIDDEN, "Player is on GM Island");

            // save prevoius point (瞬移被拦截时将上次合法坐标写入数据库)
            Player::SavePositionInDB(mover->GetObjectGuid(), mover->GetMapId(),
                mover->m_movementInfo.pos.x,
                mover->m_movementInfo.pos.y,
                mover->m_movementInfo.pos.z,
                mover->m_movementInfo.pos.o, mover->GetZoneId());

            return false;
        }
    }

    // in launched/falling/spline movement - maybe false positives (非击飞/样条控制状态下严格清理非法移动标志位)
    if (!mover->IsLaunched() && (!mover->movespline || mover->movespline->Finalized()))
    {
        auto const moveFlags = movementInfo.GetMovementFlags();
        uint32 removeFlags;

        VerifyMovementFlags(moveFlags, removeFlags, true);

        if (!!removeFlags)
        {
            // XXX temporary debug logging
            sLog.outBasic("ANTICHEAT: Player %s sent bad movement flags (0x%x) with opcode %s (%u)",
                mover->GetName(), moveFlags, LookupOpcodeName(opcode), opcode);
            movementInfo.RemoveMovementFlag(MovementFlags(removeFlags));
        }
    }

    return true;
}

// 向 GM 输出当前待确认移动指令及详细移动调试日志
void Movement::SendOrderInfo(ChatHandler *handler) const
{
    handler->PSendSysMessage("Order ACKs pending: %lu Current time: %u", uint64(_orders.size()), WorldTimer::getMSTime());

    if (!_orders.empty())
    {
        static constexpr size_t maxCount = 10;

        size_t count = 0;
        for (auto const &order : _orders)
        {
            handler->PSendSysMessage("Next: %s Age: %ums", LookupOpcodeName(order.opcode), WorldTimer::getMSTime() - order.time);

            if (++count >= maxCount)
                break;
        }
    }
    
    std::string str;

    _anticheat->GetMovementDebugString(str);

    handler->PSendSysMessage("%s", str.c_str());
}

// 导出近期服务端下发移动指令历史文本
size_t Movement::DumpOrders(std::string &out) const
{
    std::stringstream str;

    size_t result = 0;

    auto const now = WorldTimer::getMSTime();

    for (auto const &order : _orderHistory)
    {
        str << LookupOpcodeName(order.opcode) << " at " << order.time
            << " ("<< WorldTimer::getMSTimeDiff(order.time, now) << "ms ago)\n";
        ++result;
    }

    out = str.str();

    return result;
}

// 导出近期客户端移动确认历史文本
size_t Movement::DumpAcks(std::string &out) const
{
    std::stringstream str;

    size_t result = 0;

    auto const now = WorldTimer::getMSTime();

    for (auto const &ack : _ackHistory)
    {
        str << LookupOpcodeName(ack.opcode) << " at " << ack.time << " ("
            << WorldTimer::getMSTimeDiff(ack.time, now) << "ms ago)\n";
        ++result;
    }

    out = str.str();

    return result;
}

// 导出近期客户端时间跳变历史文本
size_t Movement::DumpTimeSkips(std::string &out) const
{
    std::stringstream str;

    size_t result = 0;

    auto const now = WorldTimer::getMSTime();

    for (auto const &skip : _timeSkipHistory)
    {
        str << skip.mover.GetCounter() << ": Skipped " << skip.ms << " ms at time "
            << skip.time << " (" << WorldTimer::getMSTimeDiff(skip.time, now) << "ms ago)\n";
        ++result;
    }

    out = str.str();

    return result;
}

// 导出近期进入/离开世界记录
size_t Movement::DumpWorldChanges(std::string &out) const
{
    std::stringstream str;

    size_t result = 0;

    for (auto const &change : _worldChangeHistory)
    {
        str << (change.enter ? "Enter" : "Leave") << " at time " << change.time << "\n";
        ++result;
    }

    out = str.str();

    return result;
}

// 导出近期移动历史包明细
size_t Movement::DumpMovement(std::string &out) const
{
    std::stringstream str;

    size_t result = 0;

    auto const now = WorldTimer::getMSTime();

    for (auto const &move : _moveHistory)
    {
        str << move.movementInfo.ctime << ": age " << (now - move.movementInfo.stime) << " flags 0x" << std::hex << move.movementInfo.moveFlags << std::dec
            << " at (" << move.movementInfo.pos.x << ", " << move.movementInfo.pos.y << ", " << move.movementInfo.pos.z
            << ")";

        if (move.movementInfo.HasMovementFlag(MOVEFLAG_FALLING))
            str << " jump xy speed " << move.movementInfo.jump.xyspeed << " start ctime "
                << move.movementInfo.jump.startClientTime;

        str << " " << LookupOpcodeName(move.opcode) << "\n";

        ++result;
    }

    out = str.str();

    return result;
}
}
