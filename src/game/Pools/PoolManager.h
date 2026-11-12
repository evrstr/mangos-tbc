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

#ifndef MANGOS_POOLHANDLER_H
#define MANGOS_POOLHANDLER_H

#include "Common.h"
#include "Platform/Define.h"
#include "Entities/Creature.h"
#include "Entities/GameObject.h"

class MapPersistentState;
struct MapEntry;

// 对象池模板元数据（对应数据库 pool_template 表）
struct PoolTemplateData
{
    PoolTemplateData() : mapEntry(nullptr), MaxLimit(0), AutoSpawn(false) {}

    MapEntry const* mapEntry;                               // Map id used for pool creature/gameobject spams. In case non-instanceable map (该池所属地图条目；对于非副本地图)
    // it can be not unique but base at sharing same pool system dynamic data in this case this is not important. (可能与同地图共享池动态数据而不唯一，但此时无影响；若为 nullptr 表示无有效刷新点)
    // nullptr is no spawns by some reason
    uint32  MaxLimit;                                       // 同时允许处于激活状态的对象数量上限
    bool AutoSpawn;                                         // spawn at pool system start (not part of another pool and not part of event spawn) (是否在池系统启动时自动生成：非子池且非事件池)
    std::string description;                                // 池描述信息

    // helpers (辅助判断：该池是否允许在指定地图上生成)
    bool CanBeSpawnedAtMap(MapEntry const* entry) const
    {
        return mapEntry && mapEntry == entry;
    }
};

// 池内单个候选项（生物、游戏对象或子池）及其权重概率
struct PoolObject
{
    uint32  guid;                                           // 数据库 GUID 或子池 ID
    float   chance;                                         // 显式抽取概率（0 表示等概率）
    bool exclude;                                           // 是否临时排除在下次抽取之外

    PoolObject(uint32 _guid, float _chance): guid(_guid), chance(fabs(_chance)), exclude(false) {}

    template<typename T>
    void CheckEventLinkAndReport(uint32 poolId, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const;
};

class Pool                                                  // for Pool of Pool case (用于“池中池”模板特化的占位类型)
{
};

typedef std::set<uint32> SpawnedPoolObjects;
typedef std::map<uint32, uint32> SpawnedPoolPools;

// 记录某张地图持久化状态下当前已生成的池对象与子池计数
class SpawnedPoolData
{
    public:
        SpawnedPoolData() : m_isInitialized(false) {}

        // 判断指定生物、游戏对象或子池当前是否已生成
        template<typename T>
        bool IsSpawnedObject(uint32 db_guid_or_pool_id) const;

        // 获取指定池当前已生成的成员数量
        uint32 GetSpawnedObjects(uint32 pool_id) const;

        // 记录一个已生成的池成员或子池
        template<typename T>
        void AddSpawn(uint32 db_guid_or_pool_id, uint32 pool_id);

        // 移除一个已反生成的池成员或子池记录
        template<typename T>
        void RemoveSpawn(uint32 db_guid_or_pool_id, uint32 pool_id);

        bool IsInitialized() const { return m_isInitialized; }
        void SetInitialized() { m_isInitialized = true; }

        SpawnedPoolObjects const& GetSpawnedCreatures() const { return mSpawnedCreatures; }
        SpawnedPoolObjects const& GetSpawnedGameobjects() const { return mSpawnedGameobjects; }
        SpawnedPoolPools const& GetSpawnedPools() const { return mSpawnedPools; }
    private:
        SpawnedPoolObjects mSpawnedCreatures;
        SpawnedPoolObjects mSpawnedGameobjects;
        SpawnedPoolPools   mSpawnedPools;
        bool m_isInitialized;
};

typedef std::vector<PoolObject> PoolObjectList;

// 单个对象池中同类型成员（生物 / 游戏对象 / 子池）的分组管理模板类
template <class T>
class PoolGroup
{
    public:
        explicit PoolGroup() : poolId(0) { }
        void SetPoolId(uint32 pool_id) { poolId = pool_id; }
        ~PoolGroup() {};
        bool isEmpty() const { return ExplicitlyChanced.empty() && EqualChanced.empty(); }
        void AddEntry(PoolObject& poolitem, uint32 maxentries); // 添加池成员至显式概率列表或等概率列表
        bool CheckPool() const;                                 // 校验显式概率之和是否合法（0 或 100）
        void CheckEventLinkAndReport(int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const;
        PoolObject* RollOne(SpawnedPoolData& spawns, uint32 triggerFrom, MapPersistentState& mapState); // 按权重/随机抽取一个可生成的池成员
        void DespawnObject(MapPersistentState& mapState, uint32 guid = 0); // 移除池中指定或全部已生成成员
        void Despawn1Object(MapPersistentState& mapState, uint32 guid);    // 移除单个已生成成员
        void SpawnObject(MapPersistentState& mapState, uint32 limit, uint32 triggerFrom, bool instantly); // 按上限补充生成池成员
        void SetExcludeObject(uint32 guid, bool state);

        void Spawn1Object(MapPersistentState& mapState, PoolObject* obj, bool instantly); // 生成单个成员
        void ReSpawn1Object(MapPersistentState& mapState, PoolObject* obj);               // 重生同一个成员（仅重新加入地图）
        void RemoveOneRelation(uint16 child_pool_id);                                     // 解除环形依赖中的子池关联

        PoolObjectList const& GetExplicitlyChanced() const { return ExplicitlyChanced; }
        PoolObjectList const& GetEqualChanced() const { return EqualChanced; }

        size_t size() const { return ExplicitlyChanced.size() + EqualChanced.size(); }
    private:
        bool CanSpawn(PoolObject* object, MapPersistentState& mapState);
        
        uint32 poolId;
        PoolObjectList ExplicitlyChanced;                       // 显式指定概率的成员列表
        PoolObjectList EqualChanced;                            // 等概率随机的成员列表
};

// 全局对象池管理器，负责从数据库加载池配置并驱动各地图内的池刷新与轮换
class PoolManager
{
    public:
        PoolManager();
        ~PoolManager() {};

        void LoadFromDB();                                  // 从数据库加载所有对象池、生物池、游戏对象池与池中池配置
        void Initialize(MapPersistentState* state);         // called at new MapPersistentState object create (在新建 MapPersistentState 时初始化自动生成池)

        uint16 GetMaxPoolId() const { return max_pool_id; }

        template<typename T>
        uint16 IsPartOfAPool(uint32 db_guid_or_pool_id) const;

        // Method that tell if the creature/gameobject/pool is part of top level pool and return the pool id if yes
        // 判断生物/游戏对象/子池是否属于某个顶层母池，若是则返回最顶层母池 ID
        template<typename T>
        uint16 IsPartOfTopPool(uint32 db_guid_or_pool_id) const
        {
            if (uint16 pool_id = IsPartOfAPool<T>(db_guid_or_pool_id))
            {
                if (uint16 top_pool_id = IsPartOfTopPool<Pool>(pool_id))
                    return top_pool_id;

                return pool_id;
            }

            return 0;
        }

        template<typename T>
        void SetExcludeObject(uint16 pool_id, uint32 db_guid_or_pool_id, bool state);

        bool CheckPool(uint16 pool_id) const;
        void CheckEventLinkAndReport(uint16 pool_id, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const;

        void SpawnPool(MapPersistentState& mapState, uint16 pool_id, bool instantly);   // 在指定地图状态中生成对象池
        void DespawnPool(MapPersistentState& mapState, uint16 pool_id);                 // 在指定地图状态中移除对象池

        template<typename T>
        void UpdatePool(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id = 0); // 当池成员刷新/死亡时触发池轮换更新

        // used for calling from global systems when need spawn pool in all appropriate map instances
        // 供全局系统（如世界事件）调用：在所有适用地图实例中生成或移除对象池
        void SpawnPoolInMaps(uint16 pool_id, bool instantly);
        void DespawnPoolInMaps(uint16 pool_id);

        // used for calling from global systems when need initialize spawn pool state in appropriate (possible) map persistent state
        // 在匹配的地图持久化状态中初始化指定对象池
        void InitSpawnPool(MapPersistentState& mapState, uint16 pool_id);

        template<typename T>
        void UpdatePoolInMaps(uint16 pool_id, uint32 db_guid_or_pool_id = 0);

        void RemoveAutoSpawnForPool(uint16 pool_id) { mPoolTemplate[pool_id].AutoSpawn = false; }

        typedef std::vector<PoolTemplateData> PoolTemplateDataMap;
        PoolTemplateData const& GetPoolTemplate(uint16 pool_id) const { return mPoolTemplate[pool_id]; }
        PoolGroup<Creature> const& GetPoolCreatures(uint16 pool_id) const  { return mPoolCreatureGroups[pool_id]; }
        PoolGroup<GameObject> const& GetPoolGameObjects(uint16 pool_id) const  { return mPoolGameobjectGroups[pool_id]; }
        PoolGroup<Pool> const& GetPoolPools(uint16 pool_id) const  { return mPoolPoolGroups[pool_id]; }
    protected:
        template<typename T>
        void SpawnPoolGroup(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id, bool instantly);

        uint16 max_pool_id;

        typedef std::vector<PoolGroup<Creature> >   PoolGroupCreatureMap;
        typedef std::vector<PoolGroup<GameObject> > PoolGroupGameObjectMap;
        typedef std::vector<PoolGroup<Pool> >       PoolGroupPoolMap;
        typedef std::pair<uint32, uint16> SearchPair;
        typedef std::map<uint32, uint16> SearchMap;

        PoolTemplateDataMap mPoolTemplate;
        PoolGroupCreatureMap mPoolCreatureGroups;
        PoolGroupGameObjectMap mPoolGameobjectGroups;
        PoolGroupPoolMap mPoolPoolGroups;

        // static maps DB low guid -> pool id (静态反查映射：数据库低位 GUID / 子池 ID -> 所属池 ID)
        SearchMap mCreatureSearchMap;
        SearchMap mGameobjectSearchMap;
        SearchMap mPoolSearchMap;
};

#define sPoolMgr MaNGOS::Singleton<PoolManager>::Instance()

// Method that tell if the creature is part of a pool and return the pool id if yes
// 判断生物是否属于某个对象池，若属于则返回池 ID
template<>
inline uint16 PoolManager::IsPartOfAPool<Creature>(uint32 db_guid) const
{
    SearchMap::const_iterator itr = mCreatureSearchMap.find(db_guid);
    if (itr != mCreatureSearchMap.end())
        return itr->second;

    return 0;
}

// Method that tell if the gameobject is part of a pool and return the pool id if yes
// 判断游戏对象是否属于某个对象池，若属于则返回池 ID
template<>
inline uint16 PoolManager::IsPartOfAPool<GameObject>(uint32 db_guid) const
{
    SearchMap::const_iterator itr = mGameobjectSearchMap.find(db_guid);
    if (itr != mGameobjectSearchMap.end())
        return itr->second;

    return 0;
}

// Method that tell if the pool is part of another pool and return the pool id if yes
// 判断子池是否属于某个母池，若属于则返回母池 ID
template<>
inline uint16 PoolManager::IsPartOfAPool<Pool>(uint32 pool_id) const
{
    SearchMap::const_iterator itr = mPoolSearchMap.find(pool_id);
    if (itr != mPoolSearchMap.end())
        return itr->second;

    return 0;
}

#endif
