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

#ifndef MANGOSSERVER_GAMEOBJECTMODEL_H
#define MANGOSSERVER_GAMEOBJECTMODEL_H

#include <G3D/Matrix3.h>
#include <G3D/Vector3.h>
#include <G3D/AABox.h>
#include <G3D/Ray.h>
#include "Server/DBCStructure.h"
#include "Entities/GameObject.h"

#include "Platform/Define.h"

namespace VMAP
{
    class WorldModel;
}


// 动态游戏对象 (GameObject) 的 VMAP 碰撞模型包装类，绑定变换矩阵与共享 WorldModel 几何体
class GameObjectModel
{
        bool collision_enabled;                                         // 当前是否启用碰撞检测（如门开启后关闭碰撞）
        G3D::AABox iBound;                                              // 世界坐标系下的轴对齐包围盒
        G3D::Matrix3 iInvRot;                                           // 逆旋转矩阵（世界坐标转模型局部坐标）
        G3D::Vector3 iPos;                                              // 世界坐标位置
        //G3D::Vector3 iRot;
        float iInvScale;                                                // 缩放倒数
        float iScale;                                                   // 模型缩放倍数
        VMAP::WorldModel* iModel;                                       // 指向 VMapManager2 管理的共享几何模型

        GameObjectModel() : collision_enabled(false), iInvScale(0), iScale(0), iModel(nullptr) {}
        bool initialize(const GameObject* const pGo, const GameObjectDisplayInfoEntry* pDisplayInfo); // 依据 GO 与显示信息加载模型并初始化空间变换

    public:
        std::string name;                                               // 模型文件名

        const G3D::AABox& getBounds() const { return iBound; }          // 获取世界空间包围盒

        ~GameObjectModel();                                             // 析构时通过 VMapFactory 归还共享模型引用

        const G3D::Vector3& getPosition() const { return iPos;}         // 获取世界坐标

        /** Enables\disables collision. (启用或禁用该动态对象的碰撞判定) */
        void disable() { collision_enabled = false;}
        void enable(bool enabled) { collision_enabled = enabled;}

        bool intersectRay(const G3D::Ray& ray, float& MaxDist, bool StopAtFirstHit, bool ignoreM2Model) const; // 世界射线转局部坐标后与模型求交

        static GameObjectModel* construct(const GameObject* const pGo); // 工厂方法：按 GO 的 DisplayId 创建并初始化碰撞模型

        bool Relocate(GameObject const& go);                            // 坐标/朝向变化时重新计算包围盒与逆旋转矩阵
};
#endif
