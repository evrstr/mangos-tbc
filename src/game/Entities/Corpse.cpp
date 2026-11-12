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

#include "Entities/Corpse.h"
#include "Entities/Player.h"
#include "Globals/ObjectAccessor.h"
#include "Entities/ObjectGuid.h"
#include "Database/DatabaseEnv.h"
#include "World/World.h"
#include "Globals/ObjectMgr.h"

// 构造尸体对象，初始化对象类型掩码、更新标志与创建时间
Corpse::Corpse(CorpseType type) : WorldObject(),
    lootRecipient(nullptr),
    lootForBody(false)
{
    m_objectType |= TYPEMASK_CORPSE;
    m_objectTypeId = TYPEID_CORPSE;
    // 2.3.2 - 0x58 (2.3.2 起尸体更新标志掩码)
    m_updateFlag = (UPDATEFLAG_LOWGUID | UPDATEFLAG_HIGHGUID | UPDATEFLAG_HAS_POSITION);

    m_valuesCount = CORPSE_END;

    m_type = type;

    m_time = time(nullptr);
}

Corpse::~Corpse()
{
}

// 加入世界并注册到 ObjectAccessor 以支持 GUID 查找
void Corpse::AddToWorld()
{
    ///- Register the corpse for guid lookup (注册尸体以供 GUID 查询)
    if (!IsInWorld())
        sObjectAccessor.AddObject(this);

    WorldObject::AddToWorld();
}

// 从 ObjectAccessor 注销并移出世界
void Corpse::RemoveFromWorld()
{
    ///- Remove the corpse from the accessor (从 ObjectAccessor 中移除尸体)
    if (IsInWorld())
        sObjectAccessor.RemoveObject(this);

    Object::RemoveFromWorld();
}

// 仅使用低位 GUID 初始化尸体基础对象
bool Corpse::Create(uint32 guidlow)
{
    Object::_Create(guidlow, guidlow, 0, HIGHGUID_CORPSE);
    return true;
}

// 基于所属玩家当前位置与地图信息创建尸体对象
bool Corpse::Create(uint32 guidlow, Player* owner)
{
    MANGOS_ASSERT(owner);

    WorldObject::_Create(guidlow, HIGHGUID_CORPSE);
    Relocate(owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ(), owner->GetOrientation());

    // we need to assign owner's map for corpse
    // in other way we will get a crash in Corpse::SaveToDB()
    // 必须为尸体绑定所属玩家的地图指针，否则后续 Corpse::SaveToDB() 会崩溃
    SetMap(owner->GetMap());

    if (!IsPositionValid())
    {
        sLog.outError("Corpse (guidlow %d, owner %s) not created. Suggested coordinates isn't valid (X: %f Y: %f)",
                      guidlow, owner->GetName(), owner->GetPositionX(), owner->GetPositionY());
        return false;
    }

    SetObjectScale(DEFAULT_OBJECT_SCALE);
    SetFloatValue(CORPSE_FIELD_POS_X, GetPositionX());
    SetFloatValue(CORPSE_FIELD_POS_Y, GetPositionY());
    SetFloatValue(CORPSE_FIELD_POS_Z, GetPositionZ());
    SetFloatValue(CORPSE_FIELD_FACING, GetOrientation());
    SetOwnerGuid(owner->GetObjectGuid());

    m_grid = MaNGOS::ComputeGridPair(GetPositionX(), GetPositionY());

    return true;
}

// 将可复活尸体持久化至角色数据库（在事务中先删旧记录再插入）
void Corpse::SaveToDB()
{
    // bones should not be saved to DB (would be deleted on startup anyway) (骸骨不应存入数据库，启动时也会被清理)
    MANGOS_ASSERT(GetType() != CORPSE_BONES);

    // prevent DB data inconsistence problems and duplicates (防止数据库重复记录或不一致)
    CharacterDatabase.BeginTransaction();
    DeleteFromDB();

    std::ostringstream ss;
    ss  << "INSERT INTO corpse (guid,player,position_x,position_y,position_z,orientation,map,time,corpse_type,instance) VALUES ("
        << GetGUIDLow() << ", "
        << GetOwnerGuid().GetCounter() << ", "
        << GetPositionX() << ", "
        << GetPositionY() << ", "
        << GetPositionZ() << ", "
        << GetOrientation() << ", "
        << GetMapId() << ", "
        << uint64(m_time) << ", "
        << uint32(GetType()) << ", "
        << int(GetInstanceId()) << ")";
    CharacterDatabase.Execute(ss.str().c_str());
    CharacterDatabase.CommitTransaction();
}

// 将骸骨尸体加入待移除列表以从世界删除
void Corpse::DeleteBonesFromWorld()
{
    MANGOS_ASSERT(GetType() == CORPSE_BONES);
    Corpse* corpse = GetMap()->GetCorpse(GetObjectGuid());

    if (!corpse)
    {
        sLog.outError("Bones %u not found in world.", GetGUIDLow());
        return;
    }

    AddObjectToRemoveList();
}

// 从角色数据库删除该玩家的所有非骸骨尸体记录
void Corpse::DeleteFromDB() const
{
    // bones should not be saved to DB (would be deleted on startup anyway) (骸骨不应存入数据库)
    MANGOS_ASSERT(GetType() != CORPSE_BONES);

    // all corpses (not bones) (删除所有非骸骨类型的尸体记录)
    static SqlStatementID id;

    SqlStatement stmt = CharacterDatabase.CreateStatement(id, "DELETE FROM corpse WHERE player = ? AND corpse_type <> '0'");
    stmt.PExecute(GetOwnerGuid().GetCounter());
}

// 从数据库查询字段恢复尸体外观、装备缓存、阵营标志与空间位置
bool Corpse::LoadFromDB(uint32 lowguid, Field* fields)
{
    ////                                                    0            1       2                  3                  4                  5                   6
    // QueryResult *result = CharacterDatabase.Query("SELECT corpse.guid, player, corpse.position_x, corpse.position_y, corpse.position_z, corpse.orientation, corpse.map,"
    ////   7     8            9         10      11    12     13           14            15              16       17
    //    "time, corpse_type, instance, gender, race, class, playerBytes, playerBytes2, equipmentCache, guildId, playerFlags FROM corpse"
    uint32 playerLowGuid = fields[1].GetUInt32();
    float positionX     = fields[2].GetFloat();
    float positionY     = fields[3].GetFloat();
    float positionZ     = fields[4].GetFloat();
    float orientation   = fields[5].GetFloat();
    uint32 mapid        = fields[6].GetUInt32();

    Object::_Create(lowguid, lowguid, 0, HIGHGUID_CORPSE);

    m_time = time_t(fields[7].GetUInt64());
    m_type = CorpseType(fields[8].GetUInt32());

    if (m_type >= MAX_CORPSE_TYPE)
    {
        sLog.outError("%s Owner %s have wrong corpse type (%i), not load.", GetGuidStr().c_str(), GetOwnerGuid().GetString().c_str(), m_type);
        return false;
    }

    uint32 instanceid   = fields[9].GetUInt32();
    uint8 gender        = fields[10].GetUInt8();
    uint8 race          = fields[11].GetUInt8();
    uint8 _class        = fields[12].GetUInt8();
    uint32 playerBytes  = fields[13].GetUInt32();
    uint32 playerBytes2 = fields[14].GetUInt32();
    uint32 guildId      = fields[16].GetUInt32();
    uint32 playerFlags  = fields[17].GetUInt32();

    ObjectGuid guid = ObjectGuid(HIGHGUID_CORPSE, lowguid);
    ObjectGuid playerGuid = ObjectGuid(HIGHGUID_PLAYER, playerLowGuid);

    // overwrite possible wrong/corrupted guid (覆写可能存在偏差或损坏的 GUID)
    SetGuidValue(OBJECT_FIELD_GUID, guid);
    SetOwnerGuid(playerGuid);

    SetObjectScale(DEFAULT_OBJECT_SCALE);

    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(race, _class);
    if (!info)
    {
        sLog.outError("Player %u has incorrect race/class pair.", GetGUIDLow());
        return false;
    }
    SetUInt32Value(CORPSE_FIELD_DISPLAY_ID, gender == GENDER_FEMALE ? info->displayId_f : info->displayId_m);

    // Load equipment (根据装备缓存填充尸体展示装备槽位)
    Tokens data = StrSplit(fields[15].GetCppString(), " ");
    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        uint32 visualbase = slot * 2;
        uint32 item_id = GetUInt32ValueFromArray(data, visualbase);
        const ItemPrototype* proto = ObjectMgr::GetItemPrototype(item_id);
        if (!proto)
        {
            SetUInt32Value(CORPSE_FIELD_ITEM + slot, 0);
            continue;
        }

        SetUInt32Value(CORPSE_FIELD_ITEM + slot, proto->DisplayInfoID | (proto->InventoryType << 24));
    }

    uint8 skin       = (uint8)(playerBytes);
    uint8 face       = (uint8)(playerBytes >> 8);
    uint8 hairstyle  = (uint8)(playerBytes >> 16);
    uint8 haircolor  = (uint8)(playerBytes >> 24);
    uint8 facialhair = (uint8)(playerBytes2);
    SetUInt32Value(CORPSE_FIELD_BYTES_1, ((0x00) | (race << 8) | (gender << 16) | (skin << 24)));
    SetUInt32Value(CORPSE_FIELD_BYTES_2, ((face) | (hairstyle << 8) | (haircolor << 16) | (facialhair << 24)));

    SetUInt32Value(CORPSE_FIELD_GUILD, guildId);

    uint32 flags = 0;
    if (playerFlags & PLAYER_FLAGS_HIDE_HELM)
        flags |= CORPSE_FLAG_HIDE_HELM;
    if (playerFlags & PLAYER_FLAGS_HIDE_CLOAK)
        flags |= CORPSE_FLAG_HIDE_CLOAK;
    SetUInt32Value(CORPSE_FIELD_FLAGS, flags);

    // no need to mark corpse as lootable, because corpses are not saved in battle grounds (战场尸体不入库，无需在此恢复可拾取标志)

    // place (设置尸体所在地图、副本与坐标)
    SetLocationInstanceId(instanceid);
    SetLocationMapId(mapid);
    Relocate(positionX, positionY, positionZ, orientation);

    if (!IsPositionValid())
    {
        sLog.outError("%s Owner %s not created. Suggested coordinates isn't valid (X: %f Y: %f)",
                      GetGuidStr().c_str(), GetOwnerGuid().GetString().c_str(), GetPositionX(), GetPositionY());
        return false;
    }

    m_grid = MaNGOS::ComputeGridPair(GetPositionX(), GetPositionY());

    return true;
}

// 根据种族 DBC 获取尸体所属阵营模板 ID
uint32 Corpse::GetFaction() const
{
    if (const uint8 race = getRace())
    {
        if (const ChrRacesEntry* raceEntry = sChrRacesStore.LookupEntry(race))
            return raceEntry->FactionID;
    }
    return 0;
}

// 判断尸体对观察者玩家是否处于可见距离内
bool Corpse::isVisibleForInState(Player const* u, WorldObject const* viewPoint, bool /*inVisibleList*/) const
{
    return IsInWorld() && u->IsInWorld() && IsWithinDistInMap(viewPoint, GetMap()->GetVisibilityDistance(), false);
}

// 判断尸体是否过期（骸骨保留 60 分钟，普通尸体保留 3 天）
bool Corpse::IsExpired(time_t t) const
{
    if (m_type == CORPSE_BONES)
        return m_time < t - 60 * MINUTE;
    return m_time < t - 3 * DAY;
}

// 根据尸体种族返回阵营（联盟 / 部落）
Team Corpse::GetTeam() const
{
    return Player::TeamForRace(getRace());
}
