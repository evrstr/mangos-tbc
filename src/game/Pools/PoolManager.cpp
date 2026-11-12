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

#include "Pools/PoolManager.h"
#include "Globals/ObjectMgr.h"
#include "Entities/ObjectGuid.h"
#include "Util/ProgressBar.h"
#include "Log/Log.h"
#include "Maps/MapPersistentStateMgr.h"
#include "World/World.h"
#include "Policies/Singleton.h"
#include <algorithm>

INSTANTIATE_SINGLETON_1(PoolManager);

////////////////////////////////////////////////////////////
// template class SpawnedPoolData (模板类 SpawnedPoolData：追踪池内已生成对象数量与状态)

// Method that tell amount spawned objects/subpools (获取指定池当前已生成的对象或子池数量)
uint32 SpawnedPoolData::GetSpawnedObjects(uint32 pool_id) const
{
    SpawnedPoolPools::const_iterator itr = mSpawnedPools.find(pool_id);
    return itr != mSpawnedPools.end() ? itr->second : 0;
}

// Method that tell if a creature is spawned currently (判断生物是否已在池中生成)
template<>
bool SpawnedPoolData::IsSpawnedObject<Creature>(uint32 db_guid) const
{
    return mSpawnedCreatures.find(db_guid) != mSpawnedCreatures.end();
}

// Method that tell if a gameobject is spawned currently (判断游戏对象是否已在池中生成)
template<>
bool SpawnedPoolData::IsSpawnedObject<GameObject>(uint32 db_guid) const
{
    return mSpawnedGameobjects.find(db_guid) != mSpawnedGameobjects.end();
}

// Method that tell if a pool is spawned currently (判断子池是否已在池中激活)
template<>
bool SpawnedPoolData::IsSpawnedObject<Pool>(uint32 sub_pool_id) const
{
    return mSpawnedPools.find(sub_pool_id) != mSpawnedPools.end();
}

// 记录已生成生物并递增所属池计数
template<>
void SpawnedPoolData::AddSpawn<Creature>(uint32 db_guid, uint32 pool_id)
{
    mSpawnedCreatures.insert(db_guid);
    ++mSpawnedPools[pool_id];
}

// 记录已生成游戏对象并递增所属池计数
template<>
void SpawnedPoolData::AddSpawn<GameObject>(uint32 db_guid, uint32 pool_id)
{
    mSpawnedGameobjects.insert(db_guid);
    ++mSpawnedPools[pool_id];
}

// 初始化子池生成计数并递增母池计数
template<>
void SpawnedPoolData::AddSpawn<Pool>(uint32 sub_pool_id, uint32 pool_id)
{
    mSpawnedPools[sub_pool_id] = 0;
    ++mSpawnedPools[pool_id];
}

// 移除生物生成记录并递减所属池计数
template<>
void SpawnedPoolData::RemoveSpawn<Creature>(uint32 db_guid, uint32 pool_id)
{
    mSpawnedCreatures.erase(db_guid);
    uint32& val = mSpawnedPools[pool_id];
    if (val > 0)
        --val;
}

// 移除游戏对象生成记录并递减所属池计数
template<>
void SpawnedPoolData::RemoveSpawn<GameObject>(uint32 db_guid, uint32 pool_id)
{
    mSpawnedGameobjects.erase(db_guid);
    uint32& val = mSpawnedPools[pool_id];
    if (val > 0)
        --val;
}

// 移除子池生成记录并递减母池计数
template<>
void SpawnedPoolData::RemoveSpawn<Pool>(uint32 sub_pool_id, uint32 pool_id)
{
    mSpawnedPools.erase(sub_pool_id);
    uint32& val = mSpawnedPools[pool_id];
    if (val > 0)
        --val;
}

////////////////////////////////////////////////////////////
// Methods of class PoolObject (PoolObject 方法：校验成员与世界事件的关联完整性)
template<>
void PoolObject::CheckEventLinkAndReport<Creature>(uint32 poolId, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& /*go2event*/) const
{
    std::map<uint32, int16>::const_iterator itr = creature2event.find(guid);
    if (itr == creature2event.end() || itr->second != event_id)
        sLog.outErrorDb("Creature (GUID: %u) expected to be listed in `game_event_creature` for event %u as part pool %u", guid, event_id, poolId);
}

template<>
void PoolObject::CheckEventLinkAndReport<GameObject>(uint32 poolId, int16 event_id, std::map<uint32, int16> const& /*creature2event*/, std::map<uint32, int16> const& go2event) const
{
    std::map<uint32, int16>::const_iterator itr = go2event.find(guid);
    if (itr == go2event.end() || itr->second != event_id)
        sLog.outErrorDb("Gameobject (GUID: %u) expected to be listed in `game_event_gameobject` for event %u as part pool %u", guid, event_id, poolId);
}

template<>
void PoolObject::CheckEventLinkAndReport<Pool>(uint32 /*poolId*/, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const
{
    sPoolMgr.CheckEventLinkAndReport(guid, event_id, creature2event, go2event);
}

////////////////////////////////////////////////////////////
// Methods of template class PoolGroup (模板类 PoolGroup 方法)

// Method to add a gameobject/creature guid to the proper list depending on pool type and chance value
// 根据池上限及概率配置，将成员加入显式概率列表或等概率列表
template <class T>
void PoolGroup<T>::AddEntry(PoolObject& poolitem, uint32 maxentries)
{
    if (poolitem.chance != 0 && maxentries == 1)
        ExplicitlyChanced.push_back(poolitem);
    else
        EqualChanced.push_back(poolitem);
}

// Method to check the chances are proper in this object pool
// 校验显式概率总和是否为 100% 或 0（当无等概率成员时）
template <class T>
bool PoolGroup<T>::CheckPool() const
{
    if (EqualChanced.empty())
    {
        float chance = 0;
        for (uint32 i = 0; i < ExplicitlyChanced.size(); ++i)
            chance += ExplicitlyChanced[i].chance;
        if (chance != 100 && chance != 0)
            return false;
    }
    return true;
}

// Method to check event linking (检查池内所有成员是否正确关联到指定世界事件)
template <class T>
void PoolGroup<T>::CheckEventLinkAndReport(int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const
{
    for (uint32 i = 0; i < EqualChanced.size(); ++i)
        EqualChanced[i].template CheckEventLinkAndReport<T>(poolId, event_id, creature2event, go2event);

    for (uint32 i = 0; i < ExplicitlyChanced.size(); ++i)
        ExplicitlyChanced[i].template CheckEventLinkAndReport<T>(poolId, event_id, creature2event, go2event);
}

// 设置或取消指定 GUID 成员的排除标记
template <class T>
void PoolGroup<T>::SetExcludeObject(uint32 guid, bool state)
{
    for (uint32 i = 0; i < EqualChanced.size(); ++i)
    {
        if (EqualChanced[i].guid == guid)
        {
            EqualChanced[i].exclude = state;
            return;
        }
    }

    for (uint32 i = 0; i < ExplicitlyChanced.size(); ++i)
    {
        if (ExplicitlyChanced[i].guid == guid)
        {
            ExplicitlyChanced[i].exclude = state;
            return;
        }
    }
}

// 从池中随机抽取一个可生成的成员（优先按显式概率，其次等概率）
template <class T>
PoolObject* PoolGroup<T>::RollOne(SpawnedPoolData& spawns, uint32 triggerFrom, MapPersistentState& mapState)
{
    PoolObject* explicitlyObjFound = nullptr;
    auto gen = GetRandomGenerator();

    if (!ExplicitlyChanced.empty())
    {
        float roll = (float)rand_chance();

        std::vector <PoolObject*> explicitlyChancedVector;

        // call memory manager once to reserve enough memory for performance (预分配向量空间减少内存重分配)
        explicitlyChancedVector.reserve(ExplicitlyChanced.size());

        // fill new vector with address of object in EqualChanced list (收集显式概率成员指针并随机打乱顺序)
        std::transform(ExplicitlyChanced.begin(), ExplicitlyChanced.end(), std::back_inserter(explicitlyChancedVector), [](PoolObject& objPtr) { return &objPtr; });

        // randomize the new vector (随机洗牌避免顺序偏差)
        std::shuffle(explicitlyChancedVector.begin(), explicitlyChancedVector.end(), *gen);

        for (auto obj : explicitlyChancedVector)
        {
            if (obj->exclude)
                continue;

            if (!CanSpawn(obj, mapState))
                continue;

            if (obj->guid != triggerFrom && spawns.IsSpawnedObject<T>(obj->guid))
                continue;

            // we found an object but it may have no luck to be picked so we save it in case of no other object have been picked.
            // 记录首个合法候选对象作为保底，防止本轮概率均未命中
            if (!explicitlyObjFound)
                explicitlyObjFound = obj;

            if (roll < obj->chance)
                return obj;
        }
    }

    if (!EqualChanced.empty())
    {
        std::vector <PoolObject*> equalyChancedVector;

        // call memory manager once to reserve enough memory for performance (预分配向量空间)
        equalyChancedVector.reserve(EqualChanced.size());

        // fill new vector with address of object in EqualChanced list (收集等概率成员指针并洗牌)
        std::transform(EqualChanced.begin(), EqualChanced.end(), std::back_inserter(equalyChancedVector), [](PoolObject& objPtr) { return &objPtr; });

        // randomize the new vector
        std::shuffle(equalyChancedVector.begin(), equalyChancedVector.end(), *gen);

        for (auto obj : equalyChancedVector)
        {
            if (obj->exclude)
                continue;

            if (!CanSpawn(obj, mapState))
                continue;

            if (obj->guid != triggerFrom && spawns.IsSpawnedObject<T>(obj->guid))
                continue;

            return obj;
        }
    }

    // return first explicitly chanced object if there is one (未命中等概率且显式列表无直接命中时，返回保底候选)
    return explicitlyObjFound;
}

// Main method to despawn a creature or gameobject in a pool
// If no guid is passed, the pool is just removed (event end case)
// If guid is filled, cache will be used and no removal will occur, it just fill the cache
// 移除池中已生成的生物/游戏对象/子池；传入 guid=0 时清空整个池（如事件结束）
template<class T>
void PoolGroup<T>::DespawnObject(MapPersistentState& mapState, uint32 guid)
{
    for (size_t i = 0; i < EqualChanced.size(); ++i)
    {
        // if spawned (仅处理当前已生成的对象)
        if (mapState.GetSpawnedPoolData().IsSpawnedObject<T>(EqualChanced[i].guid))
        {
            // any or specially requested (全部移除或匹配指定 GUID)
            if (!guid || EqualChanced[i].guid == guid)
            {
                Despawn1Object(mapState, EqualChanced[i].guid);
                mapState.GetSpawnedPoolData().RemoveSpawn<T>(EqualChanced[i].guid, poolId);
            }
        }
    }

    for (size_t i = 0; i < ExplicitlyChanced.size(); ++i)
    {
        // spawned (仅处理当前已生成的对象)
        if (mapState.GetSpawnedPoolData().IsSpawnedObject<T>(ExplicitlyChanced[i].guid))
        {
            // any or specially requested
            if (!guid || ExplicitlyChanced[i].guid == guid)
            {
                Despawn1Object(mapState, ExplicitlyChanced[i].guid);
                mapState.GetSpawnedPoolData().RemoveSpawn<T>(ExplicitlyChanced[i].guid, poolId);
            }
        }
    }
}

// Method that is actualy doing the removal job on one creature (实际从网格和地图移除单个生物)
template<>
void PoolGroup<Creature>::Despawn1Object(MapPersistentState& mapState, uint32 guid)
{
    if (CreatureData const* data = sObjectMgr.GetCreatureData(guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState (非副本地图的池成员可能位于与当前 mapState 不同的地图)
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
        {
            dataMapState->RemoveCreatureFromGrid(guid, data);

            if (Map* dataMap = dataMapState->GetMap())
                if (Creature* pCreature = dataMap->GetCreature(guid))
                    pCreature->AddObjectToRemoveList();
        }
    }
}

// Same on one gameobject (实际从网格和地图移除单个游戏对象)
template<>
void PoolGroup<GameObject>::Despawn1Object(MapPersistentState& mapState, uint32 guid)
{
    if (GameObjectData const* data = sObjectMgr.GetGOData(guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState (非副本地图的池成员可能位于其他地图)
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
        {
            dataMapState->RemoveGameobjectFromGrid(guid, data);

            if (Map* dataMap = dataMapState->GetMap())
                if (GameObject* pGameobject = dataMap->GetGameObject(guid))
                    pGameobject->AddObjectToRemoveList();
        }
    }
}

// Same on one pool (递归移除子池)
template<>
void PoolGroup<Pool>::Despawn1Object(MapPersistentState& mapState, uint32 child_pool_id)
{
    sPoolMgr.DespawnPool(mapState, child_pool_id);
}

// Method for a pool only to remove any found record causing a circular dependency loop (移除导致循环引用的子池关联)
template<>
void PoolGroup<Pool>::RemoveOneRelation(uint16 child_pool_id)
{
    for (PoolObjectList::iterator itr = ExplicitlyChanced.begin(); itr != ExplicitlyChanced.end(); ++itr)
    {
        if (itr->guid == child_pool_id)
        {
            ExplicitlyChanced.erase(itr);
            break;
        }
    }
    for (PoolObjectList::iterator itr = EqualChanced.begin(); itr != EqualChanced.end(); ++itr)
    {
        if (itr->guid == child_pool_id)
        {
            EqualChanced.erase(itr);
            break;
        }
    }
}

// 校验生物当前是否允许在地图生成
template<>
bool PoolGroup<Creature>::CanSpawn(PoolObject* object, MapPersistentState& mapState)
{
    if (Map* map = mapState.GetMap()) // for world maps this will fail on world start (世界启动阶段地图指针尚不存在，跳过检查)
        map->CanSpawn(TYPEID_UNIT, object->guid);
    return true;
}

// 校验游戏对象当前是否允许在地图生成
template<>
bool PoolGroup<GameObject>::CanSpawn(PoolObject* object, MapPersistentState& mapState)
{
    if (Map* map = mapState.GetMap()) // for world maps this will fail on world start (世界启动阶段地图指针尚不存在，跳过检查)
        return map->CanSpawn(TYPEID_GAMEOBJECT, object->guid);
    return true;
}

template<>
bool PoolGroup<Pool>::CanSpawn(PoolObject* /*object*/, MapPersistentState& /*mapState*/)
{
    return true;
}

// 按上限抽取并生成池内缺失的对象；若由 triggerFrom 触发则同步替换掉原对象
template <class T>
void PoolGroup<T>::SpawnObject(MapPersistentState& mapState, uint32 limit, uint32 triggerFrom, bool instantly)
{
    SpawnedPoolData& spawns = mapState.GetSpawnedPoolData();

    uint32 lastDespawned = 0;
    int count = limit - spawns.GetSpawnedObjects(poolId);

    // If triggered from some object respawn this object is still marked as spawned
    // and also counted into m_SpawnedPoolAmount so we need increase count to be
    // spawned by 1
    // 若由成员重生触发，该成员仍计入已生成数量，因此需将待生成配额加 1 以支持轮换替换
    if (triggerFrom)
    {
        if (spawns.IsSpawnedObject<T>(triggerFrom))
            ++count;
        else
            triggerFrom = 0;
    }

    // This will try to spawn the rest of pool, not guaranteed (尝试补足剩余配额，但不保证每次都能抽满)
    for (int i = 0; i < count; ++i)
    {
        PoolObject* obj = RollOne(spawns, triggerFrom, mapState);
        if (!obj)
            continue;
        if (obj->guid == lastDespawned)
            continue;

        if (obj->guid == triggerFrom)
        {
            MANGOS_ASSERT(spawns.IsSpawnedObject<T>(obj->guid));
            MANGOS_ASSERT(spawns.GetSpawnedObjects(poolId) > 0);
            ReSpawn1Object(mapState, obj);
            triggerFrom = 0;
            continue;
        }

        spawns.AddSpawn<T>(obj->guid, poolId);
        Spawn1Object(mapState, obj, instantly);

        if (triggerFrom)
        {
            // One spawn one despawn no count increase (生成新成员后立即移除旧成员，保持池内总数不变)
            DespawnObject(mapState, triggerFrom);
            lastDespawned = triggerFrom;
            triggerFrom = 0;
        }
    }
}

// Method that is actualy doing the spawn job on 1 creature (将单个生物加入网格并在地图已加载时实例化)
template <>
void PoolGroup<Creature>::Spawn1Object(MapPersistentState& mapState, PoolObject* obj, bool instantly)
{
    if (CreatureData const* data = sObjectMgr.GetCreatureData(obj->guid))
    {
        mapState.AddCreatureToGrid(obj->guid, data);
        Map* dataMap = mapState.GetMap();

        // We use spawn coords to spawn (仅在目标网格已加载时创建实体)
        if (dataMap && dataMap->IsLoaded(data->posX, data->posY))
        {
            Creature* pCreature = new Creature;
            if (!pCreature->LoadFromDB(obj->guid, dataMap, obj->guid, 0))
                delete pCreature;
            else
            {
                // if new spawn replaces a just despawned creature, not instantly spawn but set respawn timer
                // 若非立即生成（替换刚死亡/消失的生物），则设置延迟重生计时器
                if (!instantly)
                {
                    pCreature->SetRespawnDelay(data->GetRandomRespawnTime());
                    pCreature->SetRespawnTime(pCreature->GetRespawnDelay());
                    if (sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY) || pCreature->IsWorldBoss())
                        pCreature->SaveRespawnTime();
                }
            }
        }
        // for not loaded grid just update respawn time (avoid work for instances until implemented support)
        // 若网格尚未加载且非立即生成，仅更新持久化重生时间
        else if (!instantly)
            mapState.SaveCreatureRespawnTime(obj->guid, time(nullptr) + data->GetRandomRespawnTime());
    }
}

// Same for 1 gameobject (将单个游戏对象加入网格并在地图已加载时实例化)
template <>
void PoolGroup<GameObject>::Spawn1Object(MapPersistentState& mapState, PoolObject* obj, bool instantly)
{
    if (GameObjectData const* data = sObjectMgr.GetGOData(obj->guid))
    {
        mapState.AddGameobjectToGrid(obj->guid, data);

        Map* dataMap = mapState.GetMap();

        // We use spawn coords to spawn (仅在目标网格已加载时创建实体)
        if (dataMap && dataMap->IsLoaded(data->posX, data->posY))
        {
            GameObjectData const* data = sObjectMgr.GetGOData(obj->guid);
            MANGOS_ASSERT(data);
            GameObject* pGameobject = GameObject::CreateGameObject(data->id);
            if (!pGameobject->LoadFromDB(obj->guid, dataMap, obj->guid, 0))
                delete pGameobject;
            else
            {
                // if new spawn replaces a just despawned object, not instantly spawn but set respawn timer
                // 若非立即生成，则设置游戏对象重生计时器
                if (!instantly)
                {
                    pGameobject->SetRespawnTime(data->GetRandomRespawnTime());
                    if (sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
                        pGameobject->SaveRespawnTime();
                }
            }
        }
        // for not loaded grid just update respawn time (avoid work for instances until implemented support)
        // 若网格尚未加载且非立即生成，仅记录重生时间并通知动态 GUID 刷新管理器
        else if (!instantly)
        {
            // for spawned by default object only (仅处理默认刷新的对象)
            if (data->spawntimesecsmin >= 0)
            {
                uint32 respawnTime = data->GetRandomRespawnTime();
                mapState.SaveGORespawnTime(obj->guid, time(nullptr) + respawnTime);
                if (dataMap)
                {
                    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(data->id);
                    if (!goinfo || goinfo->ExtraFlags & GAMEOBJECT_EXTRA_FLAG_DYNGUID)
                        dataMap->GetSpawnManager().RespawnGameObject(obj->guid, respawnTime);
                }
            }
        }
    }
}

// Same for 1 pool (递归生成子池)
template <>
void PoolGroup<Pool>::Spawn1Object(MapPersistentState& mapState, PoolObject* obj, bool instantly)
{
    sPoolMgr.SpawnPool(mapState, obj->guid, instantly);
}

// Method that does the respawn job on the specified creature (将同一生物重新加入地图触发重生)
template <>
void PoolGroup<Creature>::ReSpawn1Object(MapPersistentState& mapState, PoolObject* obj)
{
    if (CreatureData const* data = sObjectMgr.GetCreatureData(obj->guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState (非副本地图按生物实际 mapid 查找持久化状态)
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
            if (Map* dataMap = dataMapState->GetMap())
                if (Creature* pCreature = dataMap->GetCreature(obj->guid))
                    pCreature->GetMap()->Add(pCreature);
    }
}

// Method that does the respawn job on the specified gameobject (将同一游戏对象重新加入地图触发重生)
template <>
void PoolGroup<GameObject>::ReSpawn1Object(MapPersistentState& mapState, PoolObject* obj)
{
    if (GameObjectData const* data = sObjectMgr.GetGOData(obj->guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState (非副本地图按游戏对象实际 mapid 查找持久化状态)
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
            if (Map* dataMap = dataMapState->GetMap())
                if (GameObject* pGameobject = dataMap->GetGameObject(obj->guid))
                    pGameobject->GetMap()->Add(pGameobject);
    }
}

// Nothing to do for a child Pool (子池无需单独执行 ReSpawn1Object)
template <>
void PoolGroup<Pool>::ReSpawn1Object(MapPersistentState& /*mapState*/, PoolObject* /*obj*/)
{
}


////////////////////////////////////////////////////////////
// Methods of class PoolManager (PoolManager 类方法)

PoolManager::PoolManager() : max_pool_id(0)
{
}

// Check listing all pool spawns in single instanceable map or only in non-instanceable maps
// This applied to all pools have common mother pool
// 校验池内所有成员是否均位于同一张地图（适用于共享母池的所有子池）
// 校验池内所有成员是否均位于同一张地图（适用于共享母池的所有子池）
struct PoolMapChecker
{
    PoolManager::PoolTemplateDataMap& m_poolTemplates;

    explicit PoolMapChecker(PoolManager::PoolTemplateDataMap& poolTemplates) : m_poolTemplates(poolTemplates) {}

    bool CheckAndRemember(uint32 mapid, uint32 pool_id, char const* tableName, char const* elementName)
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
        if (!mapEntry)
            return false;

        MapEntry const* poolMapEntry = m_poolTemplates[pool_id].mapEntry;

        // if not listed then just remember (尚未记录地图则直接绑定当前地图)
        if (!poolMapEntry)
        {
            m_poolTemplates[pool_id].mapEntry = mapEntry;
            return true;
        }

        // if at same map, then all ok (与已记录地图一致则返回成功)
        if (poolMapEntry == mapEntry)
            return true;

        // pool spawns must be at single map (同一个对象池的所有刷新点必须位于同一张地图，冲突则跳过)
        sLog.outErrorDb("`%s` has %s spawned at map %u when one or several other spawned at different instanceable map %u in pool id %i, skipped.",
            tableName, elementName, mapid, poolMapEntry->MapID, pool_id);
        return false;
    }
};

// 从数据库加载对象池模板、生物池、游戏对象池与子池映射关系，并检测环路与概率完整性
void PoolManager::LoadFromDB()
{
    auto queryResult = WorldDatabase.Query("SELECT MAX(entry) FROM pool_template");
    if (!queryResult)
    {
        sLog.outString(">> Table pool_template is empty.");
        sLog.outString();
        return;
    }
    Field* fields = queryResult->Fetch();
    max_pool_id = fields[0].GetUInt16();

    mPoolTemplate.resize(max_pool_id + 1);

    queryResult = WorldDatabase.Query("SELECT entry, max_limit, description FROM pool_template");
    if (!queryResult)
    {
        mPoolTemplate.clear();
        sLog.outString(">> Table pool_template is empty:");
        sLog.outString();
        return;
    }

    uint32 count = 0;

    BarGoLink bar(queryResult->GetRowCount());
    do
    {
        ++count;
        fields = queryResult->Fetch();

        bar.step();

        uint16 pool_id = fields[0].GetUInt16();

        PoolTemplateData& pPoolTemplate = mPoolTemplate[pool_id];
        pPoolTemplate.MaxLimit    = fields[1].GetUInt32();
        pPoolTemplate.description = fields[2].GetCppString();
        pPoolTemplate.AutoSpawn = true;          // will update and later data loading (默认标记为自动生成，后续加载子池或事件关联时会修正)
    }
    while (queryResult->NextRow());

    sLog.outString();
    sLog.outString(">> Loaded %u objects pools", count);

    PoolMapChecker mapChecker(mPoolTemplate);

    // Creatures (guids and entries) (加载生物池成员：先按 GUID 再按模板)

    mPoolCreatureGroups.resize(max_pool_id + 1);
    mCreatureSearchMap.clear();
    //                                   1     2           3
    queryResult = WorldDatabase.Query("SELECT guid, pool_entry, chance FROM pool_creature");

    count = 0;
    if (!queryResult)
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u creatures in pools from `pool_creature`", count);
    }
    else
    {
        BarGoLink bar2(queryResult->GetRowCount());
        do
        {
            fields = queryResult->Fetch();

            bar2.step();

            uint32 guid    = fields[0].GetUInt32();
            uint16 pool_id = fields[1].GetUInt16();
            float chance   = fields[2].GetFloat();

            CreatureData const* data = sObjectMgr.GetCreatureData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_creature` has a non existing creature spawn (GUID: %u) defined for pool id (%u), skipped.", guid, pool_id);
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_creature` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_creature` has an invalid chance (%f) for creature guid (%u) in pool id (%i), skipped.", chance, guid, pool_id);
                continue;
            }

            if (!mapChecker.CheckAndRemember(data->mapid, pool_id, "pool_creature", "creature guid"))
                continue;

            PoolTemplateData* pPoolTemplate = &mPoolTemplate[pool_id];

            ++count;

            PoolObject plObject(guid, chance);
            PoolGroup<Creature>& cregroup = mPoolCreatureGroups[pool_id];
            cregroup.SetPoolId(pool_id);
            cregroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mCreatureSearchMap.insert(p);
        }
        while (queryResult->NextRow());
        sLog.outString();
        sLog.outString(">> Loaded %u creatures in pools from `pool_creature`", count);
    }

    queryResult = WorldDatabase.Query("SELECT guid, pool_entry, chance, pool_creature_template.id FROM pool_creature_template LEFT JOIN creature ON creature.id = pool_creature_template.id");

    count = 0;
    if (!queryResult)
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u creatures in pools from `pool_creature_template`", count);
    }
    else
    {
        BarGoLink bar2(queryResult->GetRowCount());
        do
        {
            fields = queryResult->Fetch();

            bar2.step();

            uint32 guid    = fields[0].GetUInt32();
            uint16 pool_id = fields[1].GetUInt16();
            float chance   = fields[2].GetFloat();
            uint16 entry_id = fields[3].GetUInt32();        // for errors output only (仅用于错误日志输出)

            CreatureData const* data = sObjectMgr.GetCreatureData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_creature_template` has a non existing creature spawn (GUID: %u Entry: %u) defined for pool id (%u), skipped.", guid, entry_id, pool_id);
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_creature_template` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_creature_template` has an invalid chance (%f) for creature (Guid %u Entry %u) in pool id (%i), skipped.", chance, guid, entry_id, pool_id);
                continue;
            }

            // `pool_creature` and `pool_creature_template` can't have guids duplicates (in second case because entries also unique)
            // So if guid already listed in pools then this duplicate from alt.table
            // Also note: for added guid not important what case we skip from 2 tables
            // 检查该 GUID 是否已在 pool_creature 中注册，若重复则跳过
            if (uint16 alt_pool_id = IsPartOfAPool<Creature>(guid))
            {
                sLog.outErrorDb("`pool_creature` has guid %u for pool %u that already added to pool %u from `pool_creature_template` for creature entry %u, skipped.",
                                guid, pool_id, alt_pool_id, entry_id);
                continue;
            }

            if (!mapChecker.CheckAndRemember(data->mapid, pool_id, "pool_creature_template", "creature guid"))
                continue;

            PoolTemplateData* pPoolTemplate = &mPoolTemplate[pool_id];

            ++count;

            PoolObject plObject(guid, chance);
            PoolGroup<Creature>& cregroup = mPoolCreatureGroups[pool_id];
            cregroup.SetPoolId(pool_id);
            cregroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mCreatureSearchMap.insert(p);
        }
        while (queryResult->NextRow());
        sLog.outString();
        sLog.outString(">> Loaded %u creatures in pools from `pool_creature_template`", count);
    }

    // Gameobjects (guids and entries) (加载游戏对象池成员：先按 GUID 再按模板)

    mPoolGameobjectGroups.resize(max_pool_id + 1);
    mGameobjectSearchMap.clear();
    //                                        1     2           3
    queryResult = WorldDatabase.Query("SELECT guid, pool_entry, chance FROM pool_gameobject");

    count = 0;
    if (!queryResult)
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u gameobject in pools from `pool_gameobject`", count);
    }
    else
    {
        BarGoLink bar2(queryResult->GetRowCount());
        do
        {
            fields = queryResult->Fetch();

            bar2.step();

            uint32 guid    = fields[0].GetUInt32();
            uint16 pool_id = fields[1].GetUInt16();
            float chance   = fields[2].GetFloat();

            GameObjectData const* data = sObjectMgr.GetGOData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_gameobject` has a non existing gameobject spawn (GUID: %u) defined for pool id (%u), skipped.", guid, pool_id);
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_gameobject` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_gameobject` has an invalid chance (%f) for gameobject guid (%u) in pool id (%i), skipped.", chance, guid, pool_id);
                continue;
            }

            if (!mapChecker.CheckAndRemember(data->mapid, pool_id, "pool_gameobject", "gameobject guid"))
                continue;

            PoolTemplateData* pPoolTemplate = &mPoolTemplate[pool_id];

            ++count;

            PoolObject plObject(guid, chance);
            PoolGroup<GameObject>& gogroup = mPoolGameobjectGroups[pool_id];
            gogroup.SetPoolId(pool_id);
            gogroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mGameobjectSearchMap.insert(p);
        }
        while (queryResult->NextRow());
        sLog.outString();
        sLog.outString(">> Loaded %u gameobject in pools from `pool_gameobject`", count);
    }

    //                                        1     2           3
    queryResult = WorldDatabase.Query("SELECT guid, pool_entry, chance, pool_gameobject_template.id FROM pool_gameobject_template LEFT JOIN gameobject ON gameobject.id = pool_gameobject_template.id");

    count = 0;
    if (!queryResult)
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u gameobject in pools from `pool_gameobject_template`", count);
    }
    else
    {
        BarGoLink bar2(queryResult->GetRowCount());
        do
        {
            fields = queryResult->Fetch();

            bar2.step();

            uint32 guid    = fields[0].GetUInt32();
            uint16 pool_id = fields[1].GetUInt16();
            float chance   = fields[2].GetFloat();
            uint16 entry_id = fields[3].GetUInt32();        // for errors output only (仅用于错误日志输出)

            GameObjectData const* data = sObjectMgr.GetGOData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_gameobject_template` has a non existing gameobject spawn (GUID: %u Entry %u) defined for pool id (%u), skipped.", guid, entry_id, pool_id);
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_gameobject_template` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_gameobject_template` has an invalid chance (%f) for gameobject (Guid %u Entry %u) in pool id (%i), skipped.", chance, guid, entry_id, pool_id);
                continue;
            }

            // `pool_gameobject` and `pool_gameobject_template` can't have guids duplicates (in second case because entries also unique)
            // So if guid already listed in pools then this duplicate from alt.table
            // Also note: for added guid not important what case we skip from 2 tables
            // 检查游戏对象 GUID 是否已在 pool_gameobject 中注册，若重复则跳过
            if (uint16 alt_pool_id = IsPartOfAPool<GameObject>(guid))
            {
                sLog.outErrorDb("`pool_gameobject` has guid %u for pool %u that already added to pool %u from `pool_gameobject_template` for gameobject entry %u, skipped.",
                                guid, pool_id, alt_pool_id, entry_id);
                continue;
            }

            if (!mapChecker.CheckAndRemember(data->mapid, pool_id, "pool_gameobject_template", "gameobject guid"))
                continue;

            PoolTemplateData* pPoolTemplate = &mPoolTemplate[pool_id];

            ++count;

            PoolObject plObject = PoolObject(guid, chance);
            PoolGroup<GameObject>& gogroup = mPoolGameobjectGroups[pool_id];
            gogroup.SetPoolId(pool_id);
            gogroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mGameobjectSearchMap.insert(p);
        }
        while (queryResult->NextRow());
        sLog.outString();
        sLog.outString(">> Loaded %u gameobject in pools from `pool_gameobject_template`", count);
    }

    // Pool of pools (加载子池嵌套母池关系)
    mPoolPoolGroups.resize(max_pool_id + 1);
    //                                   1        2            3
    queryResult = WorldDatabase.Query("SELECT pool_id, mother_pool, chance FROM pool_pool");

    count = 0;
    if (!queryResult)
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u pools in pools", count);
    }
    else
    {
        BarGoLink bar2(queryResult->GetRowCount());
        do
        {
            fields = queryResult->Fetch();

            bar2.step();

            uint16 child_pool_id  = fields[0].GetUInt16();
            uint16 mother_pool_id = fields[1].GetUInt16();
            float chance          = fields[2].GetFloat();

            if (mother_pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_pool` mother_pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", mother_pool_id);
                continue;
            }
            if (child_pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_pool` included pool_id (%i) is out of range compared to max pool id in `pool_template`, skipped.", child_pool_id);
                continue;
            }
            if (mother_pool_id == child_pool_id)
            {
                sLog.outErrorDb("`pool_pool` pool_id (%i) includes itself, dead-lock detected, skipped.", child_pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_pool` has an invalid chance (%f) for pool id (%u) in mother pool id (%i), skipped.", chance, child_pool_id, mother_pool_id);
                continue;
            }

            PoolTemplateData* pPoolTemplateMother = &mPoolTemplate[mother_pool_id];

            ++count;

            PoolObject plObject = PoolObject(child_pool_id, chance);
            PoolGroup<Pool>& plgroup = mPoolPoolGroups[mother_pool_id];
            plgroup.SetPoolId(mother_pool_id);
            plgroup.AddEntry(plObject, pPoolTemplateMother->MaxLimit);
            SearchPair p(child_pool_id, mother_pool_id);
            mPoolSearchMap.insert(p);

            // update top independent pool flag (子池归母池调度，取消独立自动生成标记)
            mPoolTemplate[child_pool_id].AutoSpawn = false;
        }
        while (queryResult->NextRow());

        // Now check for circular reference (检测母子池之间是否存在环形引用与跨地图冲突)
        for (uint16 i = 0; i < max_pool_id; ++i)
        {
            std::set<uint16> checkedPools;
            for (SearchMap::iterator poolItr = mPoolSearchMap.find(i); poolItr != mPoolSearchMap.end(); poolItr = mPoolSearchMap.find(poolItr->second))
            {
                // if child pool not have map data then it empty or have not checked child then will checked and all line later
                // 若子池已有地图绑定，则校验并向上传递至母池
                if (MapEntry const* childMapEntry = mPoolTemplate[poolItr->first].mapEntry)
                {
                    if (!mapChecker.CheckAndRemember(childMapEntry->MapID, poolItr->second, "pool_pool", "pool with creature/gameobject"))
                    {
                        mPoolPoolGroups[poolItr->second].RemoveOneRelation(poolItr->first);
                        mPoolSearchMap.erase(poolItr);
                        --count;
                        break;
                    }
                }

                checkedPools.insert(poolItr->first);
                if (checkedPools.find(poolItr->second) != checkedPools.end())
                {
                    std::ostringstream ss;
                    ss << "The pool(s) ";
                    for (uint16 checkedPool : checkedPools)
                        ss << checkedPool << " ";
                    ss << "create(s) a circular reference, which can cause the server to freeze.\nRemoving the last link between mother pool "
                       << poolItr->first << " and child pool " << poolItr->second;
                    sLog.outErrorDb("%s", ss.str().c_str());
                    mPoolPoolGroups[poolItr->second].RemoveOneRelation(poolItr->first);
                    mPoolSearchMap.erase(poolItr);
                    --count;
                    break;
                }
            }
        }

        sLog.outString();
        sLog.outString(">> Loaded %u pools in mother pools", count);
    }

    // check chances integrity (校验所有自动生成池的概率总和及空池告警)
    for (uint16 pool_entry = 0; pool_entry < mPoolTemplate.size(); ++pool_entry)
    {
        auto& poolTemplate = mPoolTemplate[pool_entry];
        if (poolTemplate.AutoSpawn)
        {
            if (!CheckPool(pool_entry))
            {
                sLog.outErrorDb("Pool Id (%u) has all creatures or gameobjects with explicit chance sum <>100 and no equal chance defined. The pool system cannot pick one to spawn.", pool_entry);
                poolTemplate.AutoSpawn = false;
            }
        }
        if (poolTemplate.mapEntry != nullptr || poolTemplate.MaxLimit > 0 || poolTemplate.description != "")
            if (pool_entry <= max_pool_id)
                if (mPoolGameobjectGroups[pool_entry].isEmpty() && mPoolCreatureGroups[pool_entry].isEmpty() && mPoolPoolGroups[pool_entry].isEmpty())
                    sLog.outErrorDb("Pool Template Id (%u) is empty.", pool_entry);
    }

    sLog.outString();
}

// The initialize method will spawn all pools not in an event and not in another pool
// 初始化地图持久化状态对应的所有顶层自动生成池（不含事件池与子池）
void PoolManager::Initialize(MapPersistentState* state)
{
    // spawn pools for expected map or for not initialized shared pools state for non-instanceable maps
    // 在目标地图或未初始化的非副本地图共享状态中初始化各自动生成池
    for (uint16 pool_entry = 0; pool_entry < mPoolTemplate.size(); ++pool_entry)
        if (mPoolTemplate[pool_entry].AutoSpawn)
            InitSpawnPool(*state, pool_entry);
}

// Call to spawn a pool, if cache if true the method will spawn only if cached entry is different
// If it's same, the creature is respawned only (added back to map)
// 生成或轮换生物池成员（若抽中同一生物则直接将其重新加入地图）
template<>
void PoolManager::SpawnPoolGroup<Creature>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid, bool instantly)
{
    if (!mPoolCreatureGroups[pool_id].isEmpty())
        mPoolCreatureGroups[pool_id].SpawnObject(mapState, mPoolTemplate[pool_id].MaxLimit, db_guid, instantly);
}

// Call to spawn a pool, if cache if true the method will spawn only if cached entry is different
// If it's same, the gameobject is respawned only (added back to map)
// 生成或轮换游戏对象池成员
template<>
void PoolManager::SpawnPoolGroup<GameObject>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid, bool instantly)
{
    if (!mPoolGameobjectGroups[pool_id].isEmpty())
        mPoolGameobjectGroups[pool_id].SpawnObject(mapState, mPoolTemplate[pool_id].MaxLimit, db_guid, instantly);
}

// Call to spawn a pool, if cache if true the method will spawn only if cached entry is different
// If it's same, the pool is respawned only
// 生成或轮换子池成员
template<>
void PoolManager::SpawnPoolGroup<Pool>(MapPersistentState& mapState, uint16 pool_id, uint32 sub_pool_id, bool instantly)
{
    if (!mPoolPoolGroups[pool_id].isEmpty())
        mPoolPoolGroups[pool_id].SpawnObject(mapState, mPoolTemplate[pool_id].MaxLimit, sub_pool_id, instantly);
}

/*!
    \param instantly defines if (leaf-)objects are spawned instantly or with fresh respawn timer
    生成指定池中的所有子池、游戏对象及生物；instantly 控制叶子对象是否立即出现还是进入正常刷新倒计时 */
void PoolManager::SpawnPool(MapPersistentState& mapState, uint16 pool_id, bool instantly)
{
    SpawnPoolGroup<Pool>(mapState, pool_id, 0, instantly);
    SpawnPoolGroup<GameObject>(mapState, pool_id, 0, instantly);
    SpawnPoolGroup<Creature>(mapState, pool_id, 0, instantly);
}

// Call to despawn a pool, all gameobjects/creatures in this pool are removed (移除指定对象池内的全部生物、游戏对象及子池)
void PoolManager::DespawnPool(MapPersistentState& mapState, uint16 pool_id)
{
    if (!mPoolCreatureGroups[pool_id].isEmpty())
        mPoolCreatureGroups[pool_id].DespawnObject(mapState);

    if (!mPoolGameobjectGroups[pool_id].isEmpty())
        mPoolGameobjectGroups[pool_id].DespawnObject(mapState);

    if (!mPoolPoolGroups[pool_id].isEmpty())
        mPoolPoolGroups[pool_id].DespawnObject(mapState);
}

// Method that check chance integrity of the creatures and gameobjects in this pool (校验对象池的概率配置是否合法)
bool PoolManager::CheckPool(uint16 pool_id) const
{
    return pool_id <= max_pool_id &&
           mPoolGameobjectGroups[pool_id].CheckPool() &&
           mPoolCreatureGroups[pool_id].CheckPool() &&
           mPoolPoolGroups[pool_id].CheckPool();
}

// Method that check linking all elements to event (校验池中所有元素是否与指定世界事件正确关联)
void PoolManager::CheckEventLinkAndReport(uint16 pool_id, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const
{
    mPoolGameobjectGroups[pool_id].CheckEventLinkAndReport(event_id, creature2event, go2event);
    mPoolCreatureGroups[pool_id].CheckEventLinkAndReport(event_id, creature2event, go2event);
    mPoolPoolGroups[pool_id].CheckEventLinkAndReport(event_id, creature2event, go2event);
}

// Method that exclude some elements from next spawn (将指定生物从下次池抽取中排除或恢复)
template<>
void PoolManager::SetExcludeObject<Creature>(uint16 pool_id, uint32 db_guid_or_pool_id, bool state)
{
    mPoolCreatureGroups[pool_id].SetExcludeObject(db_guid_or_pool_id, state);
}

// 将指定游戏对象从下次池抽取中排除或恢复
template<>
void PoolManager::SetExcludeObject<GameObject>(uint16 pool_id, uint32 db_guid_or_pool_id, bool state)
{
    mPoolGameobjectGroups[pool_id].SetExcludeObject(db_guid_or_pool_id, state);
}

// Call to update the pool when a gameobject/creature part of pool [pool_id] is ready to respawn
// Here we cache only the creature/gameobject whose guid is passed as parameter
// Then the spawn pool call will use this cache to decide
// 当池内某个生物或游戏对象到期准备重生时更新池状态：若属于母池则通过母池轮换，否则直接在当前池中替换生成
template<typename T>
void PoolManager::UpdatePool(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id)
{
    if (uint16 motherpoolid = IsPartOfAPool<Pool>(pool_id))
        SpawnPoolGroup<Pool>(mapState, motherpoolid, pool_id, false);
    else
        SpawnPoolGroup<T>(mapState, pool_id, db_guid_or_pool_id, false);
}

template void PoolManager::UpdatePool<Pool>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id);
template void PoolManager::UpdatePool<GameObject>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id);
template void PoolManager::UpdatePool<Creature>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id);

// 跨地图状态遍历器：在所有匹配的地图持久化状态中生成指定对象池
struct SpawnPoolInMapsWorker
{
    explicit SpawnPoolInMapsWorker(PoolManager& mgr, uint32 pool_id, bool instantly)
        : i_mgr(mgr), i_pool_id(pool_id), i_instantly(instantly) {}

    void operator()(MapPersistentState* state)
    {
        i_mgr.SpawnPool(*state, i_pool_id, i_instantly);
    }

    PoolManager& i_mgr;
    uint32 i_pool_id;
    bool i_instantly;
};

// used for calling from global systems when need spawn pool in all appropriate map persistent states
// 供全局系统调用：在目标地图的所有持久化实例状态中生成对象池
void PoolManager::SpawnPoolInMaps(uint16 pool_id, bool instantly)
{
    PoolTemplateData& poolTemplate = mPoolTemplate[pool_id];

    // pool no have spawns (base at loading algo) (该池无任何关联刷新点，直接跳过)
    if (!poolTemplate.mapEntry)
        return;

    SpawnPoolInMapsWorker worker(*this, pool_id, instantly);
    sMapPersistentStateMgr.DoForAllStatesWithMapId(poolTemplate.mapEntry->MapID, worker);
}

// 跨地图状态遍历器：在所有匹配的地图持久化状态中移除指定对象池
struct DespawnPoolInMapsWorker
{
    explicit DespawnPoolInMapsWorker(PoolManager& mgr, uint32 pool_id)
        : i_mgr(mgr), i_pool_id(pool_id) {}

    void operator()(MapPersistentState* state)
    {
        i_mgr.DespawnPool(*state, i_pool_id);
    }

    PoolManager& i_mgr;
    uint32 i_pool_id;
};

// used for calling from global systems when need spawn pool in all appropriate map persistent states
// 供全局系统调用：在目标地图的所有持久化实例状态中移除对象池
void PoolManager::DespawnPoolInMaps(uint16 pool_id)
{
    PoolTemplateData& poolTemplate = mPoolTemplate[pool_id];

    // pool no have spawns (base at loading algo (该池无任何关联刷新点，直接跳过)
    if (!poolTemplate.mapEntry)
        return;

    DespawnPoolInMapsWorker worker(*this, pool_id);
    sMapPersistentStateMgr.DoForAllStatesWithMapId(poolTemplate.mapEntry->MapID, worker);
}

// 若该池属于指定地图状态则立即初始化生成
void PoolManager::InitSpawnPool(MapPersistentState& mapState, uint16 pool_id)
{
    // spawn pool for expected map (仅在匹配的目标地图上生成)
    if (mPoolTemplate[pool_id].CanBeSpawnedAtMap(mapState.GetMapEntry()))
        SpawnPool(mapState, pool_id, true);
}

// 跨地图状态遍历器：在所有匹配的地图持久化状态中触发对象池轮换更新
template<typename T>
struct UpdatePoolInMapsWorker
{
    explicit UpdatePoolInMapsWorker(PoolManager& mgr, uint32 pool_id, uint32 db_guid_or_pool_id)
        : i_mgr(mgr), i_pool_id(pool_id), i_db_guid_or_pool_id(db_guid_or_pool_id) {}

    void operator()(MapPersistentState* state)
    {
        i_mgr.UpdatePool<T>(*state, i_pool_id, i_db_guid_or_pool_id);
    }

    PoolManager& i_mgr;
    uint32 i_pool_id;
    uint32 i_db_guid_or_pool_id;
};

// 在目标地图的所有持久化状态中更新指定对象池
template<typename T>
void PoolManager::UpdatePoolInMaps(uint16 pool_id, uint32 db_guid_or_pool_id)
{
    PoolTemplateData& poolTemplate = mPoolTemplate[pool_id];

    // pool no have spawns (base at loading algo (该池无任何关联刷新点，直接跳过)
    if (!poolTemplate.mapEntry)
        return;

    UpdatePoolInMapsWorker<T> worker(*this, pool_id, db_guid_or_pool_id);
    sMapPersistentStateMgr.DoForAllStatesWithMapId(poolTemplate.mapEntry->MapID, worker);
}

template void PoolManager::UpdatePoolInMaps<Pool>(uint16 pool_id, uint32 db_guid_or_pool_id);
template void PoolManager::UpdatePoolInMaps<GameObject>(uint16 pool_id, uint32 db_guid_or_pool_id);
template void PoolManager::UpdatePoolInMaps<Creature>(uint16 pool_id, uint32 db_guid_or_pool_id);
