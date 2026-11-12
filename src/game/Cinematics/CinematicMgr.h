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

#ifndef MANGOS_CINEMATICMGR_H
#define MANGOS_CINEMATICMGR_H

#include "Entities/Object.h"
#include "M2Stores.h"

#define CINEMATIC_LOOKAHEAD (2 * IN_MILLISECONDS)           // 过场镜头前瞻预读时间（2 秒），用于提前移动远景视野对象
#define CINEMATIC_UPDATEDIFF 200                            // 过场动画位置更新间隔（毫秒）

class Player;
class CinematicMgr;

typedef std::unique_ptr<CinematicMgr> CinematicMgrUPtr;

/// 过场动画（FlyBy 镜头）管理器：控制玩家播放开场/过场动画时的远景视野点移动与清理
class CinematicMgr
{
    friend class Player;
public:
    explicit CinematicMgr(Player* playerref);
    ~CinematicMgr();

    // Cinematic camera data and remote sight functions (过场动画镜头数据与远景视野控制接口)
    uint32 GetActiveCinematicCamera() const { return m_activeCinematicCameraId; }
    void SetActiveCinematicCamera(uint32 cinematicCameraId = 0) { m_activeCinematicCameraId = cinematicCameraId; }
    bool IsOnCinematic() const { return (m_cinematicCamera != nullptr); }
    /// 开始播放过场动画，召唤不可见航点生物并绑定玩家镜头视野
    void BeginCinematic();
    /// 结束过场动画，恢复玩家自身视角并销毁航点生物
    void EndCinematic();
    /// 根据已播放时间插值计算镜头位置并驱动远景航点移动，保证玩家周边网格保持加载
    bool UpdateCinematicLocation(uint32 diff);

private:
    // Remote location information (所属玩家指针)
    Player*     player;

protected:
    uint32      m_cinematicDiff;                            // 当前过场已播放毫秒数
    uint32      m_lastCinematicCheck;                       // 上次校验时间戳
    uint32      m_activeCinematicCameraId;                  // 当前激活的过场镜头 ID（0 表示未播放）
    uint32      m_cinematicLength;                          // 过场镜头总时长（毫秒）
    FlyByCameraCollection* m_cinematicCamera;               // 当前镜头关键帧路径集合
    Position    m_remoteSightPosition;                      // 远景视野位置缓存
    Creature*   m_CinematicObject;                          // 充当玩家远景视点的临时视觉航点生物
};

#endif
