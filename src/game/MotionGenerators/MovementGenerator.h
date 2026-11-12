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

#ifndef MANGOS_MOVEMENTGENERATOR_H
#define MANGOS_MOVEMENTGENERATOR_H

#include "Platform/Define.h"
#include "Dynamic/FactoryHolder.h"
#include "MotionGenerators/MotionMaster.h"
#include "Util/Timer.h"
#include "Globals/SharedDefines.h"

class Unit;
class Creature;
class Player;

/// 移动生成器抽象基类：由 MotionMaster 以栈式结构调度，控制单位的各类移动行为
class MovementGenerator
{
    public:
        virtual ~MovementGenerator();

        // called before adding movement generator to motion stack (压入运动栈激活前调用)
        virtual void Initialize(Unit&) = 0;
        // called aftre remove movement generator from motion stack (从运动栈弹出移除后调用)
        virtual void Finalize(Unit&) = 0;

        // called before lost top position (before push new movement generator above) (被更高优先级生成器压栈覆盖前调用)
        virtual void Interrupt(Unit&) = 0;
        // called after return movement generator to top position (after remove above movement generator) (上层生成器移除后重新回到栈顶时调用)
        virtual void Reset(Unit&) = 0;

        /// 每帧驱动移动逻辑，返回 false 表示移动已完成并可从栈顶过期移除
        virtual bool Update(Unit&, const uint32& time_diff) = 0;

        virtual MovementGeneratorType GetMovementGeneratorType() const = 0;
        virtual Unit* GetCurrentTarget() const { return nullptr; }

        /// 单位移动速度变化时的通知回调
        virtual void UnitSpeedChanged() { }

        // used by Evade code for select point to evade with expected restart default movement
        // 供脱战返巢逻辑查询复位坐标（例如巡逻中途接战后的返回点）
        virtual bool GetResetPosition(Unit&, Position& pos) const { return false; }
        virtual void SetResetPosition(Unit&, Position const& pos) { }

        // given destination unreachable? due to pathfinsing or other
        // 目标点是否可达（寻路或地形不可达时返回 false）
        virtual bool IsReachable() const { return true; }

        /// 过期后是否自动移除（默认 false）
        virtual bool IsRemovedOnExpire() const { return false; }

        // used for check from Update call is movegen still be active (top movement generator)
        // after some not safe for this calls
        // 检查当前生成器是否仍处于 MotionMaster 栈顶（防止 Update 内部触发重入清栈后继续操作）
        bool IsActive(Unit& u);
};

/// CRTP 中间模板基类：将 Unit& 接口静态派发给子类强类型实现（如 Creature&）
template<class T, class D>
class MovementGeneratorMedium : public MovementGenerator
{
    public:
        void Initialize(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Initialize(*((T*)&u));
        }
        void Finalize(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Finalize(*((T*)&u));
        }
        void Interrupt(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Interrupt(*((T*)&u));
        }
        void Reset(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Reset(*((T*)&u));
        }
        bool Update(Unit& u, const uint32& time_diff) override
        {
            // u->AssertIsType<T>();
            return (static_cast<D*>(this))->Update(*((T*)&u), time_diff);
        }
        bool GetResetPosition(Unit& u, Position& pos) const override
        {
            // u->AssertIsType<T>();
            return (static_cast<D const*>(this))->GetResetPosition(*((T*)&u), pos);
        }
        void SetResetPosition(Unit& u, Position const& pos) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->SetResetPosition(*((T*)&u), pos);
        }
    public:
        // Will not link if not overridden in the generators - also not generate for T==Unit
        template <class U = T, typename std::enable_if<false == std::is_same<U, Unit>::value, U>::type* = nullptr>
        void Initialize(U& u);
        template <class U = T, typename std::enable_if<false == std::is_same<U, Unit>::value, U>::type* = nullptr>
        void Finalize(U& u);
        template <class U = T, typename std::enable_if<false == std::is_same<U, Unit>::value, U>::type* = nullptr>
        void Interrupt(U& u);
        template <class U = T, typename std::enable_if<false == std::is_same<U, Unit>::value, U>::type* = nullptr>
        void Reset(U& u);
        template <class U = T, typename std::enable_if<false == std::is_same<U, Unit>::value, U>::type* = nullptr>
        bool Update(U& u, const uint32& time_diff);

        // not need always overwrites
        template <class U = T, typename std::enable_if<false == std::is_same<U, Unit>::value, U>::type* = nullptr>
        bool GetResetPosition(U& /*u*/, Position& /*pos*/) const { return false; }
};

struct SelectableMovement : public FactoryHolder<MovementGenerator, MovementGeneratorType>
{
    SelectableMovement(MovementGeneratorType mgt) : FactoryHolder<MovementGenerator, MovementGeneratorType>(mgt) {}
};

template<class REAL_MOVEMENT>
struct MovementGeneratorFactory : public SelectableMovement
{
    MovementGeneratorFactory(MovementGeneratorType mgt) : SelectableMovement(mgt) {}

    MovementGenerator* Create(void*) const override;
};

typedef FactoryHolder<MovementGenerator, MovementGeneratorType> MovementGeneratorCreator;
typedef FactoryHolder<MovementGenerator, MovementGeneratorType>::FactoryHolderRegistry MovementGeneratorRegistry;
typedef FactoryHolder<MovementGenerator, MovementGeneratorType>::FactoryHolderRepository MovementGeneratorRepository;

#endif
