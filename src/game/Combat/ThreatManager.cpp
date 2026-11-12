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

#include "Combat/ThreatManager.h"
#include "Entities/Unit.h"
#include "Entities/Creature.h"
#include "AI/BaseAI/CreatureAI.h"
#include "Maps/Map.h"
#include "Entities/Player.h"
#include "Globals/ObjectAccessor.h"
#include "Entities/UnitEvents.h"
#include "Spells/SpellAuras.h"

//==============================================================
//================= ThreatCalcHelper ===========================
//==============================================================

// The pHatingUnit is not used yet (根据法术、暴击、仇恨修饰光环计算最终仇恨值)
float ThreatCalcHelper::CalcThreat(Unit* hatedUnit, Unit* hatingUnit, float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* threatSpell, bool assist)
{
    // all flat mods applied early (基础仇恨为 0 时直接返回)
    if (!threat)
        return 0.f;

    if (hatedUnit->GetNoThreatState()) // some NPCs cause no threat (某些特殊 NPC 不会产生仇恨)
        return 0.f;

    if (hatingUnit->GetTypeId() == TYPEID_PLAYER) // players have entries with 0 threat during charm (被魅惑玩家列表中的仇恨项归零)
        return 0.f;

    if (!assist && hatedUnit->IsSupportThreatOnly())
        return 0.f;

    if (threatSpell)
    {
        if (Player* modOwner = hatedUnit->GetSpellModOwner())
            modOwner->ApplySpellMod(threatSpell->Id, SPELLMOD_THREAT, threat);

        if (crit)
            threat *= hatedUnit->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_CRITICAL_THREAT, schoolMask);
    }

    threat = hatedUnit->ApplyTotalThreatModifier(threat, schoolMask);
    return threat;
}

//============================================================
//================= HostileReference ==========================
//============================================================

// 构造仇恨引用并建立与目标单位的双向链接
HostileReference::HostileReference(Unit* unit, ThreatManager* threatManager, float threat) : 
    m_hostileState(STATE_NORMAL), m_tauntState(STATE_NONE)
{
    iThreat = threat;
    iFadeoutThreadReduction = 0.f;
    link(unit, threatManager);
    iUnitGuid = unit->GetObjectGuid();
    m_online = true;
    m_suppresabilityToggle = false;
    iAccessible = true;
}

//============================================================
// Tell our refTo (target) object that we have a link (将本引用登记到目标单位的受仇恨列表)
void HostileReference::targetObjectBuildLink()
{
    getTarget()->addHatedBy(this);
}

//============================================================
// Tell our refTo (taget) object, that the link is cut (从目标单位的受仇恨列表移除)
void HostileReference::targetObjectDestroyLink()
{
    getTarget()->removeHatedBy(this);
}

//============================================================
// Tell our refFrom (source) object, that the link is cut (Target destroyed) (目标销毁时通知来源将其置为离线)

void HostileReference::sourceObjectDestroyLink()
{
    setOnlineOfflineState(false);
}

//============================================================
// Inform the source, that the status of the reference changed (通知 ThreatManager 处理仇恨状态变更事件)

void HostileReference::fireStatusChanged(ThreatRefStatusChangeEvent& threatRefStatusChangeEvent)
{
    if (getSource())
        getSource()->processThreatEvent(threatRefStatusChangeEvent);
}

//============================================================
// 增减仇恨值（保证不低于 0），必要时解除压制标记并触发变更事件
void HostileReference::addThreat(float mod)
{
    if (mod + iThreat < 0)
        mod = -iThreat;

    iThreat += mod;
    // If threat has changed, clear suppress (仇恨发生变化时清除临时压制状态)
    if (m_suppresabilityToggle && mod != 0.f)
        SetHostileState(STATE_NORMAL);

    if (mod != 0.0f)
    {
        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_THREAT_CHANGE, this, mod);
        fireStatusChanged(event);
    }
}

//============================================================
// check, if source can reach target and set the status (检查目标是否仍有效且非离线状态)

void HostileReference::updateOnlineStatus()
{
    bool online = false;
    if (!isValid())
    {
        if (Unit* target = ObjectAccessor::GetUnit(*getSourceUnit(), getUnitGuid()))
            link(target, getSource());
    }
    // only check for online status if
    // ref is valid (仅在引用有效时判断离线条件)
    if (isValid())
    {
        Unit* unit = getSourceUnit();
        Unit* target = getTarget();
        if (!unit->IsOfflineTarget(target))
            online = true;
    }
    setOnlineOfflineState(online);
}

//============================================================
// set the status and fire the event on status change (更新在线状态并在变化时派发事件)

void HostileReference::setOnlineOfflineState(bool isOnline)
{
    if (m_online != isOnline)
    {
        m_online = isOnline;

        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_ONLINE_STATUS, this);
        fireStatusChanged(event);
    }
}

//============================================================
// 更新物理可达性状态并派发事件
void HostileReference::setAccessibleState(bool isAccessible)
{
    if (iAccessible != isAccessible)
    {
        iAccessible = isAccessible;

        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_ASSECCIBLE_STATUS, this);
        fireStatusChanged(event);
    }
}

// 设置压制状态并通知容器重排序
void HostileReference::SetHostileState(HostileState state)
{
    if (m_hostileState != state)
    {
        m_hostileState = state;

        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_SUPPRESSED_STATUS, this);
        fireStatusChanged(event);
    }
    m_suppresabilityToggle = false;
}

//============================================================
// prepare the reference for deleting
// this is called be the target (断开引用并通知 ThreatManager 将其从列表移除)

void HostileReference::removeReference()
{
    invalidate();

    ThreatRefStatusChangeEvent event(UEV_THREAT_REF_REMOVE_FROM_LIST, this);
    fireStatusChanged(event);
}

//============================================================
// 获取拥有此仇恨管理器的单位
Unit* HostileReference::getSourceUnit() const
{
    return getSource()->getOwner();
}

//============================================================
//================ ThreatContainer ===========================
//============================================================

// 断开并释放容器内的所有仇恨引用
void ThreatContainer::clearReferences()
{
    for (ThreatList::const_iterator i = iThreatList.begin(); i != iThreatList.end(); ++i)
    {
        (*i)->unlink();
        delete (*i);
    }
    iThreatList.clear();
}

//============================================================
// Return the HostileReference of nullptr, if not found (按目标 GUID 查找仇恨引用)

HostileReference* ThreatContainer::getReferenceByTarget(Unit* victim)
{
    if (!victim)
        return nullptr;

    ObjectGuid guid = victim->GetObjectGuid();

    for (ThreatList::const_iterator i = iThreatList.begin(); i != iThreatList.end(); ++i)
        if ((*i)->getUnitGuid() == guid)
            return (*i);

    return nullptr;
}

//============================================================
// Add the threat, if we find the reference (若存在目标引用则叠加仇恨值)

HostileReference* ThreatContainer::addThreat(Unit* victim, float threat)
{
    HostileReference* ref = getReferenceByTarget(victim);
    if (ref)
        ref->addThreat(threat);
    return ref;
}

//============================================================
// 按百分比修改指定目标的仇恨（<-100 时直接移除引用）
void ThreatContainer::modifyThreatPercent(Unit* victim, int32 threatPercent)
{
    if (HostileReference* ref = getReferenceByTarget(victim))
    {
        if (threatPercent < -100)
        {
            ref->removeReference();
            delete ref;
        }
        else
            ref->addThreatPercent(threatPercent);
    }
}

//============================================================
// Modify all threat by provided percentage (按百分比修改容器内所有目标的仇恨)

void ThreatContainer::modifyAllThreatPercent(int32 threatPercent)
{
    if (threatPercent < -100)
    {
        while (!iThreatList.empty())
        {
            HostileReference* ref = *iThreatList.begin();
            ref->removeReference();
            delete ref;
        }
    }
    else
    {
        for (auto itr : iThreatList)
            itr->addThreatPercent(threatPercent);
    }
}
//============================================================
// Check if the list is dirty and sort if necessary
// 当列表脏标记置位或处于特殊过滤模式时，按优先级（玩家>可攻击性>嘲讽>近战可达>压制状态>仇恨降序）排序

void ThreatContainer::update(bool force, bool isPlayer)
{
    if ((iDirty || force || isPlayer) && iThreatList.size() > 1)
    {
        iThreatList.sort([&](const HostileReference* lhs, const HostileReference* rhs)->bool
        {
            Unit* owner = lhs->getSource()->getOwner();
            if (isPlayer)
            {
                Unit* left = lhs->getTarget();
                Unit* right = rhs->getTarget();
                if (left->IsPlayer() && !right->IsPlayer())
                    return true;
                if (!left->IsPlayer() && right->IsPlayer())
                    return false;
                bool attackLeft = owner->CanAttack(left);
                bool attackRight = owner->CanAttack(right);
                if (attackLeft && !attackRight)
                    return true;
                if (!attackLeft && attackRight)
                    return false;
            }
            if (lhs->GetTauntState() != rhs->GetTauntState())
                return lhs->GetTauntState() > rhs->GetTauntState();
            if (force)
            {
                bool first = owner->CanReachWithMeleeAttack(lhs->getTarget());
                bool second = owner->CanReachWithMeleeAttack(rhs->getTarget());
                if (first != second)
                    return first;
            }
            if (lhs->GetHostileState() != rhs->GetHostileState())
                return lhs->GetHostileState() > rhs->GetHostileState();
            return lhs->getThreat() > rhs->getThreat(); // reverse sorting (按仇恨值从高到低降序排列)
        });
    }
    iDirty = false;
}

//============================================================
// return the next best victim
// could be the current victim (按 OT 规则与当前状态选出下一个最佳攻击目标)

HostileReference* ThreatContainer::selectNextVictim(Unit* attacker, HostileReference* currentVictim)
{
    HostileReference* currentRef = nullptr;
    bool found = false;
    bool suppressRanged = attacker->IsIgnoringRangedTargets();
    bool currentVictimInMelee = true;
    if (suppressRanged && currentVictim)
        currentVictimInMelee = attacker->CanReachWithMeleeAttack(currentVictim->getTarget());

    ThreatList::const_iterator lastRef = iThreatList.end();
    --lastRef;

    for (ThreatList::const_iterator iter = iThreatList.begin(); iter != iThreatList.end() && !found;)
    {
        currentRef = (*iter);

        Unit* target = currentRef->getTarget();
        MANGOS_ASSERT(target); // if the ref has status online the target must be there! (在线引用的目标指针必须非空)

        bool isInMelee = attacker->CanReachWithMeleeAttack(target);
        if (currentVictim) // select 1.3/1.1 better target in comparison current target (与当前目标对比，应用 110% 近战 / 130% 远程 OT 阈值)
        {
            // normal case: pCurrentRef is still valid and most hated (常规情况：当前目标仍是最高仇恨且有效)
            if (currentVictim == currentRef)
            {
                if (suppressRanged && !currentVictimInMelee)
                {
                    ++iter;
                    continue;
                }
                found = true;
                break;
            }

            if (currentRef->GetTauntState() > currentVictim->GetTauntState()) // taunt overrides root skipping (嘲讽优先级高于定身忽略远程规则)
            {
                found = true;
                break;
            }

            if (currentRef->getTarget()->IsPlayer() && !currentVictim->getTarget()->IsPlayer())
            {
                found = true;
                break;
            }

            if (suppressRanged) // suppress ranged when rooted (定身时忽略无法近战命中的远程目标)
            {
                if (!isInMelee) // if current ref is not in melee - skip it (非近战范围则跳过)
                {
                    ++iter;
                    continue;
                }
                else if (!currentVictimInMelee)
                {
                    found = true;
                    break;
                }
            }

            if (currentRef->GetHostileState() > currentVictim->GetHostileState())
            {
                found = true;
                break;
            }

            // list sorted and and we check current target, then this is best case (已排好序且未超过 110% 阈值，保持当前目标)
            if (currentRef->getThreat() <= 1.1f * currentVictim->getThreat())
            {
                currentRef = currentVictim;
                found = true;
                break;
            }

            if (currentRef->getThreat() > 1.3f * currentVictim->getThreat() ||
                (currentRef->getThreat() > 1.1f * currentVictim->getThreat() && isInMelee))
            {
                // implement 110% threat rule for targets in melee range (近战范围目标需超过当前目标 110% 仇恨才会切换)
                found = true;                           // and 130% rule for targets in ranged distances (远程范围目标需超过当前目标 130% 仇恨才会切换)
                break;                                  // for selecting alive targets
            }
        }
        else if (!suppressRanged || isInMelee) // select any (无当前目标时选取首个合法近战/任意目标)
        {
            found = true;
            break;
        }
        ++iter;
    }
    if (!found)
        currentRef = nullptr;

    return currentRef;
}

//============================================================
//=================== ThreatManager ==========================
//============================================================

ThreatManager::ThreatManager(Unit* owner)
    : iCurrentVictim(nullptr), iOwner(owner)
{
}

//============================================================
// 清空全部仇恨引用及当前锁定目标
void ThreatManager::clearReferences()
{
    iThreatContainer.clearReferences();
    iThreatOfflineContainer.clearReferences();
    iCurrentVictim = nullptr;
}

//============================================================
// 为 victim 添加仇恨（过滤自身/GM/死亡单位，计算修正与误导后写入容器）
void ThreatManager::addThreat(Unit* victim, float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* threatSpell, bool assist)
{
    // function deals with adding threat and adding players and pets into ThreatList
    // mobs, NPCs, guards have ThreatList and HateOfflineList
    // players and pets have only InHateListOf
    // HateOfflineList is used co contain unattackable victims (in-flight, in-water, GM etc.)
    // 怪物/NPC 同时拥有在线与离线仇恨列表；离线列表用于存放飞行、水中、GM 等暂不可攻击的目标

    // not to self (不对自己产生仇恨)
    if (victim == getOwner())
        return;

    // not to GM (不对 GM 产生仇恨)
    if (!victim || (victim->GetTypeId() == TYPEID_PLAYER && static_cast<Player*>(victim)->IsGameMaster()))
        return;

    // not to dead and not for dead (双方必须存活)
    if (!victim->IsAlive() || !getOwner()->IsAlive())
        return;

    float calculatedThreat = ThreatCalcHelper::CalcThreat(victim, iOwner, threat, crit, schoolMask, threatSpell, assist);

    // 若存在仇恨误导目标，将全部仇恨转移给误导目标（原目标仍需以 0 仇恨入表）
    if (calculatedThreat > 0.0f && !iOwner->IsIgnoringMisdirection())
    {
        if (Unit* redirectedTarget = victim->getHostileRefManager().GetThreatRedirectionTarget())
        {
            if (redirectedTarget != getOwner() && redirectedTarget->IsAlive())
            {
                addThreatDirectly(redirectedTarget, calculatedThreat, false);
                calculatedThreat = 0;                                 // but still need add to threat list (原目标仍需保留 0 仇恨条目)
            }
        }
    }

    addThreatDirectly(victim, calculatedThreat, threatSpell && threatSpell->HasAttribute(SPELL_ATTR_EX_NO_THREAT));
}

// 直接写入仇恨值，必要时新建引用并触发联结进战（Aggro Linking）与宠物主人连带仇恨
void ThreatManager::addThreatDirectly(Unit* victim, float threat, bool noNew)
{
    HostileReference* ref = iThreatContainer.addThreat(victim, threat);
    // Ref is not in the online refs, search the offline refs next (在线容器未找到则查找离线容器)
    if (!ref)
        ref = iThreatOfflineContainer.addThreat(victim, threat);

    if (!ref && !noNew) // there was no ref => create a new one (尚未入表且允许新建时创建新的 HostileReference)
    {
        HostileReference* hostileReference = new HostileReference(victim, this, 0); // threat has to be 0 here (先以 0 仇恨构造并入表)
        iThreatContainer.addReference(hostileReference);
        hostileReference->addThreat(threat); // now we add the real threat (再叠加实际仇恨以正确触发变更事件)
        Unit* owner = getOwner();
        owner->TriggerAggroLinkingEvent(victim);
        Unit* victim_owner = victim->GetMaster();
        if (victim->IsPropagatingThreatToOwner() && victim_owner && victim_owner->IsAlive() && victim_owner->CanJoinInAttacking(getOwner()))
            addThreat(victim_owner, 0.0f); // create a threat to the owner of a pet, if the pet attacks (宠物攻击时同步将主人以 0 仇恨拉入仇恨表)
        if (owner->IsOfflineTarget(victim) || victim->IsPlayer() && static_cast<Player*>(victim)->IsGameMaster())
            hostileReference->setOnlineOfflineState(false); // GM is always offline (GM 或离线条件目标转入离线列表)
    }
}

//============================================================
// 按百分比调整单个目标仇恨
void ThreatManager::modifyThreatPercent(Unit* victim, int32 threatPercent)
{
    iThreatContainer.modifyThreatPercent(victim, threatPercent);
}

// 按百分比调整所有在线目标仇恨
void ThreatManager::modifyAllThreatPercent(int32 threatPercent)
{
    iThreatContainer.modifyAllThreatPercent(threatPercent);
}

//============================================================
// 刷新并排序仇恨容器
void ThreatManager::UpdateContainers()
{
    iThreatContainer.update(getOwner()->IsIgnoringRangedTargets(), getOwner()->IsPlayer());
}

// 排序并选出当前应攻击的最高优先级敌对目标
Unit* ThreatManager::getHostileTarget()
{
    UpdateContainers();
    HostileReference* nextVictim = iThreatContainer.selectNextVictim(getOwner(), getCurrentVictim());
    setCurrentVictim(nextVictim);
    return getCurrentVictim() != nullptr ? getCurrentVictim()->getTarget() : nullptr;
}

//============================================================
// 查询指定目标的仇恨值（可选搜索离线列表）
float ThreatManager::getThreat(Unit* victim, bool alsoSearchOfflineList)
{
    float threat = 0.0f;
    HostileReference* ref = iThreatContainer.getReferenceByTarget(victim);
    if (!ref && alsoSearchOfflineList)
        ref = iThreatOfflineContainer.getReferenceByTarget(victim);
    if (ref)
        threat = ref->getThreat();
    else if (alsoSearchOfflineList)
        threat = iThreatOfflineContainer.getReferenceByTarget(victim)->getThreat();

    return threat;
}

// 返回在线及离线仇恨列表中的最高仇恨数值
float ThreatManager::GetHighestThreat()
{
    float value = 0.f;
    for (auto& ref : iThreatContainer.getThreatList())
        if (ref->getThreat() > value)
            value = ref->getThreat();
    for (auto& ref : iThreatOfflineContainer.getThreatList())
        if (ref->getThreat() > value)
            value = ref->getThreat();
    return value;
}

// 判断目标是否存在于仇恨列表中
bool ThreatManager::HasThreat(Unit * victim, bool alsoSearchOfflineList)
{
    HostileReference* ref = iThreatContainer.getReferenceByTarget(victim);
    if (!ref && alsoSearchOfflineList)
        ref = iThreatOfflineContainer.getReferenceByTarget(victim);
    return ref != nullptr;
}

//============================================================
// 依据当前生效的嘲讽/反嘲讽光环同步各敌对引用的 TauntState 并标记重排
void ThreatManager::TauntUpdate()
{
    const Unit::AuraList& tauntAuras = iOwner->GetAurasByType(SPELL_AURA_MOD_TAUNT);
    std::unordered_map<ObjectGuid, TauntState> tauntStates;
    uint32 state = STATE_TAUNTED;
    for (auto tauntAura : tauntAuras)
        tauntStates[tauntAura->GetCasterGuid()] = TauntState(state++);

    for (auto& ref : iThreatContainer.getThreatList())
    {
        if (ref->GetTauntState() == STATE_FIXATED)
            continue;
        auto iter = tauntStates.find(ref->getTarget()->GetObjectGuid());
        if (iter != tauntStates.end())
            ref->SetTauntState((*iter).second);
        else if (ref->getTarget()->HasAuraTypeWithCaster(SPELL_AURA_DETAUNT, ref->getSource()->getOwner()->GetObjectGuid()))
            ref->SetTauntState(STATE_DETAUNTED);
        else
            ref->SetTauntState(STATE_NONE);
    }
    setDirty(true);
}

// 将指定目标设为强制锁定 (STATE_FIXATED)，并清除其他目标的锁定状态
void ThreatManager::FixateTarget(Unit* victim)
{
    HostileReference* fixateRef = iThreatContainer.getReferenceByTarget(victim);
    if (fixateRef)
        fixateRef->SetTauntState(STATE_FIXATED);

    for (auto& ref : iThreatContainer.getThreatList())
        if (ref != fixateRef && ref->GetTauntState() == STATE_FIXATED)
            ref->SetTauntState(STATE_NONE);

    TauntUpdate();
}

void ThreatManager::setCurrentVictim(HostileReference* hostileReference)
{
    iCurrentVictim = hostileReference;
}

void ThreatManager::setCurrentVictimByTarget(Unit* target)
{
    if (iCurrentVictim && target == iCurrentVictim->getTarget())
        return;
    if (HostileReference* ref = iThreatContainer.getReferenceByTarget(target))
        setCurrentVictim(ref);
}

//============================================================
// The hated unit is gone, dead or deleted
// return true, if the event is consumed
// 响应仇恨引用发出的各类状态变更事件（仇恨变更、上下线切换、移除、压制状态变更）

void ThreatManager::processThreatEvent(ThreatRefStatusChangeEvent& threatRefStatusChangeEvent)
{
    threatRefStatusChangeEvent.setThreatManager(this);     // now we can set the threat manager (关联当前 ThreatManager 到事件)

    HostileReference* hostileReference = threatRefStatusChangeEvent.getReference();

    switch (threatRefStatusChangeEvent.getType())
    {
        case UEV_THREAT_REF_THREAT_CHANGE:
            if ((getCurrentVictim() == hostileReference && threatRefStatusChangeEvent.getFValue() < 0.0f) ||
                    (getCurrentVictim() != hostileReference && threatRefStatusChangeEvent.getFValue() > 0.0f))
                setDirty(true);                             // the order in the threat list might have changed (当前目标仇恨降低或非当前目标仇恨上升时标记重排序)
            break;
        case UEV_THREAT_REF_ONLINE_STATUS:
            if (!hostileReference->isOnline())
            {
                if (hostileReference == getCurrentVictim())
                {
                    setCurrentVictim(nullptr);
                    setDirty(true);
                }
                iThreatContainer.remove(hostileReference);
                iThreatOfflineContainer.addReference(hostileReference);
            }
            else
            {
                if (getCurrentVictim() && hostileReference->getThreat() > (1.1f * getCurrentVictim()->getThreat()))
                    setDirty(true);
                iThreatContainer.addReference(hostileReference);
                iThreatOfflineContainer.remove(hostileReference);
            }
            break;
        case UEV_THREAT_REF_REMOVE_FROM_LIST:
            if (hostileReference == getCurrentVictim())
            {
                setCurrentVictim(nullptr);
                setDirty(true);
            }
            if (hostileReference->isOnline())
            {
                iThreatContainer.remove(hostileReference);
            }
            else
                iThreatOfflineContainer.remove(hostileReference);
            break;
        case UEV_THREAT_REF_SUPPRESSED_STATUS:
            // Clear suppressed on suppress change (压制状态变化时清理其余目标的旧压制标记并重排)
            ClearSuppressed(hostileReference);
            setDirty(true);
            break;
    }
}

// 恢复不再满足压制条件的目标为正常仇恨状态
void ThreatManager::ClearSuppressed(HostileReference* except)
{
    for (HostileReference* const curRef : iThreatContainer.getThreatList())
        if (curRef->GetHostileState() == STATE_SUPPRESSED && curRef != except && !getOwner()->IsSuppressedTarget(curRef->getTarget()))
            curRef->SetHostileState(STATE_NORMAL);
}

// 移除战斗距离超过 60 码的仇恨引用（多用于定身单位清理过远目标）
void ThreatManager::DeleteOutOfRangeReferences()
{
    std::vector<HostileReference*> m_refs;
    for (auto& ref : iThreatContainer.getThreatList())
        if (ref->isValid() && ref->getTarget()->GetDistance(getOwner(), true, DIST_CALC_COMBAT_REACH) > 60.f)
            m_refs.push_back(ref);
    for (auto& ref : iThreatOfflineContainer.getThreatList())
        if (ref->isValid() && ref->getTarget()->GetDistance(getOwner(), true, DIST_CALC_COMBAT_REACH) > 60.f)
            m_refs.push_back(ref);
    for (auto& ref : m_refs)
    {
        ref->removeReference();
        delete ref;
    }
}

// 标记指定目标为压制状态，并在其下次仇恨变动时允许恢复
void ThreatManager::SetTargetSuppressed(Unit* target)
{
    if (HostileReference* ref = iThreatContainer.getReferenceByTarget(target))
    {
        ref->SetHostileState(STATE_SUPPRESSED);
        ref->SetSuppressabilityToggle();
    }
}

// 渐隐效果生效：记录并扣除临时仇恨值
void HostileReference::setFadeoutThreatReduction(float value)
{
    iFadeoutThreadReduction = value;
    addThreat(iFadeoutThreadReduction);
}

// 渐隐效果结束：归还临时扣除的仇恨值
void HostileReference::resetFadeoutThreatReduction()
{
    addThreat(-iFadeoutThreadReduction);
    iFadeoutThreadReduction = 0.f;
}
