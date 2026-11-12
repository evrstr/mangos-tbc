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

#ifndef DYNAMICMAP_TREE_H
#define DYNAMICMAP_TREE_H
#include "Platform/Define.h"
namespace G3D
{
    class Vector3;
    class Matrix3;
    class AABox;
    class Ray;
}
class GameObjectModel;

// 动态地图碰撞树（Pimpl 封装）：管理可动态开关/移动的 GameObject 模型（如大门、电梯、桥梁）的视线与高度检测
class DynamicMapTree
{
    public:
        DynamicMapTree();
        ~DynamicMapTree();

        bool isInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2, bool ignoreM2Model) const; // 校验两点间动态模型视线通畅性
        bool getIntersectionTime(const G3D::Ray& ray, const G3D::Vector3& endPos, float& maxDist) const; // 计算射线与动态模型的最近交点距离
        bool getObjectHitPos(const G3D::Vector3& pPos1, const G3D::Vector3& pPos2, G3D::Vector3& pResultHitPos, float pModifyDist) const; // 获取碰撞回退后的落点（Vector3 重载）
        bool getObjectHitPos(float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float& ry, float& rz, float pModifyDist) const; // 获取碰撞回退后的落点（坐标重载）
        float getHeight(float x, float y, float z, float maxSearchDist) const; // 向下发射垂直射线查询动态模型地面高度

        void insert(const GameObjectModel&);                            // 插入游戏对象模型到动态树
        void remove(const GameObjectModel&);                            // 从动态树移除游戏对象模型
        bool contains(const GameObjectModel&) const;                    // 判断模型是否已注册在动态树中
        int size() const;                                               // 返回已注册模型数量

        void balance();                                                 // 强制平衡底层网格 BIH 节点
        void update(uint32 t_diff);                                     // 周期性刷新脏节点平衡状态
    private:
        struct DynTreeImpl& impl;                                       // 内部实现（基于 RegularGrid2D + BIHWrap）
};

#endif
