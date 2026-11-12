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

#ifndef _HOSTILEREFMANAGER
#define _HOSTILEREFMANAGER

#include "Common.h"
#include "Entities/ObjectGuid.h"
#include "Utilities/LinkedReference/RefManager.h"

class Unit;
class ThreatManager;
class HostileReference;
struct SpellEntry;

//=================================================

// 敌对引用管理器：挂在“被仇恨者”身上，反向追踪所有将其列入仇恨列表的 ThreatManager（即哪些怪物/单位正在仇恨自己）
class HostileRefManager : public RefManager<Unit, ThreatManager>
{
    public:
        explicit HostileRefManager(Unit* owner);
        ~HostileRefManager();

        Unit* getOwner() const { return iOwner; }

        // send threat to all my hateres for the pVictim
        // The pVictim is hated than by them as well
        // use for buffs and healing threat functionality
        // 向所有仇恨自己的敌对单位传递对 victim 的协助仇恨（用于治疗和增益法术分摊仇恨）
        void threatAssist(Unit* victim, float threat, SpellEntry const* threatSpell = nullptr, bool singleTarget = false, bool ignoreTimer = false);
        // 临时性仇恨消退（如渐隐术 Fade 应用与结束还原）
        void threatTemporaryFade(Unit* victim, float threat, bool apply);

        // 按百分比调整所有敌对单位对自己的仇恨
        void addThreatPercent(int32 threatPercent);

        // The references are not needed anymore
        // tell the source to remove them from the list and free the mem
        // 清理并释放全部仇恨引用（通知来源 ThreatManager 移除）
        void deleteReferences();

        // Remove specific faction references (移除属于指定阵营的敌对引用)
        void deleteReferencesForFaction(uint32 faction);

        HostileReference* getFirst();

        // 重新计算并更新所有仇恨引用对应的在线/离线状态
        void updateThreatTables();

        void setOnlineOfflineState(bool isOnline);
        void updateOnlineOfflineState(bool pIsOnline);

        // set state for one reference, defined by Unit (按单位设置单个仇恨引用的在线/离线状态)
        void setOnlineOfflineState(Unit* victim, bool isOnline);

        // delete one reference, defined by Unit (删除来自指定单位的单个仇恨引用)
        void deleteReference(Unit* victim);

        // redirection threat data (误导/仇恨转移目标管理)
        void SetThreatRedirection(ObjectGuid guid) { m_redirectionTargetGuid = guid; }
        void ResetThreatRedirection() { m_redirectionTargetGuid.Clear(); }
        Unit* GetThreatRedirectionTarget() const;

        // Suppression (目标压制状态处理，如无法攻击/免疫伤害时的降优先级标记)
        void HandleSuppressed(bool apply, bool immunity = false);

    private:
        Unit* iOwner;                                       // owner of manager variable, back ref. to it, always exist (所属单位指针)

        ObjectGuid m_redirectionTargetGuid;                 // in 2.x redirected only full threat (TBC 2.x 中误导仅全额转移仇恨至单一目标 GUID)
};
//=================================================
#endif
