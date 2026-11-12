/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software licensed under GPL version 2
 * Please see the included DOCS/LICENSE.TXT for more information */

#include "AI/ScriptDevAI/include/sc_common.h"

#include "Grids/Cell.h"
#include "Grids/CellImpl.h"
#include "Grids/GridNotifiers.h"
#include "Grids/GridNotifiersImpl.h"

// return closest GO in grid, with range from pSource (获取以 source 为中心、指定范围内最近的特定 Entry 游戏对象)
GameObject* GetClosestGameObjectWithEntry(WorldObject* source, uint32 entry, float maxSearchRange)
{
    GameObject* go = nullptr;

    MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*source, entry, maxSearchRange);
    MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> searcher(go, go_check);

    Cell::VisitGridObjects(source, searcher, maxSearchRange);

    return go;
}

// return closest creature alive in grid, with range from pSource (获取指定范围内最近的特定 Entry 生物)
Creature* GetClosestCreatureWithEntry(WorldObject* source, uint32 entry, float maxSearchRange, bool onlyAlive/*=true*/, bool onlyDead/*=false*/, bool excludeSelf/*=false*/)
{
    Creature* creature = nullptr;

    MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck creature_check(*source, entry, onlyAlive, onlyDead, maxSearchRange, excludeSelf);
    MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(creature, creature_check);

    Cell::VisitGridObjects(source, searcher, maxSearchRange);

    return creature;
}

// 收集指定范围内所有匹配特定 Entry 的游戏对象列表
void GetGameObjectListWithEntryInGrid(GameObjectList& goList, WorldObject* source, uint32 entry, float maxSearchRange)
{
    MaNGOS::GameObjectEntryInPosRangeCheck check(*source, entry, source->GetPositionX(), source->GetPositionY(), source->GetPositionZ(), maxSearchRange);
    MaNGOS::GameObjectListSearcher<MaNGOS::GameObjectEntryInPosRangeCheck> searcher(goList, check);

    Cell::VisitGridObjects(source, searcher, maxSearchRange);
}

// 收集指定范围内匹配多个 Entry 之一的所有游戏对象列表
void GetGameObjectListWithEntryInGrid(GameObjectList& goList, WorldObject* source, std::vector<uint32> const& entries, float maxSearchRange)
{
    MaNGOS::AllGameObjectsMatchingOneEntryInRange check(source, entries, maxSearchRange);
    MaNGOS::GameObjectListSearcher<MaNGOS::AllGameObjectsMatchingOneEntryInRange> searcher(goList, check);

    Cell::VisitGridObjects(source, searcher, maxSearchRange);
}

// 收集指定范围内所有匹配特定 Entry 的生物列表
void GetCreatureListWithEntryInGrid(CreatureList& creatureList, WorldObject* source, uint32 entry, float maxSearchRange)
{
    MaNGOS::AllCreaturesOfEntryInRangeCheck check(source, entry, maxSearchRange);
    MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(creatureList, check);

    Cell::VisitGridObjects(source, searcher, maxSearchRange);
}

// 收集指定范围内匹配多个 Entry 之一的所有生物列表
void GetCreatureListWithEntryInGrid(CreatureList& creatureList, WorldObject* source, std::vector<uint32> const& entries, float maxSearchRange)
{
    MaNGOS::AllCreaturesMatchingOneEntryInRange check(source, entries, maxSearchRange);
    MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesMatchingOneEntryInRange> searcher(creatureList, check);

    Cell::VisitGridObjects(source, searcher, maxSearchRange);
}

// 收集指定距离内的所有玩家列表
void GetPlayerListWithEntryInWorld(PlayerList& playerList, WorldObject* source, float maxSearchRange)
{
    MaNGOS::AnyPlayerInObjectRangeCheck check(source, maxSearchRange);
    MaNGOS::PlayerListSearcher<MaNGOS::AnyPlayerInObjectRangeCheck> searcher(playerList, check);

    Cell::VisitWorldObjects(source, searcher, maxSearchRange);
}
