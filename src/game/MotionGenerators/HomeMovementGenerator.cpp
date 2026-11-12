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

#include "HomeMovementGenerator.h"
#include "Maps/Map.h"
#include "Entities/Creature.h"
#include "AI/BaseAI/CreatureAI.h"
#include "Movement/MoveSplineInit.h"
#include "Movement/MoveSpline.h"

/// 初始化返巢状态：临时提升为活跃对象、标记 EVADE_HOME 并启动返巢路径
void HomeMovementGenerator<Creature>::Initialize(Creature& owner)
{
    wasActive = owner.isActiveObject();
    if (!wasActive)
        owner.SetActiveObjectState(true);
    owner.GetCombatManager().SetEvadeState(EVADE_HOME);

    _setTargetLocation(owner);
}

void HomeMovementGenerator<Creature>::Reset(Creature&)
{
}

/// 确定返巢目标位置（底层默认移动重置点 > 战斗起点 > 出生坐标）并构建移动样条
void HomeMovementGenerator<Creature>::_setTargetLocation(Creature& owner)
{
    if (owner.hasUnitState(UNIT_STAT_NOT_MOVE))
        return;

    Position pos;
    // at apply we can select more nice return points base at current movegen (优先向栈内当前默认移动生成器查询合适的复位坐标)
    if (owner.GetMotionMaster()->empty() || !owner.GetMotionMaster()->top()->GetResetPosition(owner, pos))
        owner.GetCombatStartPosition(pos);

    if (pos.IsEmpty())
        owner.GetRespawnCoord(pos.x, pos.y, pos.z, &pos.o);
    // 野外距离过远（超过 150 码）直接强制消失并在 5 秒后原地重生，避免长距离穿图奔跑
    if (owner.GetDistance(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ(), DIST_CALC_NONE, owner.GetTransport()) > 150.f * 150.f)
    {
        if (!owner.IsInWorld() || !owner.GetMap()->IsDungeon())
        {
            arrived = true;
            Finalize(owner);
            owner.SetRespawnDelay(5, true);
            owner.ForcedDespawn(1);
            return;
        }
    }

    PathFinder path(&owner);

    Position curPos = owner.GetPosition(owner.GetTransport());
    // source and target pos must be local coords (起止坐标均使用载具局部坐标系，非载具时即世界坐标)
    path.calculate(G3D::Vector3(curPos.GetPositionX(), curPos.GetPositionY(), curPos.GetPositionZ()), G3D::Vector3(pos.GetPositionX(), pos.GetPositionY(), pos.GetPositionZ()), true);

    Movement::MoveSplineInit init(owner);
    init.MovebyPath(path.getPath());
    init.SetWalk(!runHome);
    init.SetFacing(pos.GetPositionO());
    // 若寻路失败或走捷径，提高速度使生物快速复位
    if (path.getPathType() & (PATHFIND_NOPATH | PATHFIND_SHORTCUT))
        init.SetVelocity(400.f);
    init.Launch();

    arrived = false;
    owner.clearUnitState(static_cast<uint32>(UNIT_STAT_ALL_DYN_STATES));
}

bool HomeMovementGenerator<Creature>::Update(Creature& owner, const uint32& /*time_diff*/)
{
    arrived = owner.movespline->Finalized();
    return !arrived;
}

/// 返巢结束清理：清除脱战状态、恢复阵营/附加光环、通知召唤者并还原 ActiveObject 状态
void HomeMovementGenerator<Creature>::Finalize(Creature& owner)
{
    owner.GetCombatManager().SetEvadeState(EVADE_NONE);
    if (arrived)
    {
        if (owner.GetTemporaryFactionFlags() & TEMPFACTION_RESTORE_REACH_HOME)
            owner.ClearTemporaryFaction();

        owner.LoadCreatureAddon(true);
        owner.TriggerHomeEvents();
        if (owner.IsTemporarySummon())
        {
            if (owner.GetSpawnerGuid().IsCreatureOrPet())
                if (Creature* pSummoner = owner.GetMap()->GetAnyTypeCreature(owner.GetSpawnerGuid()))
                    if (pSummoner->AI())
                        pSummoner->AI()->SummonedJustReachedHome(&owner);
        }

        if (!wasActive && owner.IsInWorld())
            owner.SetActiveObjectState(false);

        owner.SetCombatStartPosition(Position());
    }
}
