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

#include "Entities/Pet.h"
#include "Database/DatabaseEnv.h"
#include "Log/Log.h"
#include "Server/WorldPacket.h"
#include "Globals/ObjectMgr.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"
#include "Tools/Formulas.h"
#include "Spells/SpellAuras.h"
#include "Entities/Unit.h"
#include "Entities/Transports.h"

// numbers represent minutes * 100 while happy (you get 100 loyalty points per min while happy) (各忠诚度等级升级所需点数，快乐状态下每分钟获得 100 点)
uint32 const LevelUpLoyalty[6] =
{
    5500,
    11500,
    17000,
    23500,
    31000,
    39500,
};

// 各忠诚度等级降级后或初始化时的起始点数
uint32 const LevelStartLoyalty[6] =
{
    2000,
    4500,
    7000,
    10000,
    13500,
    17500,
};

// 构造函数：初始化宠物默认属性与 CharmInfo 控制信息
Pet::Pet(PetType type) :
    Creature(CREATURE_SUBTYPE_PET),
    m_TrainingPoints(0), m_resetTalentsCost(0), m_resetTalentsTime(0),
    m_removed(false), m_happinessTimer(7500), m_loyaltyTimer(12000), m_petType(type), m_duration(0),
    m_loyaltyPoints(0), m_loading(false),
    m_xpRequiredForNextLoyaltyLevel(0), m_declinedname(nullptr),
    m_petModeFlags(PET_MODE_DEFAULT), m_originalCharminfo(nullptr), m_inStatsUpdate(false), m_scaleWithCls(false)
{
    m_name = "Pet";

    // pets always have a charminfo, even if they are not actually charmed
    InitCharmInfo(this);
}

// 析构函数：释放附身前保存的原始控制信息及屈折语名称
Pet::~Pet()
{
    delete m_originalCharminfo;
    delete m_declinedname;
}

// 将宠物注册到地图对象存储并加入世界
void Pet::AddToWorld()
{
    ///- Register the pet for guid lookup
    if (!IsInWorld())
        GetMap()->GetObjectsStore().insert<Pet>(GetObjectGuid(), (Pet*)this);

    Unit::AddToWorld();
}

// 将宠物从地图对象存储移除并退出世界（非玩家控制的临时宠物先执行解散清理）
void Pet::RemoveFromWorld()
{
    ///- Remove the pet from the accessor
    if (IsInWorld())
    {
        if (!m_removed && !IsPlayerControlled())
            Unsummon(PET_SAVE_NOT_IN_SLOT, nullptr, false);

        GetMap()->GetObjectsStore().erase<Pet>(GetObjectGuid(), (Pet*)nullptr);
    }

    ///- Don't call the function for Creature, normal mobs + totems go in a different storage
    Unit::RemoveFromWorld();
}

// 预检是否可从数据库加载指定宠物（供召唤法术 CheckCast 校验存在性、驯服性及存活状态）
SpellCastResult Pet::TryLoadFromDB(Unit* owner, uint32 petentry /*= 0*/, uint32 petnumber /*= 0*/, bool current /*= false*/, PetType mandatoryPetType /*= MAX_PET_TYPE*/)
{
    uint32 ownerid = owner->GetGUIDLow();

    std::ostringstream request;
    //                 0      1          2               3
    request << "SELECT entry, curhealth, CreatedBySpell, PetType ";

    if (petnumber)
        // known petnumber entry
        request << "FROM character_pet WHERE owner = '" << ownerid << "' AND id = '" << petnumber << "'";
    else if (current)
        // current pet (slot 0)
        request << "FROM character_pet WHERE owner = '" << ownerid << "' AND slot = '" << uint32(PET_SAVE_AS_CURRENT) << "'";
    else if (petentry)
        // known petentry entry (unique for summoned pet, but non unique for hunter pet (only from current or not stabled pets)
        request << "FROM character_pet WHERE owner = '" << ownerid << "' AND entry = '" << petentry << "' AND (slot = '" << uint32(PET_SAVE_AS_CURRENT) << "' OR slot > '" << uint32(PET_SAVE_LAST_STABLE_SLOT) << "')";
    else
        // any current or other non-stabled pet (for hunter "call pet")
        request << "FROM character_pet WHERE owner = '" << ownerid << "' AND (slot = '" << uint32(PET_SAVE_AS_CURRENT) << "' OR slot > '" << uint32(PET_SAVE_LAST_STABLE_SLOT) << "')";

    auto queryResult = CharacterDatabase.PQuery("%s", request.str().c_str());

    if (!queryResult)
        return SPELL_FAILED_NO_PET;

    Field* fields = queryResult->Fetch();

    petentry                    = fields[0].GetUInt32();
    uint32 savedHealth          = fields[1].GetUInt32();
    uint32 summon_spell_id      = fields[2].GetUInt32();
    PetType petType             = PetType(fields[3].GetUInt8());

    // update for case of current pet "slot = 0"
    if (!petentry)
        return SPELL_FAILED_NO_PET;

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(petentry);
    if (!creatureInfo)
    {
        sLog.outError("Pet entry %u does not exist but used at pet load (owner: %s).", petentry, owner->GetGuidStr().c_str());
        return SPELL_FAILED_NO_PET;
    }

    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(summon_spell_id);

    bool isTemporarySummoned = spellInfo && GetSpellDuration(spellInfo) > 0;

    // check temporary summoned pets like mage water elemental
    if (current && isTemporarySummoned)
        return SPELL_FAILED_NO_PET;

    if (petType == HUNTER_PET && !creatureInfo->isTameable())
        return SPELL_FAILED_NO_PET;

    if (!savedHealth)
        return SPELL_FAILED_TARGETS_DEAD;

    if (mandatoryPetType != MAX_PET_TYPE && petType != mandatoryPetType)
        return SPELL_FAILED_BAD_TARGETS;

    return SPELL_CAST_OK; // If errors occur down the line, one must think about data consistency
}

// 从数据库 character_pet 表加载宠物实例、属性、技能栏、光环与冷却并加入地图
bool Pet::LoadPetFromDB(Player* owner, Position const& spawnPos, uint32 petentry /*= 0*/, uint32 petnumber /*= 0*/, bool current /*= false*/, uint32 healthPercentage /*= 0*/, bool permanentOnly /*= false*/, bool forced /*= false*/)
{
    m_loading = true;

    uint32 ownerid = owner->GetGUIDLow();

    std::unique_ptr<QueryResult> queryResult;

    if (petnumber)
        // known petnumber entry                       0   1      2      3        4      5    6           7              8        9           10    11    12       13         14       15            16      17              18        19                 20                 21              22       23
        queryResult = CharacterDatabase.PQuery("SELECT id, entry, owner, modelid, level, exp, Reactstate, loyaltypoints, loyalty, trainpoint, slot, name, renamed, curhealth, curmana, curhappiness, abdata, TeachSpelldata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType, xpForNextLoyalty "
                                          "FROM character_pet WHERE owner = '%u' AND id = '%u'",
                                          ownerid, petnumber);
    else if (current)
        // current pet (slot 0)                        0   1      2      3        4      5    6           7              8        9           10    11    12       13         14       15            16      17              18        19                 20                 21              22       23
        queryResult = CharacterDatabase.PQuery("SELECT id, entry, owner, modelid, level, exp, Reactstate, loyaltypoints, loyalty, trainpoint, slot, name, renamed, curhealth, curmana, curhappiness, abdata, TeachSpelldata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType, xpForNextLoyalty "
                                          "FROM character_pet WHERE owner = '%u' AND slot = '%u'",
                                          ownerid, PET_SAVE_AS_CURRENT);
    else if (petentry)
        // known petentry entry (unique for summoned pet, but non unique for hunter pet (only from current or not stabled pets)
        //                                             0   1      2      3        4      5    6           7              8        9           10    11    12       13         14       15            16      17              18        19                 20                 21              22       23
        queryResult = CharacterDatabase.PQuery("SELECT id, entry, owner, modelid, level, exp, Reactstate, loyaltypoints, loyalty, trainpoint, slot, name, renamed, curhealth, curmana, curhappiness, abdata, TeachSpelldata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType, xpForNextLoyalty "
                                          "FROM character_pet WHERE owner = '%u' AND entry = '%u' AND (slot = '%u' OR slot > '%u') ",
                                          ownerid, petentry, PET_SAVE_AS_CURRENT, PET_SAVE_LAST_STABLE_SLOT);
    else
        // any current or other non-stabled pet (for hunter "call pet")
        //                                             0   1      2      3        4      5    6           7              8        9           10    11    12       13         14       15            16      17              18        19                 20                 21              22       23
        queryResult = CharacterDatabase.PQuery("SELECT id, entry, owner, modelid, level, exp, Reactstate, loyaltypoints, loyalty, trainpoint, slot, name, renamed, curhealth, curmana, curhappiness, abdata, TeachSpelldata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType, xpForNextLoyalty "
                                          "FROM character_pet WHERE owner = '%u' AND (slot = '%u' OR slot > '%u') ",
                                          ownerid, PET_SAVE_AS_CURRENT, PET_SAVE_LAST_STABLE_SLOT);

    if (!queryResult)
        return false;

    Field* fields = queryResult->Fetch();

    // update for case of current pet "slot = 0"
    petentry = fields[1].GetUInt32();
    if (!petentry)
    {
        return false;
    }

    CreatureInfo const* creatureInfo = ObjectMgr::GetCreatureTemplate(petentry);
    if (!creatureInfo)
    {
        sLog.outError("Pet entry %u does not exist but used at pet load (owner: %s).", petentry, owner->GetGuidStr().c_str());
        return false;
    }

    uint32 summon_spell_id = fields[21].GetUInt32();
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(summon_spell_id);

    if (permanentOnly && spellInfo && GetSpellDuration(spellInfo) > 0)
    {
        return false;
    }

    PetType pet_type = PetType(fields[22].GetUInt8());
    if (pet_type == HUNTER_PET)
    {
        if (!creatureInfo->isTameable())
        {
            return false;
        }
    }

    m_petType = pet_type;

    uint32 pet_number = fields[0].GetUInt32();

    if (!forced && owner->IsPetNeedBeTemporaryUnsummoned(nullptr))
    {
        // set temporary summon that way its possible if the player unmount to resummon it automaticaly
        owner->SetTemporaryUnsummonedPetNumber(pet_number);

        // change pet slot directly in database
        CharacterDatabase.BeginTransaction();
        static SqlStatementID ChangePetSlot_ID;
        SqlStatement ChangePetSlot = CharacterDatabase.CreateStatement(ChangePetSlot_ID, "UPDATE character_pet SET slot = ? WHERE id = ? ");
        ChangePetSlot.PExecute(uint32(PET_SAVE_AS_CURRENT), pet_number);
        CharacterDatabase.CommitTransaction();

        return false;
    }
    owner->SetTemporaryUnsummonedPetNumber(0);

    Map* map = owner->GetMap();

    CreatureCreatePos pos(owner->GetMap(), spawnPos.x, spawnPos.y, spawnPos.z, spawnPos.o);

    uint32 guid = pos.GetMap()->GenerateLocalLowGuid(HIGHGUID_PET, pet_number);
    if (!Create(guid, pos, creatureInfo, pet_number))
    {
        return false;
    }

    setPetType(pet_type);
    setFaction(owner->GetFaction());
    SetUInt32Value(UNIT_CREATED_BY_SPELL, summon_spell_id);

    // reget for sure use real creature info selected for Pet at load/creating
    CreatureInfo const* cinfo = GetCreatureInfo();
    if (cinfo->CreatureType == CREATURE_TYPE_CRITTER)
    {
        pos.GetMap()->Add((Creature*)this);
        AIM_Initialize();
        return true;
    }

    m_charmInfo->SetPetNumber(pet_number, isControlled());

    SetOwnerGuid(owner->GetObjectGuid());
    SetDisplayId(fields[3].GetUInt32());
    SetNativeDisplayId(fields[3].GetUInt32());
    uint32 petlevel = fields[4].GetUInt32();
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
    SetName(fields[11].GetString());

    SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    SetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_DEBUFF_LIMIT, UNIT_BYTE2_PLAYER_CONTROLLED_DEBUFF_LIMIT);

    if (getPetType() == HUNTER_PET)
    {
        // loyalty
        SetByteValue(UNIT_FIELD_BYTES_1, UNIT_BYTES_1_OFFSET_PET_LOYALTY, fields[8].GetUInt32());

        SetByteFlag(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_PET_FLAGS, fields[12].GetBool() ? UNIT_CAN_BE_ABANDONED : UNIT_CAN_BE_RENAMED | UNIT_CAN_BE_ABANDONED);

        SetTP(fields[9].GetInt32());
        SetMaxPower(POWER_HAPPINESS, GetCreatePowers(POWER_HAPPINESS));
        SetPower(POWER_HAPPINESS, fields[15].GetUInt32());
        SetPowerType(POWER_FOCUS);
        m_xpRequiredForNextLoyaltyLevel = fields[23].GetUInt32();
        m_loyaltyPoints = fields[7].GetInt32();
    }
    else if (getPetType() != SUMMON_PET)
        sLog.outError("Pet have incorrect type (%u) for pet loading.", getPetType());

    if (owner->IsImmuneToNPC())
        SetImmuneToNPC(true);

    if (owner->IsImmuneToPlayer())
        SetImmuneToPlayer(true);

    if (owner->IsPvP())
        SetPvP(true);

    SetCanModifyStats(true);
    InitStatsForLevel(petlevel);
    SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(nullptr)));
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, fields[5].GetUInt32());

    ReactStates reactState = ReactStates(fields[6].GetUInt8());

    uint32 savedhealth = fields[13].GetUInt32();
    uint32 savedpower = fields[14].GetUInt32();
    Powers powerType = GetPowerType();

    // load action bar, if data broken will fill later by default spells.
    m_charmInfo->LoadPetActionBar(fields[16].GetCppString());

    // init teach spells
    Tokens tokens = StrSplit(fields[17].GetString(), " ");
    Tokens::const_iterator iter;
    int index;
    for (iter = tokens.begin(), index = 0; index < 4; ++iter, ++index)
    {
        uint32 tmp = std::stoul((*iter).c_str());

        ++iter;

        if (tmp)
            AddTeachSpell(tmp, std::stoul((*iter).c_str()));
        else
            continue;
    }

    // since last save (in seconds)
    uint32 timediff = uint32(time(nullptr) - fields[18].GetUInt64());

    m_resetTalentsCost = fields[19].GetUInt32();
    m_resetTalentsTime = fields[20].GetUInt64();

    // load spells/cooldowns/auras
    _LoadAuras(timediff);

    // remove arena auras if in arena - but only DB loaded ones
    if (map->IsBattleArena())
        RemoveArenaAuras();

    // init AB
    LearnPetPassives();
    CastPetAuras(current);
    CastOwnerTalentAuras();

    // The following call was moved here to fix health is not full after pet invocation (before, they where placed after map->Add())
    _LoadSpells();

    UpdateAllStats();

    // failsafe check
    savedhealth = savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth;
    savedpower = savedpower > GetMaxPower(powerType) ? GetMaxPower(powerType) : savedpower;

    if (!savedhealth)
    {
        if (getPetType() == HUNTER_PET && healthPercentage)
        {
            savedhealth = GetMaxHealth() * (float(healthPercentage) / 100);
            savedpower = 0;
        }
        else if (getPetType() != SUMMON_PET)
            return false;
    }

    SetHealth(savedhealth);
    SetPower(powerType, savedpower);

    map->Add((Creature*)this);
    AIM_Initialize();

    AI()->SetReactState(reactState);

    // Spells should be loaded after pet is added to map, because in CheckCast is check on it
    CleanupActionBar();                                     // remove unknown spells from action bar after load

    _LoadSpellCooldowns();

    owner->SetPet(this);                                    // in DB stored only full controlled creature
    DEBUG_LOG("New Pet has guid %u", GetGUIDLow());

    ((Player*)owner)->PetSpellInitialize();

    if (owner->GetGroup())
        owner->SetGroupUpdateFlag(GROUP_UPDATE_PET);

    if (getPetType() == HUNTER_PET)
    {
        queryResult = CharacterDatabase.PQuery("SELECT genitive, dative, accusative, instrumental, prepositional FROM character_pet_declinedname WHERE owner = '%u' AND id = '%u'", owner->GetGUIDLow(), GetCharmInfo()->GetPetNumber());

        if (queryResult)
        {
            delete m_declinedname;
            m_declinedname = new DeclinedName;

            Field* fields2 = queryResult->Fetch();
            for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
                m_declinedname->name[i] = fields2[i].GetCppString();
        }
    }

    m_loading = false;

    SynchronizeLevelWithOwner();

    SavePetToDB(PET_SAVE_AS_CURRENT, owner);

    if (GenericTransport* transport = owner->GetTransport())
        transport->AddPetToTransport(owner, this);
    return true;
}

// 将当前宠物数据、技能、冷却与光环按指定槽位模式持久化到数据库（或删除）
void Pet::SavePetToDB(PetSaveMode mode, Player* owner)
{
    if (!GetEntry())
        return;

    // save only fully controlled creature
    if (!isControlled())
        return;

    // dont save shadowfiend
    if (owner->getClass() == CLASS_PRIEST)
        return;

    // current/stable/not_in_slot
    if (mode >= PET_SAVE_AS_CURRENT)
    {
        // reagents must be returned before save call
        if (mode == PET_SAVE_REAGENTS)
            mode = PET_SAVE_NOT_IN_SLOT;
        // not save pet as current if another pet temporary unsummoned
        else if (mode == PET_SAVE_AS_CURRENT && owner->GetTemporaryUnsummonedPetNumber() &&
                 owner->GetTemporaryUnsummonedPetNumber() != m_charmInfo->GetPetNumber())
        {
            // pet will lost anyway at restore temporary unsummoned
            if (getPetType() == HUNTER_PET)
                return;

            // for warlock case
            mode = PET_SAVE_NOT_IN_SLOT;
        }

        uint32 curhealth = GetHealth();
        uint32 curpower = GetPower(GetPowerType());

        // stable and not in slot saves
        if (mode == PET_SAVE_AS_CURRENT)
        {
            // pet is dead so it doesn't have to be shown at character login
            if (!IsAlive())
                mode = PET_SAVE_NOT_IN_SLOT;
        }
        else
        {
            if (getPetType() != HUNTER_PET)
                RemoveAllAuras();
        }

        // save pet's data as one single transaction
        CharacterDatabase.BeginTransaction();
        _SaveSpells();
        _SaveSpellCooldowns();
        _SaveAuras();

        uint32 loyalty = 1;
        if (getPetType() == HUNTER_PET)
            loyalty = GetLoyaltyLevel();

        uint32 ownerLow = GetOwnerGuid().GetCounter();
        // remove current data
        static SqlStatementID delPet ;
        static SqlStatementID insPet ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM character_pet WHERE owner = ? AND id = ?");
        stmt.PExecute(ownerLow, m_charmInfo->GetPetNumber());

        // prevent duplicate using slot (except PET_SAVE_NOT_IN_SLOT)
        if (mode <= PET_SAVE_LAST_STABLE_SLOT)
        {
            static SqlStatementID updPet ;

            stmt = CharacterDatabase.CreateStatement(updPet, "UPDATE character_pet SET slot = ? WHERE owner = ? AND slot = ?");
            stmt.PExecute(uint32(PET_SAVE_NOT_IN_SLOT), ownerLow, uint32(mode));
        }

        // prevent existence another hunter pet in PET_SAVE_AS_CURRENT and PET_SAVE_NOT_IN_SLOT
        if (getPetType() == HUNTER_PET && (mode == PET_SAVE_AS_CURRENT || mode > PET_SAVE_LAST_STABLE_SLOT))
        {
            static SqlStatementID del ;

            stmt = CharacterDatabase.CreateStatement(del, "DELETE FROM character_pet WHERE owner = ? AND (slot = ? OR slot > ?)");
            stmt.PExecute(ownerLow, uint32(PET_SAVE_AS_CURRENT), uint32(PET_SAVE_LAST_STABLE_SLOT));
        }

        // save pet
        SqlStatement savePet = CharacterDatabase.CreateStatement(insPet, "INSERT INTO character_pet "
                               "( id, entry,  owner, modelid, level, exp, Reactstate, loyaltypoints, loyalty, trainpoint, slot, name, renamed, curhealth, curmana, curhappiness, abdata, TeachSpelldata, savetime, resettalents_cost, resettalents_time, CreatedBySpell, PetType, xpForNextLoyalty) "
                               "VALUES ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        savePet.addUInt32(m_charmInfo->GetPetNumber());
        savePet.addUInt32(GetEntry());
        savePet.addUInt32(ownerLow);
        savePet.addUInt32(GetNativeDisplayId());
        savePet.addUInt32(GetLevel());
        savePet.addUInt32(GetUInt32Value(UNIT_FIELD_PETEXPERIENCE));
        savePet.addUInt32(uint32(AI()->GetReactState()));
        savePet.addInt32(m_loyaltyPoints);
        savePet.addUInt32(GetLoyaltyLevel());
        savePet.addInt32(m_TrainingPoints);
        savePet.addUInt32(uint32(mode));
        savePet.addString(m_name);
        savePet.addUInt32(uint32(HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED) ? 0 : 1));
        savePet.addUInt32(curhealth);
        savePet.addUInt32(curpower);
        savePet.addUInt32(GetPower(POWER_HAPPINESS));

        std::ostringstream ss;
        for (uint32 i = ACTION_BAR_INDEX_START; i < ACTION_BAR_INDEX_END; ++i)
        {
            ss << uint32(m_charmInfo->GetActionBarEntry(i)->GetType()) << " "
               << uint32(m_charmInfo->GetActionBarEntry(i)->GetAction()) << " ";
        }
        savePet.addString(ss);

        // save spells the pet can teach to it's Master
        {
            int i = 0;
            for (TeachSpellMap::const_iterator itr = m_teachspells.begin(); i < 4 && itr != m_teachspells.end(); ++i, ++itr)
                ss << itr->first << " " << itr->second << " ";
            for (; i < 4; ++i)
                ss << uint32(0) << " " << uint32(0) << " ";
        }
        savePet.addString(ss);

        savePet.addUInt64(uint64(time(nullptr)));
        savePet.addUInt32(uint32(m_resetTalentsCost));
        savePet.addUInt64(uint64(m_resetTalentsTime));
        savePet.addUInt32(GetUInt32Value(UNIT_CREATED_BY_SPELL));
        savePet.addUInt32(uint32(getPetType()));

        // loyalty values should only be saved for hunter pets
        if (getPetType() == HUNTER_PET)
            savePet.addUInt32(m_xpRequiredForNextLoyaltyLevel);
        else
            savePet.addUInt32(0);

        savePet.Execute();
        CharacterDatabase.CommitTransaction();
    }
    else
    {
        RemoveAllAuras(AURA_REMOVE_BY_DELETE);
        DeleteFromDB(m_charmInfo->GetPetNumber());
    }
}

// 计算宠物在主人身侧的默认出生坐标（右偏 90 度、距离 2 码且避开碰撞）
Position Pet::GetPetSpawnPosition(Player* owner)
{
    Position pos;
    owner->GetFirstCollisionPosition(pos, 2.f, owner->GetOrientation() + M_PI_F / 2);
    return pos;
}

// 按宠物编号从数据库彻底删除宠物及其光环、法术、冷却与词形名称记录
void Pet::DeleteFromDB(uint32 guidlow, bool separate_transaction)
{
    if (separate_transaction)
        CharacterDatabase.BeginTransaction();

    static SqlStatementID delPet ;
    static SqlStatementID delDeclName ;
    static SqlStatementID delAuras ;
    static SqlStatementID delSpells ;
    static SqlStatementID delSpellCD ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delPet, "DELETE FROM character_pet WHERE id = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delDeclName, "DELETE FROM character_pet_declinedname WHERE id = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM pet_aura WHERE guid = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delSpells, "DELETE FROM pet_spell WHERE guid = ?");
    stmt.PExecute(guidlow);

    stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM pet_spell_cooldown WHERE guid = ?");
    stmt.PExecute(guidlow);

    if (separate_transaction)
        CharacterDatabase.CommitTransaction();
}

// 按主人和槽位查找并删除数据库中的宠物记录
void Pet::DeleteFromDB(Unit* owner, PetSaveMode slot)
{
    auto queryResult = CharacterDatabase.PQuery("SELECT id FROM character_pet WHERE owner = '%u'  AND slot = '%u' ", owner->GetGUIDLow(), uint32(slot));
    if (queryResult)
    {
        Field* fields = queryResult->Fetch();
        uint32 petNumber = fields[0].GetUInt32();

        if (petNumber)
            DeleteFromDB(petNumber);
    }
}

// 设置宠物拥有者 GUID（召唤/猎人宠物同步记录召唤者 GUID）
void Pet::SetOwnerGuid(ObjectGuid owner)
{
    switch (uint32(m_petType))
    {
        case SUMMON_PET:
        case HUNTER_PET:
            SetSummonerGuid(owner);
            break;
    }
    Unit::SetOwnerGuid(owner);
}

// 获取法术修正所属的玩家（即宠物主人）
Player* Pet::GetSpellModOwner() const
{
    Unit* owner = GetOwner();
    if (owner && owner->GetTypeId() == TYPEID_PLAYER)
        return static_cast<Player*>(owner);
    return nullptr;
}

// 处理宠物死亡与复活状态切换（禁用尸体拾取、扣除战场外快乐度、刷新主人天赋光环）
void Pet::SetDeathState(DeathState s)                       // overwrite virtual Creature::SetDeathState and Unit::SetDeathState
{
    Creature::SetDeathState(s);
    if (GetDeathState() == CORPSE) // will despawn at corpse despawning (Pet::Update code)
    {
        // pet corpse non lootable and non skinnable
        SetUInt32Value(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_NONE);
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);

        if (getPetType() != SUMMON_PET)
        {
            // lose happiness when died and not in BG/Arena
            MapEntry const* mapEntry = sMapStore.LookupEntry(GetMapId());
            if (!mapEntry || (mapEntry->map_type != MAP_ARENA && mapEntry->map_type != MAP_BATTLEGROUND))
                ModifyPower(POWER_HAPPINESS, -HAPPINESS_LEVEL_SIZE);
        }

        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);

        if (Unit* owner = GetOwner())
        {
            if (getPetType() == GUARDIAN_PET)
                owner->RemoveGuardian(this);
        }
    }
    else if (GetDeathState() == ALIVE)
    {
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
        CastPetAuras(true);
    }
    CastOwnerTalentAuras();

    if (getPetType() == GUARDIAN_PET)
        if (Unit* owner = GetOwner())
            owner->RemoveGuardian(this);
}

// 宠物每帧更新：处理尸体过期消失、主人离线/超距解散以及临时召唤倒计时
void Pet::Update(const uint32 diff)
{
    if (m_removed)                                          // pet already removed, just wait in remove queue, no updates
        return;

    switch (m_deathState)
    {
        case CORPSE:
        {
            if (IsCorpseExpired())
            {
                // pet is dead so it doesn't have to be shown at character login
                Unsummon(PET_SAVE_NOT_IN_SLOT);
                return;
            }
            break;
        }
        case ALIVE:
        {
            // unsummon pet that lost owner
            Unit* owner = GetOwner();
            if ((!owner ||
                    (!IsWithinDistInMap(owner, GetMap()->GetVisibilityDistance()) && (owner->HasCharm() && !owner->HasCharm(GetObjectGuid()))) ||
                    (isControlled() && !owner->GetPetGuid())) && (!IsGuardian() || !IsInCombat()))
            {
                Unsummon(PET_SAVE_REAGENTS);
                return;
            }

            if (isControlled())
            {
                if (owner->GetPetGuid() != GetObjectGuid())
                {
                    Unsummon(getPetType() == HUNTER_PET ? PET_SAVE_AS_DELETED : PET_SAVE_NOT_IN_SLOT, owner);
                    return;
                }
            }

            if (m_duration > 0)
            {
                if (m_duration > (int32)diff)
                    m_duration -= (int32)diff;
                else
                {
                    Unsummon(getPetType() != SUMMON_PET ? PET_SAVE_AS_DELETED : PET_SAVE_NOT_IN_SLOT, owner);
                    return;
                }
            }
            break;
        }
        default:
            break;
    }

    Creature::Update(diff);
}

// 周期性回复生命与集中值/法力，并更新猎人宠物快乐度衰减与忠诚度变化
void Pet::RegenerateAll(uint32 diff)
{
    // regenerate focus for hunter pets
    m_regenTimer += diff;
    if (m_regenTimer >= 4000)
    {
        if (!IsInCombat())
            RegenerateHealth();

        RegeneratePower(4.f);
        m_regenTimer -= 4000;
    }

    if (getPetType() != HUNTER_PET)
        return;

    if (m_happinessTimer <= diff)
    {
        LooseHappiness();
        m_happinessTimer = 7500;
    }
    else
        m_happinessTimer -= diff;

    if (m_loyaltyTimer <= diff)
    {
        TickLoyaltyChange();
        m_loyaltyTimer = 12000;
    }
    else
        m_loyaltyTimer -= diff;
}

// 猎人宠物快乐度随时间衰减（战斗中衰减速度为平时的 1.5 倍）
void Pet::LooseHappiness()
{
    uint32 curValue = GetPower(POWER_HAPPINESS);
    if (curValue <= 0)
        return;
    int32 addvalue = (140 >> GetLoyaltyLevel()) * 125;      // value is 70/35/17/8/4 (per min) * 1000 / 8 (timer 7.5 secs)
    if (IsInCombat())                                       // we know in combat happiness fades faster, multiplier guess
        addvalue = int32(addvalue * 1.5);
    ModifyPower(POWER_HAPPINESS, -addvalue);
}

// 根据主人等级计算升至下一忠诚度等级所需的经验值（主人升级所需经验的 5%）
void Pet::SetRequiredXpForNextLoyaltyLevel()
{
    uint32 loyaltylevel = GetLoyaltyLevel();
    if (loyaltylevel >= BEST_FRIEND)
    {
        m_xpRequiredForNextLoyaltyLevel = 0;
        return;
    }
    Unit* owner = GetOwner();
    if (owner)
    {
        uint32 ownerLevel = owner->GetLevel();
        m_xpRequiredForNextLoyaltyLevel = ownerLevel < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) ? sObjectMgr.GetXPForLevel(ownerLevel) * 5 / 100 : sObjectMgr.GetXPForLevel(ownerLevel - 1) * 5 / 100;
    }
}

// 扣减升级忠诚度所需的剩余经验值
void Pet::UpdateRequireXpForNextLoyaltyLevel(uint32 xp)
{
    if (xp > m_xpRequiredForNextLoyaltyLevel)
        m_xpRequiredForNextLoyaltyLevel = 0;
    else
        m_xpRequiredForNextLoyaltyLevel -= xp;
}

// 增减忠诚度点数并处理升级或降级（最低忠诚度为负时可能叛逃、主动攻击或被动停留）
void Pet::ModifyLoyalty(int32 addvalue)
{
    uint32 loyaltylevel = GetLoyaltyLevel();

    if (addvalue > 0)                                       // only gain influenced, not loss
        addvalue = int32((float)addvalue * sWorld.getConfig(CONFIG_FLOAT_RATE_LOYALTY));

    if (loyaltylevel >= BEST_FRIEND && (addvalue + m_loyaltyPoints) > int32(GetMaxLoyaltyPoints(loyaltylevel)))
        return;

    m_loyaltyPoints += addvalue;

    CharmInfo* charmInfo = GetCharmInfo();

    if (m_loyaltyPoints < 0)
    {
        if (loyaltylevel > REBELLIOUS)
        {
            // level down
            --loyaltylevel;
            SetLoyaltyLevel(LoyaltyLevel(loyaltylevel));
            m_loyaltyPoints = GetStartLoyaltyPoints(loyaltylevel);
            SetTP(m_TrainingPoints - int32(GetLevel()));
            SetRequiredXpForNextLoyaltyLevel();
        }
        else
        {
            Unit* owner = GetOwner();
            if (owner && owner->GetTypeId() == TYPEID_PLAYER)
            {
                switch (urand(0, 2))
                {
                    case 0: // Abandon owner
                    {
                        WorldPacket data(SMSG_PET_BROKEN, 0);
                        ((Player*)GetOwner())->GetSession()->SendPacket(data);
                        Unsummon(PET_SAVE_AS_DELETED, GetOwner());
                        m_loyaltyPoints = 0;
                        break;
                    }
                    case 1: // Turn aggressive
                    {
                        charmInfo->SetIsRetreating();
                        AI()->SetReactState(ReactStates(REACT_AGGRESSIVE));
                        SetModeFlags(PetModeFlags(AI()->GetReactState() | charmInfo->GetCommandState() * 0x100));
                        m_loyaltyPoints = 500;
                        break;
                    }
                    case 2: // Stay + passive
                    {
                        AttackStop();
                        charmInfo->SetCommandState(COMMAND_STAY);
                        AI()->SetReactState(ReactStates(REACT_PASSIVE));
                        SetModeFlags(PetModeFlags(AI()->GetReactState() | charmInfo->GetCommandState() * 0x100));
                        m_loyaltyPoints = 500;
                        break;
                    }
                }
            }
        }
    }
    // level up
    else if (m_loyaltyPoints > int32(GetMaxLoyaltyPoints(loyaltylevel)) && m_xpRequiredForNextLoyaltyLevel <= 0)
    {
        ++loyaltylevel;
        SetLoyaltyLevel(LoyaltyLevel(loyaltylevel));
        m_loyaltyPoints = GetStartLoyaltyPoints(loyaltylevel);
        SetTP(m_TrainingPoints + GetLevel());
        SetRequiredXpForNextLoyaltyLevel();
    }
}

// 根据当前快乐度状态周期性调整宠物忠诚度
void Pet::TickLoyaltyChange()
{
    int32 addvalue;

    switch (GetHappinessState())
    {
        case HAPPY:   addvalue =  20; break;
        case CONTENT: addvalue =  10; break;
        case UNHAPPY: addvalue = -20; break;
        default:
            return;
    }
    ModifyLoyalty(addvalue);
}

// 击杀怪物后按等级与当前忠诚度给予额外忠诚度奖励
void Pet::KillLoyaltyBonus(uint32 level)
{
    if (level > 100)
        return;

    // at lower levels gain is faster | the lower loyalty the more loyalty is gained
    uint32 bonus = uint32(((100 - level) / 10) + (6 - GetLoyaltyLevel()));
    ModifyLoyalty(bonus);
}

// 获取宠物当前快乐度区间枚举（不高兴/满足/快乐）
HappinessState Pet::GetHappinessState() const
{
    if (GetPower(POWER_HAPPINESS) < HAPPINESS_LEVEL_SIZE)
        return UNHAPPY;
    if (GetPower(POWER_HAPPINESS) >= HAPPINESS_LEVEL_SIZE * 2)
        return HAPPY;
    return CONTENT;
}

// 设置宠物当前忠诚度等级字段
void Pet::SetLoyaltyLevel(LoyaltyLevel level)
{
    SetByteValue(UNIT_FIELD_BYTES_1, 1, level);
}

// 检查宠物是否还能学习更多主动技能（非被动法术链上限为 ACTIVE_SPELLS_MAX）
bool Pet::CanTakeMoreActiveSpells(uint32 spellid) const
{
    uint8  activecount = 1;
    uint32 chainstartstore[ACTIVE_SPELLS_MAX];

    if (IsPassiveSpell(spellid))
        return true;

    chainstartstore[0] = sSpellMgr.GetFirstSpellInChain(spellid);

    for (PetSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
            continue;

        if (IsPassiveSpell(itr->first))
            continue;

        uint32 chainstart = sSpellMgr.GetFirstSpellInChain(itr->first);

        uint8 x;

        for (x = 0; x < activecount; ++x)
        {
            if (chainstart == chainstartstore[x])
                break;
        }

        if (x == activecount)                               // spellchain not yet saved -> add active count
        {
            ++activecount;
            if (activecount > ACTIVE_SPELLS_MAX)
                return false;
            chainstartstore[x] = chainstart;
        }
    }
    return true;
}

// 检查宠物当前训练点数是否足够学习指定技能
bool Pet::HasTPForSpell(uint32 spellid) const
{
    int32 neededtrainp = GetTPForSpell(spellid);
    if ((m_TrainingPoints - neededtrainp < 0 || neededtrainp < 0) && neededtrainp != 0)
        return false;
    return true;
}

// 计算学习指定法术所需的净训练点数（扣除同法术链已花费的点数）
int32 Pet::GetTPForSpell(uint32 spellid) const
{
    uint32 basetrainp = 0;

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(spellid);
    if (bounds.first != bounds.second)
    {
        basetrainp = bounds.first->second->reqtrainpoints;
        if (basetrainp == 0)
            return 0;
    }

    uint32 spenttrainp = 0;
    uint32 chainstart = sSpellMgr.GetFirstSpellInChain(spellid);

    for (PetSpellMap::const_iterator itr = m_spells.begin(); itr != m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
            continue;

        if (sSpellMgr.GetFirstSpellInChain(itr->first) == chainstart)
        {
            SkillLineAbilityMapBounds _bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(itr->first);

            for (SkillLineAbilityMap::const_iterator _spell_idx2 = _bounds.first; _spell_idx2 != _bounds.second; ++_spell_idx2)
            {
                if (_spell_idx2->second->reqtrainpoints > spenttrainp)
                {
                    spenttrainp = _spell_idx2->second->reqtrainpoints;
                    break;
                }
            }
        }
    }

    return int32(basetrainp) - int32(spenttrainp);
}

// 获取指定忠诚度等级的升级所需点数上限（含边界保护）
uint32 Pet::GetMaxLoyaltyPoints(uint32 level) const
{
    if (level < 1) level = 1; // prevent SIGSEGV (out of range)
    if (level > 7) level = 7; // prevent SIGSEGV (out of range)
    return LevelUpLoyalty[level - 1];
}

// 获取指定忠诚度等级的起始点数（含边界保护）
uint32 Pet::GetStartLoyaltyPoints(uint32 level) const
{
    if (level < 1) level = 1; // prevent SIGSEGV (out of range)
    if (level > 7) level = 7; // prevent SIGSEGV (out of range)
    return LevelStartLoyalty[level - 1];
}

// 设置宠物剩余训练点数并同步更新客户端显示字段
void Pet::SetTP(int32 TP)
{
    m_TrainingPoints = TP;
    SetUInt32Value(UNIT_TRAINING_POINTS, (uint32)GetDispTP());
}

// 将内部训练点数转换为客户端约定的编码显示值
int32 Pet::GetDispTP() const
{
    if (getPetType() != HUNTER_PET)
        return (0);
    if (m_TrainingPoints < 0)
        return -m_TrainingPoints;
    else
        return -(m_TrainingPoints + 1);
}

// 解散宠物：停止战斗、返还材料或记录战场复活法术、移除动作栏、保存数据库并从世界移除
void Pet::Unsummon(PetSaveMode mode, Unit* owner /*= nullptr*/, bool removeList /*= true*/)
{
    MANGOS_ASSERT(!m_removed);

    m_removed = true;

    if (!owner)
        owner = GetOwner();

    if (IsInCombat())
        CombatStop(true);

    AI()->OnUnsummon();

    if (owner)
    {
        Player* p_owner = nullptr;

        if (GetOwnerGuid() != owner->GetObjectGuid())
            return;

        if (owner->GetTypeId() == TYPEID_PLAYER)
        {
            p_owner = static_cast<Player*>(owner);

            // not save secondary permanent pet as current
            if (mode == PET_SAVE_AS_CURRENT && p_owner->GetTemporaryUnsummonedPetNumber() &&
                    p_owner->GetTemporaryUnsummonedPetNumber() != GetCharmInfo()->GetPetNumber())
                mode = PET_SAVE_NOT_IN_SLOT;

            uint32 spellId = GetUInt32Value(UNIT_CREATED_BY_SPELL);
            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
            // save minion for resummoning at resurrection in BGs
            // only save the summon spell in the case where the minion did not die before the warlock, otherwise reset it
            if (mode == PET_SAVE_REAGENTS && p_owner->getClass() == CLASS_WARLOCK && p_owner->InBattleGround() && isControlled() && !isTemporarySummoned() && spellInfo)
                p_owner->SetBGPetSpell(spellId);
            else
                p_owner->SetBGPetSpell(0);

            if (mode == PET_SAVE_REAGENTS && !p_owner->InBattleGround()) // don't restore reagents in BGs - minion will be automatically ressurected
            {
                // returning of reagents only for players, so best done here
                if (spellInfo)
                {
                    for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
                    {
                        if (spellInfo->Reagent[i] > 0)
                        {
                            ItemPosCountVec dest;           // for succubus, voidwalker, felhunter and felguard credit soulshard when despawn reason other than death (out of range, logout)
                            uint8 msg = p_owner->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, spellInfo->Reagent[i], spellInfo->ReagentCount[i]);
                            if (msg == EQUIP_ERR_OK)
                            {
                                Item* item = p_owner->StoreNewItem(dest, spellInfo->Reagent[i], true);
                                if (p_owner->IsInWorld())
                                    p_owner->SendNewItem(item, spellInfo->ReagentCount[i], true, false);
                            }
                        }
                    }
                }
            }

            if (isControlled())
            {
                p_owner->RemovePetActionBar();

                if (p_owner->GetGroup())
                    p_owner->SetGroupUpdateFlag(GROUP_UPDATE_PET);
            }
        }

        if (p_owner)
            StartCooldown(p_owner);

        // only if current pet in slot
        switch (getPetType())
        {
            case MINI_PET:
                if (p_owner)
                    p_owner->SetMiniPet(nullptr);
                break;
            case GUARDIAN_PET:
                owner->RemoveGuardian(this);
                break;
            default:
                if (owner->GetPetGuid() == GetObjectGuid())
                    owner->SetPet(nullptr);
                break;
        }

        if (p_owner)
            SavePetToDB(mode, p_owner);
    }

    if (removeList)
        AddObjectToRemoveList();
}

// 给予猎人宠物经验值并处理连续升级、忠诚度经验及击杀奖励
void Pet::GivePetXP(uint32 xp)
{
    if (getPetType() != HUNTER_PET)
        return;

    if (xp < 1)
        return;

    if (!IsAlive())
        return;

    uint32 level = GetLevel();
    uint32 maxlevel = std::min(sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL), GetOwner()->GetLevel());

    // pet not receive xp for level equal to owner level
    if (level < maxlevel)
    {

        xp *= GetMap()->GetXPModRate(RateModType::PETKILL);

        uint32 nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
        uint32 curXP = GetUInt32Value(UNIT_FIELD_PETEXPERIENCE);
        uint32 newXP = curXP + xp;

        while (newXP >= nextLvlXP && level < maxlevel)
        {
            newXP -= nextLvlXP;
            ++level;

            GivePetLevel(level);                              // also update UNIT_FIELD_PETNEXTLEVELEXP and UNIT_FIELD_PETEXPERIENCE to level start

            nextLvlXP = GetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP);
        }

        SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, level < maxlevel ? newXP : 0);
    }

    UpdateRequireXpForNextLoyaltyLevel(xp);
    KillLoyaltyBonus(level);
}

// 将宠物提升或调整至指定等级并重算基础属性与训练点数
void Pet::GivePetLevel(uint32 level)
{
    if (!level || level == GetLevel())
        return;

    if (getPetType() == HUNTER_PET)
    {
        SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
        SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(level));
    }

    InitStatsForLevel(level);
    SetTP(m_TrainingPoints + (GetLoyaltyLevel() - 1));
}

// 以野外生物为蓝本创建被驯服的猎人宠物初始实体
bool Pet::CreateBaseAtCreature(Creature* creature)
{
    if (!creature)
    {
        sLog.outError("CRITICAL: nullptr pointer passed into CreateBaseAtCreature()");
        return false;
    }

    CreatureCreatePos pos(creature, creature->GetOrientation());

    BASIC_LOG("Create pet");
    uint32 pet_number = sObjectMgr.GeneratePetNumber();
    uint32 guid = creature->GetMap()->GenerateLocalLowGuid(HIGHGUID_PET, pet_number);
    if (!Create(guid, pos, creature->GetCreatureInfo(), pet_number))
        return false;

    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
    {
        sLog.outError("CreateBaseAtCreature() failed, creatureInfo is missing!");
        return false;
    }

    SetDisplayId(creature->GetDisplayId());
    SetNativeDisplayId(creature->GetNativeDisplayId());
    SetMaxPower(POWER_HAPPINESS, GetCreatePowers(POWER_HAPPINESS));
    SetPower(POWER_HAPPINESS, 166500);
    SetPowerType(POWER_FOCUS);
    SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, 0);
    SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
    SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(creature->GetLevel()));
    SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);

    if (CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family))
        SetName(cFamily->Name[sWorld.GetDefaultDbcLocale()]);
    else
        SetName(creature->GetNameForLocaleIdx(sObjectMgr.GetDbc2StorageLocaleIndex()));

    m_loyaltyPoints = 1000;

    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_CLASS, CLASS_WARRIOR);
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_GENDER, GENDER_NONE);
    SetByteValue(UNIT_FIELD_BYTES_0, UNIT_BYTES_0_OFFSET_POWER_TYPE, POWER_FOCUS);
    SetSheath(SHEATH_STATE_MELEE);

    SetByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED);

    SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    SetByteValue(UNIT_FIELD_BYTES_2, UNIT_BYTES_2_OFFSET_DEBUFF_LIMIT, UNIT_BYTE2_PLAYER_CONTROLLED_DEBUFF_LIMIT);

    SetUInt32Value(UNIT_MOD_CAST_SPEED, creature->GetUInt32Value(UNIT_MOD_CAST_SPEED));
    SetLoyaltyLevel(REBELLIOUS);

    return true;
}

// 按指定等级初始化宠物的基础属性、血量、法力、护甲、近战伤害及缩放光环
void Pet::InitStatsForLevel(uint32 petlevel)
{
    Unit* owner = GetOwner();
    CreatureInfo const* cInfo = GetCreatureInfo();
    MANGOS_ASSERT(cInfo);

    SetLevel(petlevel);

    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0);

    if (getPetType() == HUNTER_PET)
    {
        SetMeleeDamageSchool(SpellSchools(SPELL_SCHOOL_NORMAL));
        SetAttackTime(BASE_ATTACK, BASE_ATTACK_TIME);
        SetAttackTime(OFF_ATTACK, BASE_ATTACK_TIME);
        SetAttackTime(RANGED_ATTACK, BASE_ATTACK_TIME);
    }
    else
    {
        SetMeleeDamageSchool(SpellSchools(cInfo->DamageSchool));
        SetAttackTime(BASE_ATTACK, cInfo->MeleeBaseAttackTime);
        SetAttackTime(OFF_ATTACK, cInfo->MeleeBaseAttackTime);
        SetAttackTime(RANGED_ATTACK, cInfo->RangedBaseAttackTime);

        SetCreateResistance(SPELL_SCHOOL_HOLY, cInfo->ResistanceHoly);
        SetCreateResistance(SPELL_SCHOOL_FIRE, cInfo->ResistanceFire);
        SetCreateResistance(SPELL_SCHOOL_NATURE, cInfo->ResistanceNature);
        SetCreateResistance(SPELL_SCHOOL_FROST, cInfo->ResistanceFrost);
        SetCreateResistance(SPELL_SCHOOL_SHADOW, cInfo->ResistanceShadow);
        SetCreateResistance(SPELL_SCHOOL_ARCANE, cInfo->ResistanceArcane);
    }

    float health = 0.f;
    float mana = 0.f;
    float armor = 0.f;

    PetType petType = getPetType();
    if (!GetOwnerGuid().IsPlayer())
        petType = GUARDIAN_PET; // for purpose of pet scaling, NPC summoned SUMMON_PET scale as GUARDIAN_PET
    switch (petType)
    {
        case HUNTER_PET:
        {
            CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);

            if (cFamily && cFamily->minScale > 0.0f)
            {
                float scale;
                if (GetLevel() >= cFamily->maxScaleLevel)
                    scale = cFamily->maxScale;
                else if (GetLevel() <= cFamily->minScaleLevel)
                    scale = cFamily->minScale;
                else
                    scale = cFamily->minScale + float(GetLevel() - cFamily->minScaleLevel) / cFamily->maxScaleLevel * (cFamily->maxScale - cFamily->minScale);

                SetObjectScale(scale);
                UpdateModelData();
            }

            // Max level
            if (petlevel < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
                SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, sObjectMgr.GetXPForPetLevel(petlevel));
            else
            {
                SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
                SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);
            }

            // Info found in pet_levelstats
            if (PetLevelInfo const* pInfo = sObjectMgr.GetPetLevelInfo(1, petlevel))
            {
                for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
                    SetCreateStat(Stats(i), float(pInfo->stats[i]));

                health = pInfo->health;
                mana = 0;
                armor = pInfo->armor;

                // First we divide attack time by standard attack time, and then multipy by level and damage mod.
                uint32 mDmg = (GetAttackTime(BASE_ATTACK) * petlevel) / 2000;

                // Set damage
                SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(mDmg - mDmg / 4.f));
                SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float((mDmg - mDmg / 4.f) * 1.5));
            }
            else
            {
                sLog.outErrorDb("HUNTER PET levelstats missing in DB! 'Weakifying' pet. Entry: 1 PetLevel: %u.", petlevel);

                for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
                    SetCreateStat(Stats(i), 1.0f);

                health = 1;
                mana = 0;
                armor = 0;

                // Set damage
                SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, 1);
                SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, 1);
            }

            break;
        }

        case SUMMON_PET:
        {
            SetUInt32Value(UNIT_FIELD_PETEXPERIENCE, 0);
            SetUInt32Value(UNIT_FIELD_PETNEXTLEVELEXP, 1000);

            // Info found in pet_levelstats
            if (PetLevelInfo const* pInfo = sObjectMgr.GetPetLevelInfo(cInfo->Entry, petlevel))
            {
                for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
                    SetCreateStat(Stats(i), float(pInfo->stats[i]));

                health = pInfo->health;
                mana = pInfo->mana;
                armor = pInfo->armor;

                // Info found in ClassLevelStats
                if (CreatureClassLvlStats const* cCLS = sObjectMgr.GetCreatureClassLvlStats(petlevel, cInfo->UnitClass, cInfo->Expansion))
                {
                    float minDmg = (cCLS->BaseDamage * cInfo->DamageVariance + (cCLS->BaseMeleeAttackPower / 14) * (cInfo->MeleeBaseAttackTime / 1000.f)) * cInfo->DamageMultiplier;

                    // Apply custom damage setting (from config)
                    minDmg *= _GetDamageMod(cInfo->Rank);

                    SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(minDmg));
                    SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(minDmg * 1.5));
                }
                else
                {
                    sLog.outErrorDb("SUMMON_PET creature_template not finished (expansion field = -1) on creature %s! (entry: %u)", GetGuidStr().c_str(), cInfo->Entry);

                    float dMinLevel = cInfo->MinMeleeDmg / cInfo->MinLevel;
                    float dMaxLevel = cInfo->MaxMeleeDmg / cInfo->MaxLevel;
                    float mDmg = (dMaxLevel - ((dMaxLevel - dMinLevel) / 2)) * petlevel;

                    // Set damage
                    SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(mDmg - mDmg / 4));
                    SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float((mDmg - mDmg / 4) * 1.5));
                }
            }
            else
            {
                sLog.outErrorDb("SUMMON_PET levelstats missing in DB! 'Weakifying' pet and giving it mana to make it obvious. Entry: %u Level: %u", cInfo->Entry, petlevel);

                for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
                    SetCreateStat(Stats(i), 1.0f);

                health = 1;
                mana = 1;
                armor = 1;

                // Set damage
                SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, 1);
                SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, 1);
            }

            break;
        }
        case GUARDIAN_PET:
        {
            m_scaleWithCls = true;
            SelectLevel(petlevel);  // guardians reuse CLS function SelectLevel, so we stop here
            InitPetScalingAuras();
            return;
        }
        default:
            sLog.outError("Pet have incorrect type (%u) for level handling.", getPetType());
    }

    // Hunter and Warlock pets should NOT use creature's original modifiers/multipliers
    if (getPetType() != HUNTER_PET && getPetType() != SUMMON_PET)
    {
        health *= cInfo->HealthMultiplier;

        if (mana > 0)
            mana *= cInfo->PowerMultiplier;

        armor *= cInfo->ArmorMultiplier;
    }

    // Apply custom health setting (from config)
    health *= _GetHealthMod(cInfo->Rank);

    // A pet cannot not have health
    if (health < 1)
        health = 1;

    // Set base Health and Mana
    SetCreateHealth(health);
    SetCreateMana(mana);
    // Set base Armor
    SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, armor);

    InitPetScalingAuras();

    // Need to update stats - calculates max health/mana etc
    UpdateAllStats();

    // Need to set Health to full
    SetHealth(GetMaxHealth());

    // Need to set Mana to full
    if (GetPowerType() == POWER_MANA)
        SetPower(POWER_MANA, GetMaxPower(POWER_MANA));

    // Remove rage bar from pets (By setting rage = 0, and ensuring it stays that way by setting max rage = 0 as well)
    SetMaxPower(POWER_RAGE, 0);
    SetPower(POWER_RAGE, 0);
}

// 为宠物施放或刷新随主人属性缩放的隐藏光环（TBC 宠物属性继承机制）
void Pet::InitPetScalingAuras()
{
    if (m_scalingAuras.size() > 0) // levelup rebuild case
    {
        UpdateScalingAuras();
        return;
    }

    std::vector<uint32> scalingAuras;
    if (getPetType() == HUNTER_PET)
        scalingAuras.insert(scalingAuras.end(), { 34902, 34903, 34904 });
    else
    {
        switch (GetUInt32Value(UNIT_CREATED_BY_SPELL))
        {
            case 688: // Imp - Warlock
            case 691: // Felhunter
            case 697: // Voidwalker
            case 712: // Succubus
            case 30146: // Felguard
                scalingAuras.insert(scalingAuras.end(), { 34947, 34956, 34957, 34958 });
                break;
            case 1122: // Infernal - Warlock - unique - isnt updated on stat changes
            case 18541: // Doomguard - Ritual of Doom
            case 18662: // Doomguard - Curse of Doom
                scalingAuras.insert(scalingAuras.end(), { 36186, 36188, 36189, 36190 });
                break;
            case 31687: // Water Elemental - Mage
                scalingAuras.insert(scalingAuras.end(), { 35657, 35658, 35659, 35660 });
                break;
            case 32982: // Fire Elemental - Shaman
                scalingAuras.insert(scalingAuras.end(), { 35665, 35666, 35667, 35668 });
                break;
            case 33663: // Earth Elemental - Shaman
                scalingAuras.insert(scalingAuras.end(), { 35674, 35675, 35676 });
                break;
            case 33831: // Force Of Nature - Treants - Druid
                scalingAuras.insert(scalingAuras.end(), { 35669, 35670, 35671, 35672 });
                break;
            case 34433: // Shadowfiend - Priest
                scalingAuras.insert(scalingAuras.end(), { 35661, 35662, 35663, 35664 });
                break;
            default:
                break;
        }
    }

    for (uint32 spellId : scalingAuras)
        CastSpell(nullptr, spellId, TRIGGERED_NONE);
}

// 播放术士恶魔宠物被解散时的语音音效
void Pet::PlayDismissSound()
{
    uint32 soundId = 0;
    switch (GetUInt32Value(UNIT_CREATED_BY_SPELL))
    {
        case 688: // Imp - Warlock
            soundId = urand(0, 1) ? 11 : 371;
            break;
        //case 691: // Felhunter
        //    soundId = 11;
        //    break;
        case 697: // Voidwalker
            soundId = 162;
            break;
        case 712: // Succubus
            soundId = 37;
            break;
        case 22865: // Doomguard
            soundId = 68;
            break;
        case 30146: // Felguard
            soundId = 551;
            break;
    }
    SendPetDismiss(soundId);
}

// 检查指定物品是否在当前宠物家族的食物食谱中
bool Pet::HaveInDiet(ItemPrototype const* item) const
{
    if (!item->FoodType)
        return false;

    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
        return false;

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
    if (!cFamily)
        return false;

    const uint32 diet = cFamily->petFoodMask;
    const uint32 FoodMask = convertEnumToFlag(item->FoodType);
    return (diet & FoodMask) != 0;
}

// 根据食物物品等级与宠物等级差计算单跳回复的快乐度数值
uint32 Pet::GetCurrentFoodBenefitLevel(uint32 itemlevel) const
{
    // -5 or greater food level
    if (GetLevel() <= itemlevel + 5)                        // possible to feed level 60 pet with level 55 level food for full effect
        return 35000;
    // -10..-6
    if (GetLevel() <= itemlevel + 10)                  // pure guess, but sounds good
        return 17000;
    // -14..-11
    if (GetLevel() <= itemlevel + 14)                  // level 55 food gets green on 70, makes sense to me
        return 8000;
        // -15 or less
    return 0;
    // food too low level
}

// 从数据库加载宠物未过期的法术冷却并同步通知主人客户端
void Pet::_LoadSpellCooldowns()
{
    auto queryResult = CharacterDatabase.PQuery("SELECT spell,time FROM pet_spell_cooldown WHERE guid = '%u'", m_charmInfo->GetPetNumber());
    ByteBuffer cdData;
    uint32 cdCount = 0;

    if (queryResult)
    {
        auto curTime = GetMap()->GetCurrentClockTime();
        do
        {
            Field* fields = queryResult->Fetch();

            uint32 spell_id = fields[0].GetUInt32();
            uint64 spell_time = fields[1].GetUInt64();

            SpellEntry const* spellEntry = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
            if (!spellEntry)
            {
                sLog.outError("%s has unknown spell %u in `character_spell_cooldown`, skipping.", GetGuidStr().c_str(), spell_id);
                continue;
            }

            TimePoint spellExpireTime = std::chrono::time_point_cast<std::chrono::milliseconds>(Clock::from_time_t(spell_time));
            std::chrono::milliseconds spellRecTime = std::chrono::milliseconds::zero();
            if (spellExpireTime > curTime)
                spellRecTime = std::chrono::duration_cast<std::chrono::milliseconds>(spellExpireTime - curTime);

            // skip outdated cooldown
            if (spellRecTime == std::chrono::milliseconds::zero())
                continue;

            cdData << uint32(spell_id);
            cdData << uint32(uint32(spellRecTime.count()));
            ++cdCount;

            m_cooldownMap.AddCooldown(GetMap()->GetCurrentClockTime(), spell_id, uint32(spellRecTime.count()));
#ifdef _DEBUG
            uint32 spellCDDuration = std::chrono::duration_cast<std::chrono::seconds>(spellRecTime).count();
            sLog.outDebug("Adding spell cooldown to %s, SpellID(%u), recDuration(%us).", GetGuidStr().c_str(), spell_id, spellCDDuration);
#endif
        }
        while (queryResult->NextRow());

        if (cdCount && GetOwner() && GetOwner()->GetTypeId() == TYPEID_PLAYER)
        {
            WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 1 + cdData.size());
            data << GetObjectGuid();
            data << uint8(SPELL_COOLDOWN_FLAG_NONE);
            data.append(cdData);
            static_cast<Player*>(GetOwner())->GetSession()->SendPacket(data);
        }
    }
}

// 将宠物当前非永久法术冷却写入数据库 pet_spell_cooldown 表
void Pet::_SaveSpellCooldowns()
{
    static SqlStatementID delSpellCD;
    static SqlStatementID insSpellCD;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpellCD, "DELETE FROM pet_spell_cooldown WHERE guid = ?");
    stmt.PExecute(m_charmInfo->GetPetNumber());

    TimePoint currTime = GetMap()->GetCurrentClockTime();

    for (auto& cdItr : m_cooldownMap)
    {
        auto& cdData = cdItr.second;
        if (!cdData->IsPermanent())
        {
            TimePoint sTime = currTime;
            cdData->GetSpellCDExpireTime(sTime);
            uint64 spellExpireTime = uint64(Clock::to_time_t(sTime));

            stmt = CharacterDatabase.CreateStatement(insSpellCD, "INSERT INTO pet_spell_cooldown (guid,spell,time) VALUES (?, ?, ?)");
            stmt.PExecute(m_charmInfo->GetPetNumber(), cdItr.first, spellExpireTime);
        }
    }
}

// 从数据库加载宠物已学会的技能列表
void Pet::_LoadSpells()
{
    auto queryResult = CharacterDatabase.PQuery("SELECT spell,active FROM pet_spell WHERE guid = '%u'", m_charmInfo->GetPetNumber());

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();

            addSpell(fields[0].GetUInt32(), ActiveStates(fields[1].GetUInt8()), PETSPELL_UNCHANGED);
        }
        while (queryResult->NextRow());
    }
}

// 按增量状态（新增/修改/移除）同步宠物技能到数据库 pet_spell 表
void Pet::_SaveSpells()
{
    static SqlStatementID delSpell ;
    static SqlStatementID insSpell ;

    for (PetSpellMap::iterator itr = m_spells.begin(), next = m_spells.begin(); itr != m_spells.end(); itr = next)
    {
        ++next;

        // prevent saving family passives to DB
        if (itr->second.type == PETSPELL_FAMILY)
            continue;

        switch (itr->second.state)
        {
            case PETSPELL_REMOVED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(delSpell, "DELETE FROM pet_spell WHERE guid = ? and spell = ?");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first);
                m_spells.erase(itr);
            }
            continue;
            case PETSPELL_CHANGED:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(delSpell, "DELETE FROM pet_spell WHERE guid = ? and spell = ?");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first);

                stmt = CharacterDatabase.CreateStatement(insSpell, "INSERT INTO pet_spell (guid,spell,active) VALUES (?, ?, ?)");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first, uint32(itr->second.active));
            }
            break;
            case PETSPELL_NEW:
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insSpell, "INSERT INTO pet_spell (guid,spell,active) VALUES (?, ?, ?)");
                stmt.PExecute(m_charmInfo->GetPetNumber(), itr->first, uint32(itr->second.active));
            }
            break;
            case PETSPELL_UNCHANGED:
                continue;
        }

        itr->second.state = PETSPELL_UNCHANGED;
    }
}

// 从数据库加载宠物持久化光环并扣减离线经过时间
void Pet::_LoadAuras(uint32 timediff)
{
    auto queryResult = CharacterDatabase.PQuery("SELECT caster_guid,item_guid,spell,stackcount,remaincharges,basepoints0,basepoints1,basepoints2,periodictime0,periodictime1,periodictime2,maxduration,remaintime,effIndexMask FROM pet_aura WHERE guid = '%u'", m_charmInfo->GetPetNumber());

    if (queryResult)
    {
        do
        {
            Field* fields = queryResult->Fetch();
            ObjectGuid casterGuid = ObjectGuid(fields[0].GetUInt64());
            uint32 item_lowguid = fields[1].GetUInt32();
            uint32 spellid = fields[2].GetUInt32();
            uint32 stackcount = fields[3].GetUInt32();
            uint32 remaincharges = fields[4].GetUInt32();
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = fields[i + 5].GetInt32();
                periodicTime[i] = fields[i + 8].GetUInt32();
            }

            int32 maxduration = fields[11].GetInt32();
            int32 remaintime = fields[12].GetInt32();
            uint32 effIndexMask = fields[13].GetUInt32();

            SpellEntry const* spellproto = sSpellTemplate.LookupEntry<SpellEntry>(spellid);
            if (!spellproto)
            {
                sLog.outError("Unknown spell (spellid %u), ignore.", spellid);
                continue;
            }

            // do not load single target auras (unless they were cast by the player)
            if (casterGuid != GetObjectGuid() && sSpellMgr.IsSingleTargetSpell(spellproto))
                continue;

            if (remaintime != -1)
            {
                if (remaintime / IN_MILLISECONDS <= int32(timediff))
                    continue;

                remaintime -= timediff * IN_MILLISECONDS;
            }

            // prevent wrong values of remaincharges
            if (spellproto->procCharges == 0)
                remaincharges = 0;

            if (!spellproto->StackAmount)
                stackcount = 1;
            else if (spellproto->StackAmount < stackcount)
                stackcount = spellproto->StackAmount;
            else if (!stackcount)
                stackcount = 1;

            SpellAuraHolder* holder = CreateSpellAuraHolder(spellproto, this, nullptr);
            holder->SetLoadedState(casterGuid, ObjectGuid(HIGHGUID_ITEM, item_lowguid), stackcount, remaincharges, maxduration, remaintime);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((effIndexMask & (1 << i)) == 0)
                    continue;

                Aura* aura = CreateAura(spellproto, SpellEffectIndex(i), nullptr, nullptr, holder, this);
                if (!damage[i])
                    damage[i] = aura->GetModifier()->m_amount;

                aura->SetLoadedState(damage[i], periodicTime[i]);
                holder->AddAura(aura, SpellEffectIndex(i));
            }

            const bool empty = holder->IsEmptyHolder();
            if (!empty)
            {
                // reset stolen single target auras
                if (casterGuid != GetObjectGuid() && holder->GetTrackedAuraType() == TRACK_AURA_TYPE_SINGLE_TARGET)
                    holder->SetTrackedAuraType(TRACK_AURA_TYPE_NOT_TRACKED);

                holder->SetState(SPELLAURAHOLDER_STATE_DB_LOAD); // Safeguard mechanism against some actions
            }

            if (!empty && AddSpellAuraHolder(holder))
            {
                holder->SetState(SPELLAURAHOLDER_STATE_READY);
                DETAIL_LOG("Added pet auras from spellid %u", spellproto->Id);
            }
            else
                delete holder;
        }
        while (queryResult->NextRow());
    }
}

// 将宠物当前可保存的光环状态写入数据库 pet_aura 表
void Pet::_SaveAuras()
{
    static SqlStatementID delAuras ;
    static SqlStatementID insAuras ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delAuras, "DELETE FROM pet_aura WHERE guid = ?");
    stmt.PExecute(m_charmInfo->GetPetNumber());

    SpellAuraHolderMap const& auraHolders = GetSpellAuraHolderMap();

    if (auraHolders.empty())
        return;

    stmt = CharacterDatabase.CreateStatement(insAuras, "INSERT INTO pet_aura (guid, caster_guid, item_guid, spell, stackcount, remaincharges, "
            "basepoints0, basepoints1, basepoints2, periodictime0, periodictime1, periodictime2, maxduration, remaintime, effIndexMask) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    for (const auto& auraHolder : auraHolders)
    {
        SpellAuraHolder* holder = auraHolder.second;

        bool save = true;
        for (int32 j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            SpellEntry const* spellInfo = holder->GetSpellProto();
            if (spellInfo->EffectApplyAuraName[j] == SPELL_AURA_MOD_STEALTH ||
                    spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AREA_AURA_OWNER ||
                    spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AREA_AURA_PET)
            {
                save = false;
                break;
            }
        }

        // skip all holders from spells that are passive or channeled
        // do not save single target holders (unless they were cast by the player)
        if (save && holder->IsSaveToDbHolder())
        {
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];
            uint32 effIndexMask = 0;

            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = 0;
                periodicTime[i] = 0;

                if (Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                {
                    // don't save not own area auras
                    if (!aur->IsSaveToDbAura())
                        continue;

                    damage[i] = aur->GetModifier()->m_amount;
                    periodicTime[i] = aur->GetModifier()->periodictime;
                    effIndexMask |= (1 << i);
                }
            }

            if (!effIndexMask)
                continue;

            stmt.addUInt32(m_charmInfo->GetPetNumber());
            stmt.addUInt64(holder->GetCasterGuid().GetRawValue());
            stmt.addUInt32(holder->GetCastItemGuid().GetCounter());
            stmt.addUInt32(holder->GetId());
            stmt.addUInt32(holder->GetStackAmount());
            stmt.addUInt8(holder->GetAuraCharges());

            for (int i : damage)
                stmt.addInt32(i);

            for (unsigned int i : periodicTime)
                stmt.addUInt32(i);

            stmt.addInt32(holder->GetAuraMaxDuration());
            stmt.addInt32(holder->GetAuraDuration());
            stmt.addUInt32(effIndexMask);
            stmt.Execute();
        }
    }
}

// 为宠物添加技能，自动处理高低等级法术替换、被动触发及自动施法状态
bool Pet::addSpell(uint32 spell_id, ActiveStates active /*= ACT_DECIDE*/, PetSpellState state /*= PETSPELL_NEW*/, PetSpellType type /*= PETSPELL_NORMAL*/)
{
    SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spell_id);
    if (!spellInfo)
    {
        // do pet spell book cleanup
        if (state == PETSPELL_UNCHANGED)                    // spell load case
        {
            sLog.outError("Pet::addSpell: nonexistent in SpellStore spell #%u request, deleting for all pets in `pet_spell`.", spell_id);
            CharacterDatabase.PExecute("DELETE FROM pet_spell WHERE spell = '%u'", spell_id);
        }
        else
            sLog.outError("Pet::addSpell: nonexistent in SpellStore spell #%u request.", spell_id);

        return false;
    }

    PetSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr != m_spells.end())
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            m_spells.erase(itr);
            state = PETSPELL_CHANGED;
        }
        else if (state == PETSPELL_UNCHANGED && itr->second.state != PETSPELL_UNCHANGED)
        {
            // can be in case spell loading but learned at some previous spell loading
            itr->second.state = PETSPELL_UNCHANGED;

            if (active == ACT_ENABLED)
                ToggleAutocast(spell_id, true);
            else if (active == ACT_DISABLED)
                ToggleAutocast(spell_id, false);

            return false;
        }
        else
            return false;
    }

    PetSpell newspell;
    newspell.state = state;
    newspell.type = type;

    if (active == ACT_DECIDE)                               // active was not used before, so we save it's autocast/passive state here
    {
        if (IsAutocastable(spellInfo))
            newspell.active = ACT_ENABLED;
        else
            newspell.active = ACT_PASSIVE;
    }
    else
        newspell.active = active;

    if (sSpellMgr.GetSpellRank(spell_id) != 0)
    {
        for (PetSpellMap::const_iterator itr2 = m_spells.begin(); itr2 != m_spells.end(); ++itr2)
        {
            if (itr2->second.state == PETSPELL_REMOVED)
                continue;

            uint32 const oldspell_id = itr2->first;

            if (sSpellMgr.IsSpellAnotherRankOfSpell(spell_id, oldspell_id))
            {
                // replace by new high rank
                if (sSpellMgr.IsSpellHigherRankOfSpell(spell_id, oldspell_id))
                {
                    newspell.active = itr2->second.active;

                    if (newspell.active == ACT_ENABLED)
                        ToggleAutocast(oldspell_id, false);

                    unlearnSpell(oldspell_id, false, false);
                    break;
                }
                // ignore new lesser rank
                if (sSpellMgr.IsSpellHigherRankOfSpell(oldspell_id, spell_id))
                    return false;
            }
        }
    }

    m_spells[spell_id] = newspell;

    if (IsPassiveSpell(spellInfo))
        CastSpell(this, spell_id, TRIGGERED_OLD_TRIGGERED);
    else
        m_charmInfo->AddSpellToActionBar(spell_id, ActiveStates(newspell.active));

    if (newspell.active == ACT_ENABLED)
        ToggleAutocast(spell_id, true);

    return true;
}

// 让宠物学习新法术并在非加载阶段刷新主人客户端宠物技能栏
bool Pet::learnSpell(uint32 spell_id)
{
    // prevent duplicated entires in spell book
    if (!addSpell(spell_id))
        return false;

    if (!m_loading)
    {
        Unit* owner = GetOwner();
        if (owner && owner->GetTypeId() == TYPEID_PLAYER)
            ((Player*)owner)->PetSpellInitialize();
    }
    return true;
}

// 使宠物遗忘指定法术（removeSpell 的对外包装）
bool Pet::unlearnSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    if (removeSpell(spell_id, learn_prev, clear_ab))
        return true;
    return false;
}

// 从宠物法术书与动作栏移除指定法术，可选回退学习上一等级技能
bool Pet::removeSpell(uint32 spell_id, bool learn_prev, bool clear_ab)
{
    PetSpellMap::iterator itr = m_spells.find(spell_id);
    if (itr == m_spells.end())
        return false;

    if (itr->second.state == PETSPELL_REMOVED)
        return false;

    if (itr->second.state == PETSPELL_NEW)
        m_spells.erase(itr);
    else
        itr->second.state = PETSPELL_REMOVED;

    RemoveAurasDueToSpell(spell_id);

    if (learn_prev)
    {
        if (uint32 prev_id = sSpellMgr.GetPrevSpellInChain(spell_id))
            learnSpell(prev_id);
        else
            learn_prev = false;
    }

    // if remove last rank or non-ranked then update action bar at server and client if need
    if (clear_ab && !learn_prev && m_charmInfo->RemoveSpellFromActionBar(spell_id))
    {
        if (!m_loading)
        {
            // need update action bar for last removed rank
            if (Unit* owner = GetOwner())
                if (owner->GetTypeId() == TYPEID_PLAYER)
                    ((Player*)owner)->PetSpellInitialize();
        }
    }

    return true;
}

// 清理宠物动作栏中不再已学会的无效法术槽位
void Pet::CleanupActionBar()
{
    for (int i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
        if (UnitActionBarEntry const* ab = m_charmInfo->GetActionBarEntry(i))
            if (uint32 action = ab->GetAction())
                if (ab->IsActionBarForSpell() && !HasSpell(action))
                    m_charmInfo->SetActionBar(i, 0, ACT_DISABLED);
}

// 初始化新创建/新驯服宠物的默认初始技能、被动及可传授法术
void Pet::InitPetCreateSpells()
{
    m_charmInfo->InitPetActionBar();
    m_spells.clear();

    int32 usedtrainpoints = 0;

    uint32 petspellid;
    PetCreateSpellEntry const* CreateSpells = sObjectMgr.GetPetCreateSpellEntry(GetEntry());
    if (CreateSpells)
    {
        Unit* owner = GetOwner();
        Player* p_owner = owner && owner->GetTypeId() == TYPEID_PLAYER ? (Player*)owner : nullptr;

        for (uint8 i = 0; i < 4; ++i)
        {
            if (!CreateSpells->spellid[i])
                break;

            SpellEntry const* learn_spellproto = sSpellTemplate.LookupEntry<SpellEntry>(CreateSpells->spellid[i]);
            if (!learn_spellproto)
                continue;

            if (learn_spellproto->Effect[0] == SPELL_EFFECT_LEARN_SPELL || learn_spellproto->Effect[0] == SPELL_EFFECT_LEARN_PET_SPELL)
            {
                petspellid = learn_spellproto->EffectTriggerSpell[0];
                if (p_owner && !p_owner->HasSpell(learn_spellproto->Id))
                {
                    if (IsPassiveSpell(petspellid))         // learn passive skills when tamed, not sure if thats right
                        p_owner->learnSpell(learn_spellproto->Id, false);
                    else
                        AddTeachSpell(learn_spellproto->EffectTriggerSpell[0], learn_spellproto->Id);
                }
            }
            else
                petspellid = learn_spellproto->Id;

            addSpell(petspellid);

            SkillLineAbilityEntry const* skla = sSkillLineAbilityStore.LookupEntry(learn_spellproto->EffectTriggerSpell[0]);
            if (skla)
                usedtrainpoints += skla->reqtrainpoints;
        }
    }

    LearnPetPassives();

    CastPetAuras(false);

    SetTP(-usedtrainpoints);
}

// 猎人宠物施放可传授技能时，以 10% 概率让主人领悟对应的野兽训练法术
void Pet::CheckLearning(uint32 spellid)
{
    // charmed case -> prevent crash
    if (GetTypeId() == TYPEID_PLAYER || getPetType() != HUNTER_PET)
        return;

    Unit* owner = GetOwner();

    if (m_teachspells.empty() || !owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    TeachSpellMap::iterator itr = m_teachspells.find(spellid);
    if (itr == m_teachspells.end())
        return;

    if (urand(0, 100) < 10)
    {
        ((Player*)owner)->learnSpell(itr->second, false);
        m_teachspells.erase(itr);
    }
}

// 计算本次洗宠物天赋（技能）的金币消耗（随洗点次数递增并在 1 天后重置为 10 银）
uint32 Pet::resetTalentsCost() const
{
    uint32 days = uint32(sWorld.GetGameTime() - m_resetTalentsTime) / DAY;

    // The first time reset costs 10 silver; after 1 day cost is reset to 10 silver
    if (m_resetTalentsCost < 10 * SILVER || days > 0)
        return 10 * SILVER;
    // then 50 silver
    if (m_resetTalentsCost < 50 * SILVER)
        return 50 * SILVER;
        // then 1 gold
    if (m_resetTalentsCost < 1 * GOLD)
        return 1 * GOLD;
    // then increasing at a rate of 1 gold; cap 10 gold
    return (m_resetTalentsCost + 1 * GOLD > 10 * GOLD ? 10 * GOLD : m_resetTalentsCost + 1 * GOLD);
}

// 初始化控制信息（保留原 CharmInfo 以便解除附身控制后恢复）
CharmInfo* Pet::InitCharmInfo(Unit* charm)
{
    if (!m_originalCharminfo)
    {
        m_originalCharminfo = m_charmInfo;
        m_charmInfo = new CharmInfo(charm);
    }
    return m_charmInfo;
}

// 释放临时控制信息并还原原始 CharmInfo
void Pet::DeleteCharmInfo()
{
    if (m_originalCharminfo)
    {
        delete m_charmInfo;
        m_charmInfo = m_originalCharminfo;
        m_originalCharminfo = nullptr;
    }
}

// 切换宠物技能的自动施法开关状态并更新 m_autospells 列表
void Pet::ToggleAutocast(uint32 spellid, bool apply)
{
    if (!IsAutocastable(spellid))
        return;

    PetSpellMap::iterator itr = m_spells.find(spellid);
    PetSpell& petSpell = itr->second;

    uint32 i;

    if (apply)
    {
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid; ++i)
            ;                                               // just search

        if (i == m_autospells.size())
        {
            m_autospells.push_back(spellid);

            if (petSpell.active != ACT_ENABLED)
            {
                petSpell.active = ACT_ENABLED;
                if (petSpell.state != PETSPELL_NEW)
                    petSpell.state = PETSPELL_CHANGED;
            }
        }
    }
    else
    {
        AutoSpellList::iterator itr2 = m_autospells.begin();
        for (i = 0; i < m_autospells.size() && m_autospells[i] != spellid; ++i, ++itr2)
            ;                                               // just search

        if (i < m_autospells.size())
        {
            m_autospells.erase(itr2);
            if (petSpell.active != ACT_DISABLED)
            {
                petSpell.active = ACT_DISABLED;
                if (petSpell.state != PETSPELL_NEW)
                    petSpell.state = PETSPELL_CHANGED;
            }
        }
    }

}

// 根据模板与生成位置初始化宠物实体对象与坐标
bool Pet::Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, uint32 pet_number)
{
    SetMap(cPos.GetMap());

    if (!guidlow)                                           // no free map guid counter, already logged
        return false;

    Object::_Create(guidlow, guidlow, pet_number, HIGHGUID_PET);

    m_originalEntry = cinfo->Entry;

    if (!InitEntry(cinfo->Entry))
        return false;

    cPos.SelectFinalPoint(this, false);

    if (!cPos.Relocate(this))
        return false;

    SetSheath(SHEATH_STATE_MELEE);

    if (getPetType() == SUMMON_PET)
        SetCorpseDelay(5);

    return true;
}

// 检查宠物是否拥有指定法术（未处于待移除状态）
bool Pet::HasSpell(uint32 spell) const
{
    PetSpellMap::const_iterator itr = m_spells.find(spell);
    return (itr != m_spells.end() && itr->second.state != PETSPELL_REMOVED);
}

// Get all passive spells in our skill line (学习宠物所属野兽家族的全部被动技能)
void Pet::LearnPetPassives()
{
    CreatureInfo const* cInfo = GetCreatureInfo();
    if (!cInfo)
        return;

    CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
    if (!cFamily)
        return;

    PetFamilySpellsStore::const_iterator petStore = sPetFamilySpellsStore.find(cFamily->ID);
    if (petStore != sPetFamilySpellsStore.end())
    {
        for (uint32 petSet : petStore->second)
            addSpell(petSet, ACT_DECIDE, PETSPELL_NEW, PETSPELL_FAMILY);
    }
}

// 将主人身上的宠物加成光环施放到当前宠物身上
void Pet::CastPetAuras(bool current)
{
    if (!isControlled())
        return;

    Unit* owner = GetOwner();

    for (PetAuraSet::const_iterator itr = owner->m_petAuras.begin(); itr != owner->m_petAuras.end();)
    {
        PetAura const* pa = *itr;
        ++itr;

        if (!current && pa->IsRemovedOnChangePet())
            owner->RemovePetAura(pa);
        else
            CastPetAura(pa);
    }
}

// 刷新主人天赋赋予宠物的光环效果（如猎人“凶猛灵感”）
void Pet::CastOwnerTalentAuras()
{
    if (!GetOwner() || GetOwner()->GetTypeId() != TYPEID_PLAYER)
        return;

    Player* pOwner = static_cast<Player*>(GetOwner());

    // Handle Ferocious Inspiration Talent
    if (pOwner && pOwner->getClass() == CLASS_HUNTER)
    {
        // clear any existing Ferocious Inspiration auras
        RemoveAurasDueToSpell(34455);
        RemoveAurasDueToSpell(34459);
        RemoveAurasDueToSpell(34460);

        if (IsAlive())
        {
            if (pOwner->HasSpell(34455)) // Ferocious Inspiration Rank 1
                CastSpell(nullptr, 34457, TRIGGERED_OLD_TRIGGERED); // Ferocious Inspiration 1%

            if (pOwner->HasSpell(34459)) // Ferocious Inspiration Rank 2
                CastSpell(nullptr, 34457, TRIGGERED_OLD_TRIGGERED); // Ferocious Inspiration 2%

            if (pOwner->HasSpell(34460)) // Ferocious Inspiration Rank 3
                CastSpell(nullptr, 34457, TRIGGERED_OLD_TRIGGERED); // Ferocious Inspiration 3%
        }
    } // End Ferocious Inspiration Talent
}

// 为宠物施放单个匹配当前生物 Entry 的 PetAura 效果
void Pet::CastPetAura(PetAura const* aura)
{
    uint32 auraId = aura->GetAura(GetEntry());
    if (!auraId)
        return;

    CastSpell(nullptr, auraId, TRIGGERED_OLD_TRIGGERED);
}

// 将宠物等级与主人等级同步（召唤物始终同步，猎人宠物不超过主人等级）
void Pet::SynchronizeLevelWithOwner()
{
    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    switch (getPetType())
    {
        // always same level
        case SUMMON_PET:
            GivePetLevel(owner->GetLevel());
            break;
        // can't be greater owner level
        case HUNTER_PET:
            if (GetLevel() > owner->GetLevel())
                GivePetLevel(owner->GetLevel());
            break;
        default:
            break;
    }
}

// 更新宠物模式标志并向主人客户端发送 SMSG_PET_MODE 数据包
void Pet::SetModeFlags(PetModeFlags mode)
{
    m_petModeFlags = mode;

    Unit* owner = GetOwner();
    if (!owner || owner->GetTypeId() != TYPEID_PLAYER)
        return;

    WorldPacket data(SMSG_PET_MODE, 12);
    data << GetObjectGuid();
    data << uint32(m_petModeFlags);
    ((Player*)owner)->GetSession()->SendPacket(data);
}

// 按宠物类型计算并回复宠物生命值（每 4 秒周期调用）
void Pet::RegenerateHealth()
{
    if (!IsRegeneratingHealth())
        return;

    uint32 curValue = GetHealth();
    uint32 maxValue = GetMaxHealth();

    if (curValue >= maxValue)
        return;

    float HealthIncreaseRate = sWorld.getConfig(CONFIG_FLOAT_RATE_HEALTH);
    uint32 addvalue = 0;

    switch (m_petType)
    {
        case SUMMON_PET:
        case HUNTER_PET:
        {
            addvalue = OCTRegenHPPerSpirit() * HealthIncreaseRate * 4; // pets regen per 4 seconds
            break;
        }

        case GUARDIAN_PET:
        {
            // HACK: use this formula until we know the exact way to regen these type
            addvalue = maxValue / 20;
            break;
        }

        case MINI_PET:
        {
            // HACK: use this formula until we know the exact way to regen these type
            addvalue = maxValue / 3;
            break;
        }
        default:
            return;
    }

    ModifyHealth(addvalue);
}

// 重置宠物尸体到期时间，使其在下一秒触发清理
void Pet::ResetCorpseRespawn()
{
    m_corpseExpirationTime = GetMap()->GetCurrentClockTime() + std::chrono::milliseconds(1000); // rudimentary value - just need more than now
}

// 强制使宠物立即或延迟消失并执行解散清理
void Pet::ForcedDespawn(uint32 timeMSToDespawn, bool onlyAlive)
{
    if (timeMSToDespawn)
    {
        Creature::ForcedDespawn(timeMSToDespawn, onlyAlive);
        return;
    }

    if (IsDespawned())
        return;

    Unit* owner = GetOwner();

    if (IsAlive())
        SetDeathState(JUST_DIED);

    if (GetDeathState() == CORPSE) // rest of despawn cleanup meant to be done by Unsummon
    {
        if (AI())
        {
            uint32 respawnDelay = 0;
            AI()->CorpseRemoved(respawnDelay);
        }

        if (InstanceData* mapInstance = GetInstanceData())
            mapInstance->OnCreatureDespawn(this);
    }

    Unsummon(PET_SAVE_NOT_IN_SLOT, owner);
}

// 判断守护者宠物是否带有“忽略主人死亡”的法术属性（主人死亡时不随之消失）
bool Pet::IgnoresOwnersDeath() const
{
    if (IsGuardian())
    {
        if (uint32 spellId = GetUInt32Value(UNIT_CREATED_BY_SPELL))
        {
            SpellEntry const* spellInfo = sSpellTemplate.LookupEntry<SpellEntry>(spellId);
            // Remove infinity cooldown
            if (spellInfo && spellInfo->HasAttribute(SPELL_ATTR_EX_IGNORE_OWNERS_DEATH))
                return true;
        }
    }
    return false;
}
