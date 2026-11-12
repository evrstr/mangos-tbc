/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#ifndef SC_GRIDSEARCH_H
#define SC_GRIDSEARCH_H

#include "Entities/Object.h"
class GameObject;
class Creature;

// 基于距源对象距离升序排列的比较器结构体
struct ObjectDistanceOrder
{
    const Unit* m_source;

    ObjectDistanceOrder(const Unit* source) : m_source(source) {};

    bool operator()(const WorldObject* left, const WorldObject* right) const
    {
        return m_source->GetDistanceOrder(left, right);
    }
};

// 基于距源对象距离降序排列的比较器结构体
struct ObjectDistanceOrderReversed
{
    const Unit* m_source;

    ObjectDistanceOrderReversed(const Unit* source) : m_source(source) {};

    bool operator()(const WorldObject* left, const WorldObject* right) const
    {
        return !m_source->GetDistanceOrder(left, right);
    }
};

// 获取指定搜索范围内最近的特定 Entry 游戏对象
GameObject* GetClosestGameObjectWithEntry(WorldObject* source, uint32 entry, float maxSearchRange);

// 获取指定搜索范围内最近的特定 Entry 生物（支持仅存活、仅死亡、排除自身过滤）
Creature* GetClosestCreatureWithEntry(WorldObject* source, uint32 entry, float maxSearchRange, bool onlyAlive = true, bool onlyDead = false, bool excludeSelf = false);

// 在网格中获取所有匹配特定 Entry 的游戏对象列表
void GetGameObjectListWithEntryInGrid(GameObjectList& goList, WorldObject* source, uint32 entry, float maxSearchRange);
void GetGameObjectListWithEntryInGrid(GameObjectList& goList, WorldObject* source, std::vector<uint32> const& entries, float maxSearchRange);

// 在网格中获取所有匹配特定 Entry 的生物列表
void GetCreatureListWithEntryInGrid(CreatureList& creatureList, WorldObject* source, uint32 entry, float maxSearchRange);
void GetCreatureListWithEntryInGrid(CreatureList& creatureList, WorldObject* source, std::vector<uint32> const& entries, float maxSearchRange);

// 在世界范围内获取指定距离内的所有玩家列表
void GetPlayerListWithEntryInWorld(PlayerList& playerList, WorldObject* source, float maxSearchRange);

// Used in: hyjalAI.cpp
/*
class AllFriendlyCreaturesInGrid
{
    public:
        AllFriendlyCreaturesInGrid(Unit const* obj) : pUnit(obj) {}
        bool operator() (Unit* u)
        {
            if (u->IsAlive() && u->GetVisibility() == VISIBILITY_ON && u->CanAssist(pUnit))
                return true;

            return false;
        }

    private:
        Unit const* pUnit;
};
*/

#endif
