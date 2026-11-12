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

#ifndef MANGOSSERVER_DYNAMICOBJECT_H
#define MANGOSSERVER_DYNAMICOBJECT_H

#include "Entities/Object.h"
#include "Server/DBCEnums.h"
#include "Spells/SpellTargetDefines.h"
#include "Entities/Unit.h"

// 动态对象类型枚举
enum DynamicObjectType
{
    DYNAMIC_OBJECT_PORTAL           = 0x0,      // unused (未使用：传送门)
    DYNAMIC_OBJECT_AREA_SPELL       = 0x1,      // 持续性区域法术效果（如暴风雪、奉献、烈焰风暴）
    DYNAMIC_OBJECT_FARSIGHT_FOCUS   = 0x2,      // 远视焦点（如鹰眼术、心灵视界）
};

struct SpellEntry;
struct AuraScript;

// 动态世界对象类（表示法术生成的地面持续效果区域或远视观察焦点）
class DynamicObject : public WorldObject
{
    public:
        explicit DynamicObject();

        // 注册/注销于地图对象存储并加入/移出世界
        void AddToWorld() override;
        void RemoveFromWorld() override;

        // 初始化动态对象（绑定施法者、法术、效果索引、位置、持续时间与作用半径）
        bool Create(uint32 guidlow, Unit* caster, uint32 spellId, SpellEffectIndex effIndex, float x, float y, float z, int32 duration, float radius, DynamicObjectType type, SpellTarget target, int32 damage, int32 basePoints);
        // 心跳更新：检查存活时间、施法者状态并周期性刷新区域光环目标
        void Update(const uint32 diff) override;
        // 播放消失动画并加入地图移除队列
        void Delete();
        uint32 GetSpellId() const { return m_spellId; }
        SpellEffectIndex GetEffIndex() const { return m_effIndex; }
        uint32 GetDuration() const { return std::max(int64((m_aliveTime - GetMap()->GetCurrentClockTime()).count()), int64(0)); }
        ObjectGuid const& GetOwnerGuid() const override { return GetCasterGuid(); }
        void SetOwnerGuid(ObjectGuid guid) override { SetCasterGuid(guid); }
        ObjectGuid const& GetCasterGuid() const { return GetGuidValue(DYNAMICOBJECT_CASTER); }
        void SetCasterGuid(ObjectGuid guid) { SetGuidValue(DYNAMICOBJECT_CASTER, guid); }
        Unit* GetCaster() const;
        float GetRadius() const { return m_radius; }
        int32 const& GetDamage() const { return m_damage; }
        int32 const& GetBasePoints() const { return m_basePoints; }
        DynamicObjectType GetType() const { return (DynamicObjectType)GetByteValue(DYNAMICOBJECT_BYTES, 0); }
        // 管理受该动态对象区域光环影响的目标 GUID 集合
        bool IsAffecting(Unit* unit) const { return m_affected.find(unit->GetObjectGuid()) != m_affected.end(); }
        void AddAffected(Unit* unit) { m_affected.insert(unit->GetObjectGuid()); }
        void RemoveAffected(Unit* unit) { m_affected.erase(unit->GetObjectGuid()); }
        // 施法者受击退条时同步缩减动态对象及其区域光环的剩余时间
        void Delay(int32 delaytime);

        // 阵营敌友与法术攻击/辅助判定（委托或基于施法者关系）
        ReputationRank GetReactionTo(Unit const* unit) const override;

        bool IsEnemy(Unit const* unit) const override;
        bool IsFriend(Unit const* unit) const override;

        bool CanAttackSpell(Unit const* target, SpellEntry const* spellInfo = nullptr, bool isAOE = false) const override;
        bool CanAssistSpell(Unit const* target, SpellEntry const* spellInfo = nullptr) const override;

        // 持久区域光环结束与目标校验脚本回调
        void OnPersistentAreaAuraEnd();
        bool OnPersistentAreaAuraCheckTarget(Unit* target);

        float GetObjectBoundingRadius() const override { return 0.f; } // dynamic object not have real interact size (动态对象没有实际交互碰撞半径)
        float GetCollisionHeight() const override { return 1.f; } // to get away with ground collision (抬高碰撞高度以避开地面碰撞检测)

        bool isVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const override;
        SpellTarget GetTarget() const { return m_target; }

        GridReference<DynamicObject>& GetGridRef() { return m_gridRef; }

    protected:
        uint32 m_spellId;                                   // 关联法术 ID
        SpellEffectIndex m_effIndex;                        // 对应的法术效果槽位索引
        TimePoint m_aliveTime;                              // 到期销毁时间戳
        float m_radius;                                     // radius apply persistent effect, 0 = no persistent effect (持久区域效果作用半径，0 表示无持久区域效果)
        bool m_positive;                                    // 是否为正面（增益/治疗）区域效果
        GuidSet m_affected;                                 // 当前受影响目标的 GUID 集合
        SpellTarget m_target;                               // 施法目标类型信息
        int32 m_damage;                                     // 计算后的效果伤害/数值
        int32 m_basePoints;                                 // 原始基础点数

        AuraScript* m_auraScript;                           // 关联的光环脚本指针
    private:
        GridReference<DynamicObject> m_gridRef;             // 地图网格引用节点
};
#endif
