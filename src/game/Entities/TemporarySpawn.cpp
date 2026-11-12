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

#include "Entities/TemporarySpawn.h"
#include "Log/Log.h"
#include "AI/BaseAI/CreatureAI.h"

// 构造临时召唤生物，记录召唤者 GUID
TemporarySpawn::TemporarySpawn(ObjectGuid summoner) :
    Creature(CREATURE_SUBTYPE_TEMPORARY_SUMMON), m_type(TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN), m_lifetime(0), m_spawner(summoner), m_linkedToOwnerAura(0)
{
}

// 每帧更新临时召唤物生命周期、脱战计时与所有者光环联动状态
void TemporarySpawn::Update(const uint32 diff)
{
    switch (m_type)
    {
        case TEMPSPAWN_MANUAL_DESPAWN:
            break;

        case TEMPSPAWN_TIMED_DESPAWN:
        {
            if (IsExpired())
            {
                UnSummon();
                return;
            }
            break;
        }

        case TEMPSPAWN_TIMED_OOC_DESPAWN:
        {
            if (IsAlive())
            {
                if (!IsInCombat())
                {
                    if (IsExpired())
                    {
                        UnSummon();
                        return;
                    }
                }
                else m_expirationTimestamp = GetMap()->GetCurrentClockTime() + std::chrono::milliseconds(m_lifetime);
            }
            else if (IsDespawned())
            {
                UnSummon();
                return;
            }
            break;
        }

        case TEMPSPAWN_CORPSE_TIMED_DESPAWN:
        {
            if (IsCorpse())
            {
                if (IsExpired())
                {
                    UnSummon();
                    return;
                }
            }
            if (IsDespawned())
            {
                UnSummon();
                return;
            }
            break;
        }

        case TEMPSPAWN_CORPSE_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped (若死亡状态直接进入 DEAD，则跳过尸体阶段直接回收)
            if (IsDead())
            {
                UnSummon();
                return;
            }

            break;
        }

        case TEMPSPAWN_DEAD_DESPAWN:
        {
            if (IsDespawned())
            {
                UnSummon();
                return;
            }
            break;
        }

        case TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped (若死亡状态直接进入 DEAD，则跳过尸体阶段直接回收)
            if (IsDead())
            {
                UnSummon();
                return;
            }

            if (!IsInCombat())
            {
                if (IsExpired())
                {
                    UnSummon();
                    return;
                }
            }
            else m_expirationTimestamp = GetMap()->GetCurrentClockTime() + std::chrono::milliseconds(m_lifetime);

            break;
        }

        case TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped (若已完全消失则立即回收)
            if (IsDespawned())
            {
                UnSummon();
                return;
            }

            if (!IsInCombat() && IsAlive() && !GetCharmerGuid())
            {
                if (IsExpired())
                {
                    UnSummon();
                    return;
                }
            }
            else m_expirationTimestamp = GetMap()->GetCurrentClockTime() + std::chrono::milliseconds(m_lifetime);

            break;
        }

        case TEMPSPAWN_TIMED_OR_CORPSE_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped (死亡或到期均直接回收)
            if (IsDead())
            {
                UnSummon();
                return;
            }
            if (IsExpired())
            {
                UnSummon();
                return;
            }
            break;
        }

        case TEMPSPAWN_TIMED_OR_DEAD_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped (完全消失或未被魅惑且到期时回收)
            if (IsDespawned())
            {
                UnSummon();
                return;
            }
            if (!GetCharmerGuid())
            {
                if (IsExpired())
                {
                    UnSummon();
                    return;
                }
            }
            break;
        }

        default:
            UnSummon();
            sLog.outError("Temporary summoned creature (entry: %u) have unknown type %u of ", GetEntry(), m_type);
            break;
    }

    // 处理与召唤者光环之间的联动检查与清理
    switch (m_deathState)
    {
        case ALIVE:
            if (m_linkedToOwnerAura & TEMPSPAWN_LINKED_AURA_OWNER_CHECK)
            {
                if (!CheckAuraOnOwner())
                    UnSummon();
            }
            break;

        case DEAD:
        case CORPSE:
            if (m_linkedToOwnerAura & TEMPSPAWN_LINKED_AURA_REMOVE_OWNER)
            {
                RemoveAuraFromOwner();
                m_linkedToOwnerAura = 0;                    // we dont need to recheck (已移除关联光环，无需再次检查)
            }

        default:
            break;
    }

    Creature::Update(diff);
}

// 设置召唤类型、持续时间并计算到期时间戳
void TemporarySpawn::SetSummonProperties(TempSpawnType type, uint32 lifetime)
{
    m_type = type;
    m_expirationTimestamp = GetMap()->GetCurrentClockTime() + std::chrono::milliseconds(lifetime);
    m_lifetime = lifetime;

    // set month as re spawn delay to avoid it (将重生延迟设为 1 个月以避免意外自动刷新)
    m_respawnDelay = MONTH;
}

// 初始化召唤属性、将临时生物加入地图并初始化 AI
void TemporarySpawn::Summon(TempSpawnType type, uint32 lifetime)
{
    SetSummonProperties(type, lifetime);

    GetMap()->Add((Creature*)this);

    AIM_Initialize();
}

// 解除召唤：清理光环联动、通知召唤者与自身 AI，并加入移除列表
void TemporarySpawn::UnSummon()
{
    UnsummonCleanup();

    if (m_linkedToOwnerAura & TEMPSPAWN_LINKED_AURA_REMOVE_OWNER)
        RemoveAuraFromOwner();

    if (GetSpawnerGuid().IsCreature())
    {
        if (Creature* sum = GetMap()->GetCreature(GetSpawnerGuid()))
            if (sum->AI())
                sum->AI()->SummonedCreatureDespawn(this);
    }

    if (AI())
        AI()->SummonedCreatureDespawn(this);

    AddObjectToRemoveList();
}

// 检查召唤者身上是否仍存在创建此召唤物的法术光环
bool TemporarySpawn::CheckAuraOnOwner()
{
    if (uint32 spellId = GetUInt32Value(UNIT_CREATED_BY_SPELL))
    {
        if (Unit* spawner = GetSpawner())
           return spawner->HasAura(spellId);
    }
    return false;
}

// 从召唤者身上移除创建此召唤物的关联法术光环
void TemporarySpawn::RemoveAuraFromOwner()
{
    if (uint32 spellid = GetUInt32Value(UNIT_CREATED_BY_SPELL))
    {
        if (Unit* spawner = GetSpawner())
            spawner->RemoveAurasDueToSpell(spellid);
    }
}

// 临时召唤生物不持久化保存到数据库
void TemporarySpawn::SaveToDB()
{
}

// 设置死亡状态；若是尸体定时消失模式则在刚死亡时重置尸体过期计时
void TemporarySpawn::SetDeathState(DeathState state)
{
    if (state == JUST_DIED && m_type == TEMPSPAWN_CORPSE_TIMED_DESPAWN)
        m_expirationTimestamp = GetMap()->GetCurrentClockTime() + std::chrono::milliseconds(m_lifetime);
    Creature::SetDeathState(state);
}

// 判断当前地图时钟是否已超过到期时间
bool TemporarySpawn::IsExpired() const
{
    auto now = GetMap()->GetCurrentClockTime();
    if (now >= m_expirationTimestamp)
        return true;
    return false;
}

// 构造绑定路径点信息的临时召唤生物
TemporarySpawnWaypoint::TemporarySpawnWaypoint(ObjectGuid summoner, uint32 waypoint_id, int32 path_id, uint32 pathOrigin) :
    TemporarySpawn(summoner),
    m_waypoint_id(waypoint_id),
    m_path_id(path_id),
    m_pathOrigin(pathOrigin)
{
}
