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

#ifndef MANGOS_GRIDNOTIFIERS_H
#define MANGOS_GRIDNOTIFIERS_H

#include "Entities/UpdateData.h"

#include "Entities/Corpse.h"
#include "Entities/Object.h"
#include "Entities/DynamicObject.h"
#include "Entities/GameObject.h"
#include "Entities/Player.h"
#include "Entities/Unit.h"

#include <functional>
#include <memory>

namespace MaNGOS
{
    /// 可见性通知器：收集并同步摄像机视野内所有世界对象的创建/销毁与更新数据
    struct VisibleNotifier
    {
        Camera& i_camera;
        UpdateData& i_data;
        GuidSet i_clientGUIDs;
        WorldObjectSet i_visibleNow;
        bool i_processSend;

        explicit VisibleNotifier(Camera& c, UpdateData& data, bool processSend) : i_camera(c), i_data(data), i_clientGUIDs(c.GetOwner()->GetClientGuids()), i_processSend(processSend) {}
        template<class T> void Visit(GridRefManager<T>& m);
        void Visit(CameraMapType& /*m*/) {}
        /// 汇总视野变更（含传送船乘客、远距对象、超距销毁及新可见对象光环）并发送数据包
        void Notify(void);
    };

    /// 对象变化通知器：当某个世界对象进入视野或状态变化时，通知周围所有摄像机更新其可见性
    struct VisibleChangesNotifier
    {
        WorldObject& i_object;

        explicit VisibleChangesNotifier(WorldObject& object) : i_object(object), m_unvisitedGuids(i_object.GetClientGuidsIAmAt()) {}
        template<class T> void Visit(GridRefManager<T>&) {}
        void Visit(CameraMapType&);

        GuidSet& GetUnvisitedGuids() { return m_unvisitedGuids; }

        GuidSet m_unvisitedGuids;
    };

    /// 玩家消息投递器：向视野范围内的摄像机玩家广播数据包（可控制是否包含自身）
    struct MessageDeliverer
    {
        Player const& i_player;
        WorldPacket const& i_message;
        bool i_toSelf;
        MessageDeliverer(Player const& pl, WorldPacket const& msg, bool to_self) : i_player(pl), i_message(msg), i_toSelf(to_self) {}
        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    /// 排除指定玩家的消息投递器：向视野内除 i_skipped_receiver 外的所有玩家广播数据包
    struct MessageDelivererExcept
    {
        WorldPacket const& i_message;
        Player const* i_skipped_receiver;

        MessageDelivererExcept(WorldPacket const& msg, Player const* skipped)
            : i_message(msg), i_skipped_receiver(skipped) {}

        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    /// 通用对象消息投递器：向所有访问到的摄像机所有者发送数据包
    struct ObjectMessageDeliverer
    {
        WorldPacket const& i_message;
        explicit ObjectMessageDeliverer(WorldPacket const& msg) : i_message(msg) {}
        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    /// 按距离与阵营过滤的玩家消息投递器
    struct MessageDistDeliverer
    {
        Player const& i_player;
        WorldPacket const& i_message;
        bool i_toSelf;
        bool i_ownTeamOnly;
        float i_dist;

        MessageDistDeliverer(Player const& pl, WorldPacket const& msg, float dist, bool to_self, bool ownTeamOnly)
            : i_player(pl), i_message(msg), i_toSelf(to_self), i_ownTeamOnly(ownTeamOnly), i_dist(dist) {}
        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    /// 按距离过滤的对象消息投递器
    struct ObjectMessageDistDeliverer
    {
        WorldObject const& i_object;
        WorldPacket const& i_message;
        float i_dist;
        ObjectMessageDistDeliverer(WorldObject const& obj, WorldPacket const& msg, float dist) : i_object(obj), i_message(msg), i_dist(dist) {}
        void Visit(CameraMapType& m);
        template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
    };

    /// 网格对象更新收集器：收集待本帧更新的网格对象集合
    struct ObjectUpdater
    {
        ObjectUpdater(WorldObjectUnSet& otus, const uint32& diff) : m_objectToUpdateSet(otus), m_timeDiff(diff) {}
        template<class T> void Visit(GridRefManager<T>& m);
        void Visit(PlayerMapType&) {}
        void Visit(CorpseMapType&) {}
        void Visit(CameraMapType&) {}
        void Visit(CreatureMapType&);

        private:
            WorldObjectUnSet& m_objectToUpdateSet;
            uint32 m_timeDiff;
    };

    /// 玩家移动视野感知通知器：玩家位置更新时触发周围玩家/生物与自身 AI 的 MoveInLineOfSight 检查
    struct PlayerVisitObjectsNotifier
    {
        Player& i_player;
        PlayerVisitObjectsNotifier(Player& pl) : i_player(pl) {}
        template<class T> void Visit(GridRefManager<T>&) {}
#ifdef _MSC_VER
        template<> void Visit(PlayerMapType&);
        template<> void Visit(CreatureMapType&);
#endif
    };

    /// 生物移动视野感知通知器：生物位置更新时触发周围玩家/生物 AI 的 MoveInLineOfSight 检查
    struct CreatureVisitObjectsNotifier
    {
        Creature& i_creature;
        CreatureVisitObjectsNotifier(Creature& c) : i_creature(c) {}
        template<class T> void Visit(GridRefManager<T>&) {}
#ifdef _MSC_VER
        template<> void Visit(PlayerMapType&);
        template<> void Visit(CreatureMapType&);
#endif
    };

    /// 动态法术区域对象（DynamicObject）周期更新访问器：检测范围内目标并施加持久区域光环
    struct DynamicObjectUpdater
    {
        DynamicObject& i_dynobject;
        Unit* i_check;
        bool i_positive;
        bool i_script;
        DynamicObjectUpdater(DynamicObject& dynobject, Unit* caster, bool positive);

        template<class T> inline void Visit(GridRefManager<T>&) {}
#ifdef _MSC_VER
        template<> inline void Visit<Player>(PlayerMapType&);
        template<> inline void Visit<Creature>(CreatureMapType&);
#endif

        /// 对单个候选单位执行法术目标校验与持久光环施加
        void VisitHelper(Unit* target);
    };

    // SEARCHERS & LIST SEARCHERS & WORKERS | 搜索器、列表搜索器与遍历执行器

    /* Model Searcher class: | 搜索器模板结构示例：
    template<class Check>
    struct SomeSearcher
    {
        ResultType& i_result;
        Check & i_check;

        SomeSearcher(ResultType& result, Check & check)
            : i_phaseMask(check.GetFocusObject().GetPhaseMask()), i_result(result), i_check(check) {}

        void Visit(CreatureMapType &m);
        {
            ..some code fast return if result found

            for(CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                if (!itr->getSource()->InSamePhase(i_phaseMask))
                    continue;

                if (!i_check(itr->getSource()))
                    continue;

                ..some code for update result and possible stop search
            }
        }

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED> &) {}
    };
    */

    // WorldObject searchers & workers | WorldObject 通用搜索器与执行器

    /// 查找首个满足条件的 WorldObject
    template<class Check>
    struct WorldObjectSearcher
    {
        WorldObject*& i_object;
        Check& i_check;

        WorldObjectSearcher(WorldObject*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(GameObjectMapType& m);
        void Visit(PlayerMapType& m);
        void Visit(CreatureMapType& m);
        void Visit(CorpseMapType& m);
        void Visit(DynamicObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct WorldObjectListSearcher
    {
        WorldObjectList& i_objects;
        Check& i_check;

        WorldObjectListSearcher(WorldObjectList& objects, Check& check) : i_objects(objects), i_check(check) {}

        void Visit(PlayerMapType& m);
        void Visit(CreatureMapType& m);
        void Visit(CorpseMapType& m);
        void Visit(GameObjectMapType& m);
        void Visit(DynamicObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Do>
    struct WorldObjectWorker
    {
        Do const& i_do;

        explicit WorldObjectWorker(Do const& _do) : i_do(_do) {}

        void Visit(GameObjectMapType& m)
        {
            for (GameObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                i_do(itr->getSource());
        }

        void Visit(PlayerMapType& m)
        {
            for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                i_do(itr->getSource());
        }
        void Visit(CreatureMapType& m)
        {
            for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                i_do(itr->getSource());
        }

        void Visit(CorpseMapType& m)
        {
            for (CorpseMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                i_do(itr->getSource());
        }

        void Visit(DynamicObjectMapType& m)
        {
            for (DynamicObjectMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                i_do(itr->getSource());
        }

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Gameobject searchers | GameObject 搜索器

    /// 查找首个满足条件的 GameObject
    template<class Check>
    struct GameObjectSearcher
    {
        GameObject*& i_object;
        Check& i_check;

        GameObjectSearcher(GameObject*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(GameObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Last accepted by Check GO if any (Check can change requirements at each call)
    // 返回最后一个通过 Check 的 GameObject（Check 可在每次匹配后收紧范围条件，用于找最近目标）
    template<class Check>
    struct GameObjectLastSearcher
    {
        GameObject*& i_object;
        Check& i_check;

        GameObjectLastSearcher(GameObject*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(GameObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct GameObjectListSearcher
    {
        GameObjectList& i_objects;
        Check& i_check;

        GameObjectListSearcher(GameObjectList& objects, Check& check) : i_objects(objects), i_check(check) {}

        void Visit(GameObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct DynamicObjectListSearcher
    {
        DynamicObjectList& i_objects;
        Check& i_check;

        DynamicObjectListSearcher(DynamicObjectList& objects, Check& check) : i_objects(objects), i_check(check) {}

        void Visit(DynamicObjectMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Unit searchers | Unit 单位搜索器

    // First accepted by Check Unit if any | 查找首个通过 Check 的 Unit（含玩家与生物）
    template<class Check>
    struct UnitSearcher
    {
        Unit*& i_object;
        Check& i_check;

        UnitSearcher(Unit*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(CreatureMapType& m);
        void Visit(PlayerMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Last accepted by Check Unit if any (Check can change requirements at each call)
    // 返回最后一个通过 Check 的 Unit（Check 可动态收缩条件，用于寻找最优/最近单位）
    template<class Check>
    struct UnitLastSearcher
    {
        Unit*& i_object;
        Check& i_check;

        UnitLastSearcher(Unit*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(CreatureMapType& m);
        void Visit(PlayerMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // All accepted by Check units if any | 收集所有通过 Check 的 Unit 到列表
    template<class Check>
    struct UnitListSearcher
    {
        UnitList& i_objects;
        Check& i_check;

        UnitListSearcher(UnitList& objects, Check& check) : i_objects(objects), i_check(check) {}

        void Visit(PlayerMapType& m);
        void Visit(CreatureMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Creature searchers | Creature 生物搜索器

    /// 查找首个满足条件的 Creature
    template<class Check>
    struct CreatureSearcher
    {
        Creature*& i_object;
        Check& i_check;

        CreatureSearcher(Creature*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(CreatureMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Last accepted by Check Creature if any (Check can change requirements at each call)
    // 返回最后一个通过 Check 的 Creature（常用于查找最近生物）
    template<class Check>
    struct CreatureLastSearcher
    {
        Creature*& i_object;
        Check& i_check;

        CreatureLastSearcher(Creature*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(CreatureMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct CreatureListSearcher
    {
        CreatureList& i_objects;
        Check& i_check;

        CreatureListSearcher(CreatureList& objects, Check& check) : i_objects(objects), i_check(check) {}

        void Visit(CreatureMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Do>
    struct CreatureWorker
    {
        Do& i_do;

        CreatureWorker(WorldObject const* /*searcher*/, Do& _do) : i_do(_do) {}

        void Visit(CreatureMapType& m)
        {
            for (CreatureMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                i_do(itr->getSource());
        }

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // Player searchers | Player 玩家与摄像机搜索器/执行器

    /// 查找首个满足条件的玩家
    template<class Check>
    struct PlayerSearcher
    {
        Player*& i_object;
        Check& i_check;

        PlayerSearcher(Player*& result, Check& check) : i_object(result), i_check(check) {}

        void Visit(PlayerMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Check>
    struct PlayerListSearcher
    {
        PlayerList& i_objects;
        Check& i_check;

        PlayerListSearcher(PlayerList& objects, Check& check)
            : i_objects(objects), i_check(check) {}

        void Visit(PlayerMapType& m);

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Do>
    struct PlayerWorker
    {
        Do& i_do;

        explicit PlayerWorker(Do& _do) : i_do(_do) {}

        void Visit(PlayerMapType& m)
        {
            for (PlayerMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
                i_do(itr->getSource());
        }

        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    template<class Do>
    struct CameraDistWorker
    {
        WorldObject const* i_searcher;
        float i_dist;
        Do& i_do;

        CameraDistWorker(WorldObject const* searcher, float _dist, Do& _do)
            : i_searcher(searcher), i_dist(_dist), i_do(_do) {}

        void Visit(CameraMapType& m)
        {
            for (CameraMapType::iterator itr = m.begin(); itr != m.end(); ++itr)
            {
                Camera* camera = itr->getSource();
                if (camera->GetBody()->IsWithinDist(i_searcher, i_dist))
                    i_do(camera->GetOwner());
            }
        }
        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    struct CameraDistLambdaWorker
    {
        WorldObject const* i_searcher;
        float i_dist;
        std::function<void(Player*)> const& i_do;

        CameraDistLambdaWorker(WorldObject const* searcher, float _dist, std::function<void(Player*)> const& _do)
            : i_searcher(searcher), i_dist(_dist), i_do(_do) {}

        void Visit(CameraMapType& m)
        {
            for (auto& itr : m)
            {
                Camera* camera = itr.getSource();
                if (camera->GetBody()->IsWithinDist(i_searcher, i_dist))
                    i_do(camera->GetOwner());
            }
        }
        template<class NOT_INTERESTED> void Visit(GridRefManager<NOT_INTERESTED>&) {}
    };

    // CHECKS && DO classes | 条件谓词（Check）与动作仿函数（Do）

    /* Model Check class: | 条件谓词类示例：
    class SomeCheck
    {
        public:
            SomeCheck(SomeObjecType const* fobj, ..some other args) : i_fobj(fobj), ...other inits {}
            WorldObject const& GetFocusObject() const { return *i_fobj; }
            bool operator()(Creature* u)                    and for other intresting typs (Player/GameObject/Camera
            {
                return ..(code return true if Object fit to requirenment);
            }
            template<class NOT_INTERESTED> bool operator()(NOT_INTERESTED*) { return false; }
        private:
            SomeObjecType const* i_fobj;                    // Focus object used for check distance from, phase, so place in world | 作为距离与相位基准的焦点对象
            ..other values need for check
    };
    */

    // Can only work for unit since WO -> Corpse reaction is undefined | 仅适用于 Unit（WorldObject 到 Corpse 的阵营敌友关系未定义）；用于食尸技能目标判定
    class CannibalizeObjectCheck
    {
        public:
            CannibalizeObjectCheck(Unit const* fobj, float range) : i_fobj(fobj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_fobj; }
            bool operator()(Player* u)
            {
                if (i_fobj->CanAssist(u) || u->IsAlive() || u->IsTaxiFlying())
                    return false;

                return i_fobj->IsWithinDistInMap(u, i_range);
            }
            bool operator()(Corpse* u);
            bool operator()(Creature* u)
            {
                if (i_fobj->CanAssist(u) || u->IsAlive() || u->IsTaxiFlying() ||
                        (u->GetCreatureTypeMask() & CREATURE_TYPEMASK_HUMANOID_OR_UNDEAD) == 0)
                    return false;

                return i_fobj->IsWithinDistInMap(u, i_range);
            }
            template<class NOT_INTERESTED> bool operator()(NOT_INTERESTED*) { return false; }
        private:
            Unit const* i_fobj;
            float i_range;
    };

    class TauntFlagObjectCheck
    {
        public:
            TauntFlagObjectCheck(Unit const* fobj, float range) : i_fobj(fobj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_fobj; }
            bool operator()(Player* u)
            {
                if (i_fobj->CanAssist(u) || u->IsAlive() || u->IsTaxiFlying())
                    return false;

                return i_fobj->IsWithinDistInMap(u, i_range);
            }
            bool operator()(Corpse* u);
            template<class NOT_INTERESTED> bool operator()(NOT_INTERESTED*) { return false; }
        private:
            Unit const* i_fobj;
            float i_range;
    };

    // WorldObject do classes | WorldObject 动作执行器

    /// 重生执行器：触发指定生物或游戏对象重生
    class RespawnDo
    {
        public:
            RespawnDo() {}
            void operator()(Creature* u) const;
            void operator()(GameObject* u) const;
            void operator()(WorldObject*) const {}
            void operator()(Corpse*) const {}
    };

    // GameObject checks | GameObject 过滤谓词

    /// 检查范围内是否存在已生成且匹配的法术焦点（Spell Focus）游戏对象
    class GameObjectFocusCheck
    {
        public:
            GameObjectFocusCheck(WorldObject const* unit, uint32 focusId) : i_object(unit), i_focusId(focusId) {}
            WorldObject const& GetFocusObject() const { return *i_object; }
            bool operator()(GameObject* go) const
            {
                GameObjectInfo const* goInfo = go->GetGOInfo();
                if (goInfo->type != GAMEOBJECT_TYPE_SPELL_FOCUS)
                    return false;

                if (!go->IsSpawned())
                    return false;

                if (goInfo->spellFocus.focusId != i_focusId)
                    return false;

                float dist = (float)goInfo->spellFocus.dist;

                return go->IsWithinDistInMap(i_object, dist);
            }
        private:
            WorldObject const* i_object;
            uint32 i_focusId;
    };

    // Find the nearest Fishing hole and return true only if source object is in range of hole
    // 查找最近的鱼点（Fishing Hole），仅当源对象处于鱼点交互半径内时返回 true
    class NearestGameObjectFishingHoleCheck
    {
        public:
            NearestGameObjectFishingHoleCheck(WorldObject const& obj, float range) : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return i_obj; }
            bool operator()(GameObject* go)
            {
                if (go->GetGOInfo()->type == GAMEOBJECT_TYPE_FISHINGHOLE && go->IsSpawned() && i_obj.IsWithinDistInMap(go, i_range) && i_obj.IsWithinDistInMap(go, (float)go->GetGOInfo()->fishinghole.radius))
                {
                    i_range = i_obj.GetDistance(go, true, DIST_CALC_COMBAT_REACH);
                    return true;
                }
                return false;
            }
            float GetLastRange() const { return i_range; }
        private:
            WorldObject const& i_obj;
            float  i_range;

            // prevent clone | 禁止拷贝构造
            NearestGameObjectFishingHoleCheck(NearestGameObjectFishingHoleCheck const&);
    };

    // Success at unit in range, range update for next check (this can be used with GameobjectLastSearcher to find nearest GO)
    // 检查指定 entry 的 GameObject 是否在范围内，并收紧 i_range（配合 GameObjectLastSearcher 查找最近目标）
    class NearestGameObjectEntryInObjectRangeCheck
    {
        public:
            NearestGameObjectEntryInObjectRangeCheck(WorldObject const& obj, uint32 entry, float range) : i_obj(obj), i_entry(entry), i_range(range) {}
            WorldObject const& GetFocusObject() const { return i_obj; }
            bool operator()(GameObject* go)
            {
                if (go->GetEntry() == i_entry && i_obj.IsWithinDistInMap(go, i_range))
                {
                    i_range = i_obj.GetDistance(go);        // use found GO range as new range limit for next check | 将当前命中距离更新为下次检查的距离上限
                    return true;
                }
                return false;
            }
            float GetLastRange() const { return i_range; }
        private:
            WorldObject const& i_obj;
            uint32 i_entry;
            float  i_range;

            // prevent clone this object | 禁止拷贝此对象
            NearestGameObjectEntryInObjectRangeCheck(NearestGameObjectEntryInObjectRangeCheck const&);
    };

    // Success at go in range | 匹配指定 entry 集合中位于对象范围内的全部 GameObject
    class AllGameObjectEntriesListInObjectRangeCheck
    {
        public:
            AllGameObjectEntriesListInObjectRangeCheck(WorldObject const& obj, std::set<uint32>& entries, float range, bool is3D = true) : i_obj(obj), i_entries(entries), i_range(range), i_is3D(is3D), i_foundOutOfRange(false) {}
            WorldObject const& GetFocusObject() const { return i_obj; }
            bool operator()(GameObject* go)
            {
                if (i_entries.find(go->GetEntry()) != i_entries.end())
                {
                    if (i_obj.IsWithinDistInMap(go, i_range, i_is3D))
                        return true;
                    i_foundOutOfRange = true;
                }
                return false;
            }

            std::vector<uint32> i_ranges;
            bool FoundOutOfRange() const { return i_foundOutOfRange; }
        private:
            WorldObject const& i_obj;
            std::set<uint32>& i_entries;
            float  i_range;
            bool   i_is3D;
            bool   i_foundOutOfRange;

            // prevent clone this object
            AllGameObjectEntriesListInObjectRangeCheck(AllGameObjectEntriesListInObjectRangeCheck const&);
    };
    
    // combine with above somehow? fuck
    class AllGameObjectsMatchingOneEntryInRange
    {
        public:
            AllGameObjectsMatchingOneEntryInRange(WorldObject const* pObject, std::vector<uint32> const& entries, float fMaxRange)
                : m_pObject(pObject), entries(entries), m_fRange(fMaxRange) {}
            bool operator() (GameObject* pGo)
            {
                for (const auto entry : entries) {
                    if (pGo->GetEntry() == entry && m_pObject->IsWithinDist(pGo, m_fRange, false)) {
                        return true;
                    }
                }
                return false;
            }

        private:
            WorldObject const* m_pObject;
            std::vector<uint32> entries;
            float m_fRange;
    };

    // x y z version of above | 基于坐标 (x, y, z) 的 GameObject 集合范围匹配版本
    class AllGameObjectEntriesListInPosRangeCheck
    {
    public:
        AllGameObjectEntriesListInPosRangeCheck(float x, float y, float z, std::set<uint32>& entries, float range, bool is3D = true) : i_x(x), i_y(y), i_z(z), i_entries(entries), i_range(range), i_is3D(is3D) {}
        bool operator()(GameObject* go)
        {
            if (go->IsSpawned() && i_entries.find(go->GetEntry()) != i_entries.end() && go->GetDistance(i_x, i_y, i_z, DIST_CALC_COMBAT_REACH) < i_range)
                return true;

            return false;
        }

        std::vector<uint32> i_ranges;
    private:
        float i_x, i_y, i_z;
        std::set<uint32>& i_entries;
        float  i_range;
        bool   i_is3D;

        // prevent clone this object | 禁止拷贝此对象
        AllGameObjectEntriesListInPosRangeCheck(AllGameObjectEntriesListInObjectRangeCheck const&);
    };

    // Success at gameobject in range of xyz, range update for next check (this can be use with GameobjectLastSearcher to find nearest GO)
    // 查找指定坐标 (x, y, z) 范围内最近的指定 entry GameObject（动态缩小距离上限）
    class NearestGameObjectEntryInPosRangeCheck
    {
        public:
            NearestGameObjectEntryInPosRangeCheck(WorldObject const& obj, uint32 entry, float x, float y, float z, float range)
                : i_obj(obj), i_entry(entry), i_x(x), i_y(y), i_z(z), i_range(range) {}

            WorldObject const& GetFocusObject() const { return i_obj; }

            bool operator()(GameObject* go)
            {
                if (go->GetEntry() == i_entry && go->IsWithinDist3d(i_x, i_y, i_z, i_range))
                {
                    // use found GO range as new range limit for next check | 以命中距离更新后续搜索的距离上限
                    i_range = go->GetDistance(i_x, i_y, i_z);
                    return true;
                }

                return false;
            }

            float GetLastRange() const { return i_range; }

        private:
            WorldObject const& i_obj;
            uint32 i_entry;
            float i_x, i_y, i_z;
            float i_range;

            // prevent clone this object | 禁止拷贝此对象
            NearestGameObjectEntryInPosRangeCheck(NearestGameObjectEntryInPosRangeCheck const&);
    };

    // Success at gameobject with entry in range of provided xyz | 匹配指定坐标 (x, y, z) 范围内指定 entry 的 GameObject
    class GameObjectEntryInPosRangeCheck
    {
        public:
            GameObjectEntryInPosRangeCheck(WorldObject const& obj, uint32 entry, float x, float y, float z, float range)
                : i_obj(obj), i_entry(entry), i_x(x), i_y(y), i_z(z), i_range(range) {}

            WorldObject const& GetFocusObject() const { return i_obj; }

            bool operator()(GameObject* go)
            {
                return go->GetEntry() == i_entry && go->IsWithinDist3d(i_x, i_y, i_z, i_range);
            }

            float GetLastRange() const { return i_range; }

        private:
            WorldObject const& i_obj;
            uint32 i_entry;
            float i_x, i_y, i_z;
            float i_range;

            // prevent clone this object
            GameObjectEntryInPosRangeCheck(GameObjectEntryInPosRangeCheck const&);
    };

    class GameObjectInPosRangeCheck
    {
        public:
            GameObjectInPosRangeCheck(WorldObject const& obj, float x, float y, float z, float range)
                : i_obj(obj), i_x(x), i_y(y), i_z(z), i_range(range) {}

            WorldObject const& GetFocusObject() const { return i_obj; }

            bool operator()(GameObject* go)
            {
                return go->IsWithinDist3d(i_x, i_y, i_z, i_range);
            }

            float GetLastRange() const { return i_range; }

        private:
            WorldObject const& i_obj;
            float i_x, i_y, i_z;
            float i_range;

            // prevent clone this object
            GameObjectInPosRangeCheck(GameObjectEntryInPosRangeCheck const&);
    };

    // Unit checks | Unit 单位过滤谓词

    /// 查找范围内已损失生命值绝对值最大的友方单位
    class MostHPMissingInRangeCheck
    {
        public:
            MostHPMissingInRangeCheck(Unit const* obj, float range, float hp, bool onlyInCombat, bool targetSelf) : i_obj(obj), i_range(range), i_hp(hp), i_onlyInCombat(onlyInCombat), i_targetSelf(targetSelf) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (!u->IsAlive() || (i_onlyInCombat && !u->IsInCombat()))
                    return false;

                if (!i_targetSelf && u == i_obj)
                    return false;

                if (i_obj->CanAssist(u) && i_obj->IsWithinDistInMap(u, i_range))
                {
                    if (u->GetMaxHealth() - u->GetHealth() > i_hp)
                    {
                        i_hp = u->GetMaxHealth() - u->GetHealth();
                        return true;
                    }
                }
                return false;
            }
        private:
            Unit const* i_obj;
            float i_range;
            float i_hp;
            bool i_onlyInCombat;
            bool i_targetSelf;
    };

    /// 查找范围内已损失生命值百分比最高的友方单位
    class MostHPPercentMissingInRangeCheck
    {
        public:
            MostHPPercentMissingInRangeCheck(Unit const* obj, float range, float hp, bool onlyInCombat, bool targetSelf) : i_obj(obj), i_range(range), i_hp(hp), i_onlyInCombat(onlyInCombat), i_targetSelf(targetSelf) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (!u->IsAlive() || (i_onlyInCombat && !u->IsInCombat()))
                    return false;

                if (!i_targetSelf && u == i_obj)
                    return false;

                if (i_obj->CanAssist(u) && i_obj->IsWithinDistInMap(u, i_range))
                {
                    if (100.f - u->GetHealthPercent() > i_hp)
                    {
                        i_hp = 100.f - u->GetHealthPercent();
                        return true;
                    }
                }
                return false;
            }
        private:
            Unit const* i_obj;
            float i_range;
            float i_hp;
            bool i_onlyInCombat;
            bool i_targetSelf;
    };

    /// 检查范围内满足指定 UnitCondition 条件的存活友方单位
    class FriendlyEligibleUnitConditionCheck
    {
        public:
            FriendlyEligibleUnitConditionCheck(Unit const* obj, float range, int32 unitConditionId) : i_obj(obj), i_range(range), i_unitConditionId(unitConditionId){}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (!u->IsAlive())
                    return false;

                if (i_obj->CanAssist(u) && i_obj->IsWithinCombatDistInMap(u, i_range))
                {
                    return u->IsUnitConditionSatisfied(i_unitConditionId, i_obj);
                }
                return false;
            }
        private:
            Unit const* i_obj;
            float i_range;
            int32 i_unitConditionId;
    };

    /// 检查范围内处于受控/减速状态且可被指定驱散/机制掩码解除的战斗中友方单位
    class FriendlyEligibleDispelInRangeCheck
    {
        public:
            FriendlyEligibleDispelInRangeCheck(Unit const* obj, float range, uint32 dispelMask, uint32 mechanicMask, bool self) :
                i_obj(obj), i_range(range), m_dispelMask(dispelMask), m_mechanicMask(mechanicMask), m_self(self) {}
            Unit const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (!u->IsAlive() || !u->IsInCombat() || !i_obj->CanAssist(u) || !i_obj->IsWithinDistInMap(u, i_range))
                    return false;

                if (!m_self && i_obj == u)
                    return false;

                if (!u->IsImmobilizedState() && !u->GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED) && !u->isFrozen() && !u->IsCrowdControlled())
                    return false;

                if (!m_dispelMask && !m_mechanicMask)
                    return true;

                return u->HasMechanicMaskOrDispelMaskAura(m_dispelMask, m_mechanicMask, i_obj);
            }
        private:
            Unit const* i_obj;
            float i_range;
            uint32 m_dispelMask;
            uint32 m_mechanicMask;
            bool m_self;
    };

    /// 检查范围内处于战斗中且缺少指定增益光环的友方单位
    class FriendlyMissingBuffInRangeInCombatCheck
    {
        public:
            FriendlyMissingBuffInRangeInCombatCheck(Unit const* obj, float range, uint32 spellid, uint32 spawnGroupId) : i_obj(obj), i_range(range), i_spell(spellid), i_spawnGroupId(spawnGroupId) {}
            Unit const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                return u->IsAlive() && u->IsInCombat() && i_obj->CanAssist(u) && i_obj->IsWithinDistInMap(u, i_range) && !(u->HasAura(i_spell, EFFECT_INDEX_0) || u->HasAura(i_spell, EFFECT_INDEX_1) || u->HasAura(i_spell, EFFECT_INDEX_2))
                    && (i_spawnGroupId == 0 || (u->IsCreature() && static_cast<Creature*>(u)->GetCreatureGroup() && static_cast<Creature*>(u)->GetCreatureGroup()->GetGroupId() == i_spawnGroupId));;
            }
        private:
            Unit const* i_obj;
            float i_range;
            uint32 i_spell;
            uint32 i_spawnGroupId;
    };

    /// 检查范围内（不限战斗状态）缺少指定增益光环的友方单位
    class FriendlyMissingBuffInRangeNotInCombatCheck
    {
        public:
            FriendlyMissingBuffInRangeNotInCombatCheck(Unit const* obj, float range, uint32 spellid, uint32 spawnGroupId) : i_obj(obj), i_range(range), i_spell(spellid), i_spawnGroupId(spawnGroupId) {}
            Unit const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                return u->IsAlive() && i_obj->CanAssist(u) && i_obj->IsWithinDistInMap(u, i_range) && !(u->HasAura(i_spell, EFFECT_INDEX_0) || u->HasAura(i_spell, EFFECT_INDEX_1) || u->HasAura(i_spell, EFFECT_INDEX_2))
                    && (i_spawnGroupId == 0 || (u->IsCreature() && static_cast<Creature*>(u)->GetCreatureGroup() && static_cast<Creature*>(u)->GetCreatureGroup()->GetGroupId() == i_spawnGroupId));
            }
        private:
            Unit const* i_obj;
            float i_range;
            uint32 i_spell;
            uint32 i_spawnGroupId;
    };

    /// 匹配范围内可见且可被法术攻击的敌对单位（忽略图腾）
    class AnyUnfriendlyUnitInObjectRangeCheck
    {
        public:
            AnyUnfriendlyUnitInObjectRangeCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range)
            {
                i_controlledByPlayer = obj->IsControlledByPlayer();
            }
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u) const
            {
                // ignore totems | 忽略图腾单位
                if (u->GetTypeId() == TYPEID_UNIT && ((Creature*)u)->IsTotem())
                    return false;
                
                return u->IsAlive() && i_obj->CanAttackSpell(u) && i_obj->IsWithinDistInMap(u, i_range) && i_obj->IsWithinLOSInMap(u);
            }
        private:
            WorldObject const* i_obj;
            bool i_controlledByPlayer;
            float i_range;
    };

    /// 匹配范围内可被指定法术协助（治疗/增益）的存活单位
    class AnySpellAssistableUnitInObjectRangeCheck
    {
        public:
            AnySpellAssistableUnitInObjectRangeCheck(WorldObject const* obj, SpellEntry const* spellInfo, float range, bool ignorePhase = false)
                : i_obj(obj), i_spellInfo(spellInfo), i_range(range), i_ignorePhase(ignorePhase) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                return u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range, true, i_ignorePhase) && i_obj->CanAssistSpell(u, i_spellInfo);
            }
        private:
            WorldObject const* i_obj;
            SpellEntry const* i_spellInfo;
            float i_range;
            bool i_ignorePhase;
    };
    
    class AnyFriendlyUnitInObjectRangeCheck
    {
        public:
            AnyFriendlyUnitInObjectRangeCheck(WorldObject const* obj, float range, bool ignorePhase = false)
                : i_obj(obj), i_range(range), i_ignorePhase(ignorePhase) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                return u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range, true, i_ignorePhase) && i_obj->CanAssistSpell(u);
            }
        private:
            WorldObject const* i_obj;
            float i_range;
            bool i_ignorePhase;
    };

    /// 匹配范围内对给定法术友好的存活单位（若指定队伍则仅限同队玩家及其宠物）
    class AnyFriendlyOrGroupMemberUnitInUnitRangeCheck
    {
        public:
            AnyFriendlyOrGroupMemberUnitInUnitRangeCheck(Unit const* obj, Unit const* target, Group const* group, SpellEntry const* spellInfo, float range)
                : i_group(group), i_obj(obj), i_target(target), i_spellInfo(spellInfo), i_range(range) {}
            Unit const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                if (!u->IsAlive() || !i_target->IsWithinDistInMap(u, i_range) || !i_obj->CanAssistSpell(u, i_spellInfo))
                    return false;

                //if group is defined then we apply group members only filtering | 若指定了队伍，则仅允许该小队/团队成员及其宠物通过
                if (i_group)
                {
                    switch (u->GetTypeId())
                    {
                        case TYPEID_PLAYER:
                        {
                            if (i_group != static_cast<Player*>(u)->GetGroup())
                                return false;

                            break;
                        }
                        case TYPEID_UNIT:
                        {
                            Creature* creature = static_cast<Creature*>(u);

                            //the only other possible group members besides players are pets, we exclude anyone else
                            // 除玩家外仅有宠物可能属于队伍，其余非宠物生物一律排除
                            if (!creature->IsPet())
                                return false;

                            Group* group = nullptr;

                            if (Unit* owner = creature->GetOwner(nullptr, true))
                            {
                                if (owner->GetTypeId() == TYPEID_PLAYER)
                                {
                                    group = static_cast<Player*>(owner)->GetGroup();
                                }
                            }

                            if (i_group != group)
                                return false;

                            break;
                        }
                        default: return false;
                    }
                }
                else if (u->IsCreature() && !static_cast<Creature*>(u)->IsPet())
                    return false;

                return true;
            }
        private:
            Group const* i_group;
            Unit const* i_obj;
            Unit const* i_target;
            SpellEntry const* i_spellInfo;
            float i_range;
    };

    class AnyUnitInObjectRangeCheck
    {
        public:
            AnyUnitInObjectRangeCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                return u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range);
            }
        private:
            WorldObject const* i_obj;
            float i_range;
    };

    class AnyUnitFulfillingConditionInRangeCheck
    {
        public:
            AnyUnitFulfillingConditionInRangeCheck(WorldObject const* obj, std::function<bool(Unit*)> functor, float radius, DistanceCalculation type = DIST_CALC_COMBAT_REACH)
                    : i_obj(obj), i_functor(functor), i_range(radius), i_type(type) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                return i_functor(u) && i_obj->GetDistance(u, true, i_type) <= i_range;
            }
        private:
            WorldObject const* i_obj;
            std::function<bool(Unit*)> i_functor;
            float i_range;
            DistanceCalculation i_type;
    };

    // Success at unit in range, range update for next check (this can be use with UnitLastSearcher to find nearest unit)
    // 查找范围内最近的可攻击敌对单位，并在命中后收缩 m_range 上限（配合 UnitLastSearcher 使用）
    class NearestAttackableUnitInObjectRangeCheck
    {
        public:
            NearestAttackableUnitInObjectRangeCheck(Unit* source, Unit* owner, float range) : m_source(source), m_owner(owner), m_range(range) {}
            NearestAttackableUnitInObjectRangeCheck(NearestAttackableUnitInObjectRangeCheck const&) =  delete;

            Unit const& GetFocusObject() const { return *m_source; }

            bool operator()(Unit* currUnit)
            {
                if (currUnit->IsAlive() && (m_source->IsAttackedBy(currUnit) || (m_owner && m_owner->IsAttackedBy(currUnit)) || m_source->IsEnemy(currUnit) || currUnit->IsEnemy(m_source))
                    && m_source->CanAttack(currUnit)
                    && currUnit->IsVisibleForOrDetect(m_source, m_source, false)
                    && m_source->IsWithinDistInMap(currUnit, m_range))
                {
                    m_range = m_source->GetDistance(currUnit);        // use found unit range as new range limit for next check | 将当前距离更新为下一次检查的范围上限
                    return true;
                }

                return false;
            }

        private:
            Unit*       m_source;
            Unit*       m_owner;
            float       m_range;
    };

    /// 匹配范围内可被范围攻击法术命中的敌对单位（忽略图腾）
    class AnyAoETargetUnitInObjectRangeCheck
    {
        public:
            AnyAoETargetUnitInObjectRangeCheck(WorldObject const* obj, SpellEntry const* spellInfo, float range, bool ignorePhase = false)
                : i_obj(obj), i_spellInfo(spellInfo), i_range(range), i_ignorePhase(ignorePhase)
            {
                i_targetForPlayer = i_obj->IsControlledByPlayer();
            }
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Unit* u)
            {
                // Check contains checks for: live, non-selectable, non-attackable flags, flight check and GM check, ignore totems
                // CanAttackSpell 已包含存活、不可选中、不可攻击标记、飞行及 GM 检查，此处额外忽略图腾
                if (u->GetTypeId() == TYPEID_UNIT && ((Creature*)u)->IsTotem())
                    return false;

                return i_obj->CanAttackSpell(u, i_spellInfo) && i_obj->IsWithinDistInMap(u, i_range, true, i_ignorePhase);
            }

        private:
            WorldObject const* i_obj;
            SpellEntry const* i_spellInfo;
            float i_range;
            bool i_targetForPlayer;
            bool i_ignorePhase;
    };

    // do attack at call of help to friendly crearture | 呼救执行器：通知范围内有视线的友方生物协助攻击敌人
    class CallOfHelpCreatureInRangeDo
    {
        public:
            CallOfHelpCreatureInRangeDo(Unit* funit, Unit* enemy, float range)
                : i_funit(funit), i_enemy(enemy), i_range(range)
            {}
            void operator()(Creature* u);

        private:
            Unit* const i_funit;
            Unit* const i_enemy;
            float i_range;
    };

    class AnyDeadUnitCheck
    {
        public:
            explicit AnyDeadUnitCheck(WorldObject const* fobj) : i_fobj(fobj) {}
            WorldObject const& GetFocusObject() const { return *i_fobj; }
            bool operator()(Unit* u) { return !u->IsAlive(); }
        private:
            WorldObject const* i_fobj;
    };

    class AnyStealthedCheck
    {
        public:
            explicit AnyStealthedCheck(WorldObject const* fobj) : i_fobj(fobj) {}
            WorldObject const& GetFocusObject() const { return *i_fobj; }
            bool operator()(Unit* u) { return u->GetVisibility() == VISIBILITY_GROUP_STEALTH; }
        private:
            WorldObject const* i_fobj;
    };

    // Creature checks | Creature 生物过滤谓词

    /// 检查范围内是否存在可协助战斗且在视线内的友方生物
    class AnyAssistCreatureInRangeCheck
    {
        public:
            AnyAssistCreatureInRangeCheck(Unit* funit, Unit* enemy, float range)
                : i_funit(funit), i_enemy(enemy), i_range(range)
            {
            }
            WorldObject const& GetFocusObject() const { return *i_funit; }
            bool operator()(Creature* u);

        private:
            Unit* const i_funit;
            Unit* const i_enemy;
            float i_range;
    };

    /// 查找范围内最近的可协战友方生物
    class NearestAssistCreatureInCreatureRangeCheck
    {
        public:
            NearestAssistCreatureInCreatureRangeCheck(Creature* obj, Unit* enemy, float range)
                : i_obj(obj), i_enemy(enemy), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Creature* u)
            {
                if (u == i_obj || u->IsDead() || u->IsInCombat())
                    return false;

                if (!u->CanAssist(i_obj) || !u->CanAttack(i_enemy))
                    return false;

                if (!i_obj->IsWithinDistInMap(u, i_range))
                    return false;

                if (!i_obj->IsWithinLOSInMap(u))
                    return false;

                i_range = i_obj->GetDistance(u);            // use found unit range as new range limit for next check | 将命中生物距离作为下一次检查的距离上限
                return true;
            }
            float GetLastRange() const { return i_range; }

            // prevent clone this object | 禁止拷贝此对象
            NearestAssistCreatureInCreatureRangeCheck(NearestAssistCreatureInCreatureRangeCheck const&);

        private:
            Creature* const i_obj;
            Unit* const i_enemy;
            float  i_range;
    };

    // Success at unit in range, range update for next check (this can be use with CreatureLastSearcher to find nearest creature)
    // 按 entry 与存活状态查找范围内最近的 Creature（以距离平方收缩上限，配合 CreatureLastSearcher 使用）
    class NearestCreatureEntryWithLiveStateInObjectRangeCheck
    {
        public:
            NearestCreatureEntryWithLiveStateInObjectRangeCheck(WorldObject const& obj, uint32 entry, bool onlyAlive, bool onlyDead, float range, bool excludeSelf = false, bool is3D = true)
                : i_obj(obj), i_entry(entry), i_range(range * range), i_onlyAlive(onlyAlive), i_onlyDead(onlyDead), i_excludeSelf(excludeSelf), i_is3D(is3D), i_foundOutOfRange(false) {}
            WorldObject const& GetFocusObject() const { return i_obj; }
            bool operator()(Creature* u)
            {
                if (u->GetEntry() == i_entry && ((i_onlyAlive && u->IsAlive()) || (i_onlyDead && u->IsCorpse()) || (!i_onlyAlive && !i_onlyDead)) && (!i_excludeSelf || (&i_obj != u)))
                {
                    float dist = i_obj.GetDistance(u, true, DIST_CALC_NONE);
                    if (dist < i_range)
                    {
                        i_range = dist; // use found unit range as new range limit for next check | 更新距离平方上限
                        return true;
                    }
                    i_foundOutOfRange = true;
                }
                return false;
            }
            float GetLastRange() const { return sqrt(i_range); }
        private:
            WorldObject const& i_obj;
            uint32 i_entry;
            float  i_range;
            bool   i_onlyAlive;
            bool   i_onlyDead;
            bool   i_excludeSelf;
            bool   i_is3D;
            bool   i_foundOutOfRange;

            // prevent clone this object | 禁止拷贝此对象
            NearestCreatureEntryWithLiveStateInObjectRangeCheck(NearestCreatureEntryWithLiveStateInObjectRangeCheck const&);
    };

    // Success at unit in range, range update for next check (this can be used with CreatureListSearcher to find creatures with given entry)
    // 匹配范围内符合指定 entry/DB GUID 集合与存活状态的全部生物
    class AllCreatureEntriesWithLiveStateInObjectRangeCheck
    {
        public:
            AllCreatureEntriesWithLiveStateInObjectRangeCheck(WorldObject const& obj, std::set<uint32>& entries, bool alive, float range, bool guids = false, bool excludeSelf = false, bool is3D = true)
                : i_obj(obj), i_entries(entries), i_range(range), i_guids(guids), i_alive(alive), i_excludeSelf(excludeSelf), i_is3D(is3D), i_foundOutOfRange(false) {}
            WorldObject const& GetFocusObject() const { return i_obj; }
            bool operator()(Creature* u)
            {
                if (i_entries.find(i_guids ? u->GetDbGuid() : u->GetEntry()) != i_entries.end() && ((i_alive && u->IsAlive()) || (!i_alive && u->IsCorpse())) && (!i_excludeSelf || (&i_obj != u)))
                {
                    if (i_obj.IsWithinCombatDistInMap(u, i_range, i_is3D))
                        return true;
                    i_foundOutOfRange = true;
                }
                return false;
            }
            bool FoundOutOfRange() const { return i_foundOutOfRange; }
        private:
            WorldObject const& i_obj;
            std::set<uint32>& i_entries;
            float  i_range;
            bool   i_guids;
            bool   i_alive;
            bool   i_excludeSelf;
            bool   i_is3D;
            bool   i_foundOutOfRange;

            // prevent clone this object
            AllCreatureEntriesWithLiveStateInObjectRangeCheck(AllCreatureEntriesWithLiveStateInObjectRangeCheck const&);
    };

    class AllCreaturesOfEntryInRangeCheck
    {
        public:
            AllCreaturesOfEntryInRangeCheck(const WorldObject* pObject, uint32 uiEntry, float fMaxRange) : m_pObject(pObject), m_uiEntry(uiEntry), m_fRange(fMaxRange) {}
            WorldObject const& GetFocusObject() const { return *m_pObject; }
            bool operator()(Unit* pUnit)
            {
                return pUnit->GetEntry() == m_uiEntry && m_pObject->IsWithinDist(pUnit, m_fRange, false);
            }

            // prevent clone this object
            AllCreaturesOfEntryInRangeCheck(AllCreaturesOfEntryInRangeCheck const&);

        private:
            const WorldObject* m_pObject;
            uint32 m_uiEntry;
            float m_fRange;
    };

    class AllCreaturesMatchingOneEntryInRange
    {
        public:
            AllCreaturesMatchingOneEntryInRange(WorldObject const* pObject, std::vector<uint32> const& entries, float fMaxRange)
                : m_pObject(pObject), entries(entries), m_fRange(fMaxRange) {}
            bool operator() (Unit* pUnit)
            {
                for (const auto entry : entries) {
                    if (pUnit->GetEntry() == entry && m_pObject->IsWithinDist(pUnit, m_fRange, false)) {
                        return true;
                    }
                }
                return false;
            }

        private:
            WorldObject const* m_pObject;
            std::vector<uint32> entries;
            float m_fRange;
    };

    // Player checks and do | 玩家过滤谓词与本地化数据包缓存投递器

    /// 匹配范围内所有存活的玩家
    class AnyPlayerInObjectRangeCheck
    {
        public:
            AnyPlayerInObjectRangeCheck(WorldObject const* obj, float range) : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Player* u)
            {
                return u->IsAlive() && i_obj->IsWithinDistInMap(u, i_range);
            }
        private:
            WorldObject const* i_obj;
            float i_range;
    };

    /// 匹配范围内拥有指定光环的存活玩家
    class AnyPlayerInObjectRangeWithAuraCheck
    {
        public:
            AnyPlayerInObjectRangeWithAuraCheck(WorldObject const* obj, float range, uint32 spellId)
                : i_obj(obj), i_range(range), i_spellId(spellId) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Player* u)
            {
                return u->IsAlive()
                       && i_obj->IsWithinDistInMap(u, i_range)
                       && u->HasAura(i_spellId);
            }
        private:
            WorldObject const* i_obj;
            float i_range;
            uint32 i_spellId;
    };

    /// 匹配范围内可参与占据点（Capture Point）控制的玩家
    class AnyPlayerInCapturePointRange
    {
        public:
            AnyPlayerInCapturePointRange(WorldObject const* obj, float range)
                : i_obj(obj), i_range(range) {}
            WorldObject const& GetFocusObject() const { return *i_obj; }
            bool operator()(Player* u)
            {
                return u->CanUseCapturePoint() &&
                       i_obj->IsWithinDistInMap(u, i_range);
            }
        private:
            WorldObject const* i_obj;
            float i_range;
    };

    // Prepare using Builder localized packets with caching and send to player
    // 使用 Builder 按客户端语言区域生成并缓存单条本地化数据包后发送给玩家
    template<class Builder>
    class LocalizedPacketDo
    {
        public:
            explicit LocalizedPacketDo(Builder& builder) : i_builder(builder) {}

            void operator()(Player* p);

        private:
            Builder& i_builder;
            std::vector<std::unique_ptr<WorldPacket>> i_data_cache;         // 0 = default, i => i-1 locale index | 0 为默认语言，下标 i 对应 locale_idx = i-1
    };

    // Prepare using Builder localized packets with caching and send to player
    // 使用 Builder 按客户端语言区域生成并缓存多条本地化数据包列表后批量发送给玩家
    template<class Builder>
    class LocalizedPacketListDo
    {
        public:
            typedef std::vector<std::unique_ptr<WorldPacket>> WorldPacketList;
            explicit LocalizedPacketListDo(Builder& builder) : i_builder(builder) {}

            void operator()(Player* p);

        private:
            Builder& i_builder;
            std::vector<WorldPacketList> i_data_cache;
            // 0 = default, i => i-1 locale index | 0 为默认语言，下标 i 对应 locale_idx = i-1
    };

#ifndef _MSC_VER
    template<> void PlayerVisitObjectsNotifier::Visit<Player>(PlayerMapType&);
    template<> void PlayerVisitObjectsNotifier::Visit<Creature>(CreatureMapType&);
    template<> void CreatureVisitObjectsNotifier::Visit<Player>(PlayerMapType&);
    template<> void CreatureVisitObjectsNotifier::Visit<Creature>(CreatureMapType&);
    template<> inline void DynamicObjectUpdater::Visit<Creature>(CreatureMapType&);
    template<> inline void DynamicObjectUpdater::Visit<Player>(PlayerMapType&);
#endif
}
#endif
