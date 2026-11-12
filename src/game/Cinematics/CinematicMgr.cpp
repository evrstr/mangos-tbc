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

#include "CinematicMgr.h"
#include "Entities/Player.h"

CinematicMgr::CinematicMgr(Player* playerref)
{
    player = playerref;
    m_cinematicDiff = 0;
    m_lastCinematicCheck = 0;
    m_activeCinematicCameraId = 0;
    m_cinematicLength = 0;
    m_cinematicCamera = nullptr;
    m_remoteSightPosition = { 0.0f, 0.0f, 0.0f, 0.0f};
    m_CinematicObject = nullptr;
}

CinematicMgr::~CinematicMgr()
{
    if (m_cinematicCamera && m_activeCinematicCameraId)
        EndCinematic();
}

/// 启动过场动画：绑定镜头路径、预加载起始网格并召唤远景视野航点生物
void CinematicMgr::BeginCinematic()
{
    // Sanity check for active camera set (校验是否已设置有效的过场镜头 ID)
    if (m_activeCinematicCameraId == 0)
        return;

    auto itr = sFlyByCameraStore.find(m_activeCinematicCameraId);
    if (itr != sFlyByCameraStore.end())
    {
        // Initialize diff, and set camera (重置计时并关联镜头路径数据)
        m_cinematicDiff = 0;
        m_cinematicCamera = &itr->second;

        auto camitr = m_cinematicCamera->begin();
        if (camitr != m_cinematicCamera->end())
        {
            Position pos { camitr->locations.x, camitr->locations.y, camitr->locations.z, camitr->locations.w };
            if (!MaNGOS::IsValidMapCoord(pos.x, pos.y, pos.z, pos.o))
                return;

            player->GetMap()->ForceLoadGrid(camitr->locations.x, camitr->locations.y);
            m_CinematicObject = player->SummonCreature(VISUAL_WAYPOINT, pos.x, pos.y, pos.z, 0.0f, TEMPSPAWN_TIMED_DESPAWN, 5 * MINUTE * IN_MILLISECONDS);
            if (m_CinematicObject)
            {
                m_CinematicObject->SetActiveObjectState(true);
                player->GetCamera().SetView(m_CinematicObject, true);
            }

            // Get cinematic length (取最后一个关键帧的时间戳作为过场总时长)
            FlyByCameraCollection::const_reverse_iterator camrevitr = m_cinematicCamera->rbegin();
            if (camrevitr != m_cinematicCamera->rend())
                m_cinematicLength = camrevitr->timeStamp;
            else
                m_cinematicLength = 0;
        }
    }
}

/// 结束过场动画：重置玩家镜头视角并销毁远景航点生物
void CinematicMgr::EndCinematic()
{
    if (m_activeCinematicCameraId == 0)
        return;

    m_cinematicDiff = 0;
    m_cinematicCamera = nullptr;
    m_activeCinematicCameraId = 0;
    if (m_CinematicObject)
    {
        if (WorldObject* vpObject = player->GetCamera().GetBody())
            if (vpObject == m_CinematicObject)
                player->GetCamera().ResetView();

        m_CinematicObject->AddObjectToRemoveList();
    }
}

/// 按当前播放进度插值更新过场远景对象坐标，驱动客户端周边视野同步
bool CinematicMgr::UpdateCinematicLocation(uint32 /*diff*/)
{
    // should not occur (未处于过场动画中则直接返回)
    if (m_activeCinematicCameraId == 0)
        return false;

    // If we never received an end packet 10 seconds after the final timestamp then force an end (超过总时长 10 秒仍未收到结束包则强制结束)
    if (m_cinematicDiff > m_cinematicLength + 10 * IN_MILLISECONDS)
    {
        EndCinematic();
        return false;
    }

    // just created and waiting BeginCinematic call (刚创建，等待 BeginCinematic 初始化镜头路径)
    if (!m_cinematicCamera || m_cinematicCamera->size() == 0)
        return true;

    Position lastPosition;
    uint32 lastTimestamp = 0;
    Position nextPosition;
    uint32 nextTimestamp = 0;

    // Obtain direction of travel (查找当前时间戳前后的两个镜头关键帧以计算行进方向)
    for (FlyByCamera cam : *m_cinematicCamera)
    {
        if (cam.timeStamp > m_cinematicDiff)
        {
            nextPosition = { cam.locations.x, cam.locations.y, cam.locations.z, cam.locations.w };
            nextTimestamp = cam.timeStamp;
            break;
        }
        lastPosition = { cam.locations.x, cam.locations.y, cam.locations.z, cam.locations.w };
        lastTimestamp = cam.timeStamp;
    }

    float dx = nextPosition.x - lastPosition.x;
    float dy = nextPosition.y - lastPosition.y;

    float angle = std::atan2(dy, dx);
    angle = (angle >= 0) ? angle : 2 * float(M_PI) + angle;

    angle -= lastPosition.o;
    if (angle < 0)
        angle += 2 * float(M_PI);

    // Look for position around 2 second ahead of us. (计算约 2 秒前瞻位置，使视野提前覆盖前方场景)
    int32 workDiff = m_cinematicDiff;

    // Modify result based on camera direction (Humans for example, have the camera point behind) (根据镜头朝向调整前瞻方向，例如人类开场镜头朝后看)
    workDiff += static_cast<int32>(float(CINEMATIC_LOOKAHEAD) * cos(angle));

    // Get an iterator to the last entry in the cameras, to make sure we don't go beyond the end (限制不超过最后一个关键帧时间戳)
    FlyByCameraCollection::const_reverse_iterator endItr = m_cinematicCamera->rbegin();
    if (endItr != m_cinematicCamera->rend() && workDiff > static_cast<int32>(endItr->timeStamp))
        workDiff = endItr->timeStamp;

    // Never try to go back in time before the start of cinematic! (确保不回退到过场起始时间之前)
    if (workDiff < 0)
        workDiff = m_cinematicDiff;

    // Obtain the previous and next waypoint based on timestamp (根据前瞻时间戳重新定位前后航点)
    for (FlyByCamera cam : *m_cinematicCamera)
    {
        if (static_cast<int32>(cam.timeStamp) >= workDiff)
        {
            nextPosition = { cam.locations.x, cam.locations.y, cam.locations.z, cam.locations.w };
            nextTimestamp = cam.timeStamp;
            break;
        }
        lastPosition = { cam.locations.x, cam.locations.y, cam.locations.z, cam.locations.w };
        lastTimestamp = cam.timeStamp;
    }

    // Never try to go beyond the end of the cinematic (防止越界超出最后关键帧时间)
    if (workDiff > static_cast<int32>(nextTimestamp))
        workDiff = static_cast<int32>(nextTimestamp);

    // Interpolate the position for this moment in time (or the adjusted moment in time) (对前后航点坐标做线性插值)
    uint32 timeDiff = nextTimestamp - lastTimestamp;
    uint32 interDiff = workDiff - lastTimestamp;
    float xDiff = nextPosition.x - lastPosition.x;
    float yDiff = nextPosition.y - lastPosition.y;
    float zDiff = nextPosition.z - lastPosition.z;
    Position interPosition = { lastPosition.x + (xDiff * (float(interDiff) / float(timeDiff))), lastPosition.y +
        (yDiff * (float(interDiff) / float(timeDiff))), lastPosition.z + (zDiff * (float(interDiff) / float(timeDiff))), 0.0f };

    // Advance (at speed) to this position. The remote sight object is used
    // to send update information to player in cinematic
    // 移动远景航点对象至插值位置，触发服务端向客户端推送周围实体更新
    if (m_CinematicObject && MaNGOS::IsValidMapCoord(interPosition.x, interPosition.y, interPosition.z, interPosition.o))
        m_CinematicObject->MonsterMoveWithSpeed(interPosition.x, interPosition.y, interPosition.z, 500.0f, false, true);

    return true;
}
