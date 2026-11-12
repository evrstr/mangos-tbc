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

#ifndef _THREATMANAGER
#define _THREATMANAGER

#include "Common.h"
#include "Globals/SharedDefines.h"
#include "Utilities/LinkedReference/Reference.h"
#include "Entities/UnitEvents.h"
#include "Entities/ObjectGuid.h"
#include <list>

//==============================================================

class Unit;
class ThreatManager;
struct SpellEntry;

//==============================================================
// Class to calculate the real threat based (仇恨值计算辅助类：应用天赋、光环、暴击及法术修正)

class ThreatCalcHelper
{
    public:
        static float CalcThreat(Unit* hatedUnit, Unit* hatingUnit, float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* threatSpell, bool assist);
};

// 敌对目标仇恨优先级压制状态
enum HostileState : uint32
{
    STATE_SUPPRESSED,                                       // 被压制（如免疫/不可攻击，优先级降为最低）
    STATE_NORMAL,                                           // 正常敌对优先级
};

// 嘲讽/锁定状态优先级（数值越大越优先被选为攻击目标）
enum TauntState : uint32
{
    STATE_DETAUNTED,                                        // 被反嘲讽降低目标权重
    STATE_NONE,                                             // 无嘲讽状态
    STATE_TAUNTED,                                          // 被嘲讽（按施法顺序递增）
    STATE_FIXATED = UINT32_MAX,                             // 被强制锁定目标（最高优先级）
};

//==============================================================
// 单个敌对仇恨引用：关联仇恨管理源（ThreatManager）与被仇恨目标（Unit），保存仇恨数值及状态
class HostileReference : public Reference<Unit, ThreatManager>
{
    public:
        HostileReference(Unit* unit, ThreatManager* threatManager, float threat);

        //=================================================
        void addThreat(float mod);                          // 增减仇恨值并触发变更事件

        void setThreat(float threat) { addThreat(threat - getThreat()); }

        void addThreatPercent(int32 threatPercent)          // 按百分比修改仇恨（-100 时直接归零避免浮点误差）
        {
            // for special -100 case avoid rounding
            addThreat(threatPercent == -100 ? -iThreat : iThreat * threatPercent / 100.0f);
        }

        float getThreat() const { return iThreat; }
        float getFadeoutThreatReduction() const { return iFadeoutThreadReduction; }
        void setFadeoutThreatReduction(float value);        // 应用临时消退仇恨扣减量
        void resetFadeoutThreatReduction();                 // 恢复临时消退扣减的仇恨

        bool isOnline() const { return m_online; }          // 是否处于在线（活跃可攻击）仇恨列表

        // The Unit might be in water and the creature can not enter the water, but has range attack
        // in this case online = true, but accessable = false
        // 单位可能处于水中而生物无法入水但具备远程攻击，此时 online=true 但 accessible=false
        bool isAccessable() const { return iAccessible; }

        //=================================================
        // check, if source can reach target and set the status (检查来源与目标状态并更新在线标记)
        void updateOnlineStatus();

        void setOnlineOfflineState(bool isOnline);

        void setAccessibleState(bool isAccessible);
        //=================================================

        bool operator ==(const HostileReference& hostileReference) const { return hostileReference.getUnitGuid() == getUnitGuid(); }

        //=================================================

        ObjectGuid const& getUnitGuid() const { return iUnitGuid; }

        //=================================================
        // reference is not needed anymore. realy delete it ! (销毁引用并通知 ThreatManager 移除)

        void removeReference();

        //=================================================

        HostileReference* next() { return static_cast<HostileReference*>(Reference<Unit, ThreatManager>::next()); }

        //=================================================

        // Tell our refTo (target) object that we have a link (通知目标单位建立反向仇恨链接)
        void targetObjectBuildLink() override;

        // Tell our refTo (taget) object, that the link is cut (通知目标单位断开反向仇恨链接)
        void targetObjectDestroyLink() override;

        // Tell our refFrom (source) object, that the link is cut (Target destroyed) (目标销毁时通知来源置于离线状态)
        void sourceObjectDestroyLink() override;

        // Priority alterations (目标优先级与嘲讽状态控制)
        void SetSuppressabilityToggle() { m_suppresabilityToggle = true; }
        void SetHostileState(HostileState state);
        HostileState GetHostileState() const { return m_hostileState; }

        void SetTauntState(TauntState state) { m_tauntState = state; }
        TauntState GetTauntState() const { return m_tauntState; }
    protected:
        // Inform the source, that the status of that reference was changed (向所属 ThreatManager 派发状态变更事件)
        void fireStatusChanged(ThreatRefStatusChangeEvent& threatRefStatusChangeEvent);

        Unit* getSourceUnit() const;
    private:
        float iThreat;                                      // 当前累计仇恨值
        HostileState m_hostileState;                        // 压制状态
        bool m_suppresabilityToggle;                        // 仇恨变化时是否自动解除压制的标记
        TauntState m_tauntState;                            // 嘲讽/锁定状态
        float iFadeoutThreadReduction;                      // used for fade (渐隐类法术临时扣减的仇恨值)
        ObjectGuid iUnitGuid;                               // 目标单位 GUID
        bool m_online;                                      // 在线列表标志
        bool iAccessible;                                   // 物理可达性标志
};

//==============================================================
class ThreatManager;

typedef std::list<HostileReference*> ThreatList;


// 仇恨容器：维护一组 HostileReference 列表，负责排序与目标选择（110% 近战 / 130% 远程 OT 规则）
class ThreatContainer
{
    public:
        ThreatContainer() { iDirty = false; }
        ~ThreatContainer() { clearReferences(); }

        HostileReference* addThreat(Unit* victim, float threat);

        void modifyThreatPercent(Unit* victim, int32 threatPercent);
        void modifyAllThreatPercent(int32 threatPercent);

        // 根据当前目标与 OT 阈值规则（近战 110%、远程 130%、嘲讽优先、定身忽略远程）选择下一个目标
        HostileReference* selectNextVictim(Unit* attacker, HostileReference* currentVictim);

        void setDirty(bool dirty) { iDirty = dirty; }

        bool isDirty() const { return iDirty; }

        bool empty() const { return iThreatList.empty(); }

        HostileReference* getMostHated() { return iThreatList.empty() ? nullptr : iThreatList.front(); }

        HostileReference* getReferenceByTarget(Unit* victim);

        ThreatList const& getThreatList() const { return iThreatList; }
    protected:
        friend class ThreatManager;

        void remove(HostileReference* ref) { iThreatList.remove(ref); }
        void addReference(HostileReference* hostileReference) { iThreatList.push_back(hostileReference); }
        void clearReferences();
        // Sort the list if necessary (按需要重新排序仇恨列表)
        void update(bool force, bool isPlayer);

        ThreatList iThreatList;
    private:
        bool iDirty;                                        // 列表是否需要重排序的脏标记
};

//=================================================

// 单位仇恨管理器：维护在线与离线两个仇恨容器，处理仇恨增减、误导、嘲讽/锁定及最高目标选择
class ThreatManager
{
    public:
        friend class HostileReference;

        explicit ThreatManager(Unit* owner);

        ~ThreatManager() { clearReferences(); }

        void clearReferences();                             // 清空在线与离线仇恨列表并置空当前目标

        // 添加经过修正及误导计算后的仇恨值
        void addThreat(Unit* victim, float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* threatSpell, bool assist);
        void addThreat(Unit* victim, float threat) { addThreat(victim, threat, false, SPELL_SCHOOL_MASK_NONE, nullptr, false); }

        // add threat as raw value (ignore redirections and expection all mods applied already to it
        // 直接写入已计算好的原始仇恨值（不再经过误导或公式修正）
        void addThreatDirectly(Unit* victim, float threat, bool noNew);

        void modifyThreatPercent(Unit* victim, int32 threatPercent); // -101 removes whole ref, -100 sets threat to 0, rest modifies it (-101 删除引用，-100 归零，其余按百分比调整)
        void modifyAllThreatPercent(int32 threatPercent);

        float getThreat(Unit* victim, bool alsoSearchOfflineList = false);
        float GetHighestThreat(); // for purpose of taunt effect (获取全局最高仇恨值，供嘲讽拉齐仇恨使用)

        bool HasThreat(Unit* victim, bool alsoSearchOfflineList = false);

        bool isThreatListEmpty() const { return iThreatContainer.empty(); }

        void processThreatEvent(ThreatRefStatusChangeEvent& threatRefStatusChangeEvent); // 响应仇恨引用状态变化事件

        HostileReference* getCurrentVictim() const { return iCurrentVictim; }

        Unit* getOwner() const { return iOwner; }

        Unit* getHostileTarget();                           // 排序容器并获取当前最合适攻击的敌对目标

        void TauntUpdate();                                 // 依据自身嘲讽/反嘲讽光环刷新所有目标的 TauntState
        void FixateTarget(Unit* victim);                    // 将指定目标标记为强制锁定 (STATE_FIXATED)

        void setCurrentVictim(HostileReference* hostileReference);
        void setCurrentVictimByTarget(Unit* target); // Used in SPELL_EFFECT_ATTACK_ME to set the current target to the taunter (直接将指定目标设为当前仇恨目标)

        void setDirty(bool dirty) { iThreatContainer.setDirty(dirty); }

        // Don't must be used for explicit modify threat values in iterator return pointers (仅用于只读遍历，禁止通过返回指针直接篡改仇恨值)
        ThreatList const& getThreatList() const { return iThreatContainer.getThreatList(); }

        void DeleteOutOfRangeReferences();                  // 清理超过 60 码战斗距离的仇恨引用

        // When a target is unreachable, we need to set someone as low priority (目标不可达或免疫时压制其优先级)
        void SetTargetSuppressed(Unit* target);
        void ClearSuppressed(HostileReference* except);
    private:
        void UpdateContainers();

        HostileReference* iCurrentVictim;                   // 当前仇恨锁定目标引用
        Unit* iOwner;                                       // 仇恨列表拥有者
        ThreatContainer iThreatContainer;                   // 在线活跃仇恨容器
        ThreatContainer iThreatOfflineContainer;            // 离线/不可攻击（假死、GM、飞行等）仇恨容器
};

//=================================================
#endif
