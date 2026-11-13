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

#ifndef _BIH_H
#define _BIH_H

#include <G3D/Vector3.h>
#include <G3D/Ray.h>
#include <G3D/AABox.h>

#include <Platform/Define.h>
#include <Util/Errors.h>

#include <vector>
#include <algorithm>
#include <cmath>

#define MAX_STACK_SIZE 64 // BIH 遍历栈最大深度

using G3D::Vector3;
using G3D::AABox;
using G3D::Ray;

// 将 float 按原始二进制位解释为 uint32（用于提取符号位及写入紧致节点数组）
static inline uint32 floatToRawIntBits(float f)
{
    union
    {
        uint32 ival;
        float fval;
    } temp;
    temp.fval = f;
    return temp.ival;
}

// 将 uint32 按原始二进制位还原为 float
static inline float intBitsToFloat(uint32 i)
{
    union
    {
        uint32 ival;
        float fval;
    } temp;
    temp.ival = i;
    return temp.fval;
}

// 轴对齐包围盒区间（低点与高点）
struct AABound
{
    Vector3 lo, hi;
};

/** Bounding Interval Hierarchy Class.
    Building and Ray-Intersection functions based on BIH from
    Sunflow, a Java Raytracer, released under MIT/X11 License
    http://sunflow.sourceforge.net/
    Copyright (c) 2003-2007 Christopher Kulla
    包围区间层次树 (Bounding Interval Hierarchy, BIH)：用于静态/动态模型空间划分及高速射线/点相交查询
*/

class BIH
{
    private:
        // 初始化为空树（包含一个哑叶节点）
        void init_empty()
        {
            tree.clear();
            objects.clear();
            // create space for the first node
            tree.push_back(static_cast<uint32>(3 << 30)); // dummy leaf
            tree.insert(tree.end(), 2, 0);
        }

    public:
        BIH() {init_empty();}
        // 根据图元数组及包围盒提取函数构建 BIH 层次结构
        template< class BoundsFunc, class PrimArray >
        void build(const PrimArray& primitives, BoundsFunc& getBounds, uint32 leafSize = 3, bool printStats = false)
        {
            if (primitives.size() == 0)
            {
                init_empty();
                return;
            }
            buildData dat;
            dat.maxPrims = leafSize;
            dat.numPrims = primitives.size();
            dat.indices = new uint32[dat.numPrims];
            dat.primBound = new AABox[dat.numPrims];
            getBounds(primitives[0], bounds);
            for (uint32 i = 0; i < dat.numPrims; ++i)
            {
                dat.indices[i] = i;
                getBounds(primitives[i], dat.primBound[i]);
                bounds.merge(dat.primBound[i]);
            }
            std::vector<uint32> tempTree;
            BuildStats stats;
            buildHierarchy(tempTree, dat, stats);
            if (printStats)
                stats.printStats();

            objects.resize(dat.numPrims);
            for (uint32 i = 0; i < dat.numPrims; ++i)
                objects[i] = dat.indices[i];
            // nObjects = dat.numPrims;
            tree = tempTree;
            delete[] dat.primBound;
            delete[] dat.indices;
        }
        size_t primCount() const { return objects.size(); }

        // 射线与 BIH 树求交（使用前/后子节点栈遍历并支持首次命中即停与忽略 M2 模型）
        template<typename RayCallback>
        void intersectRay(const Ray& r, RayCallback& intersectCallback, float& maxDist, bool stopAtFirst = false, bool ignoreM2Model = false) const
        {
            float intervalMin = -1.f;
            float intervalMax = -1.f;
            Vector3 org = r.origin();
            Vector3 dir = r.direction();

            // 过滤 NaN 坐标与方向，防止死循环绕过剪枝反复入栈导致崩溃 (PR #881)
            for (int i = 0; i < 3; ++i)
                if (!std::isfinite(org[i]) || !std::isfinite(dir[i]))
                    return;
            // 距离为 NaN 或负数时直接返回（正无穷大用于室内区域判定，合法放行）
            if (std::isnan(maxDist) || maxDist < 0.0f)
                return;

            Vector3 invDir;
            for (int i = 0; i < 3; ++i)
            {
                invDir[i] = 1.f / dir[i];
                if (G3D::fuzzyNe(dir[i], 0.0f))
                {
                    float t1 = (bounds.low()[i] - org[i]) * invDir[i];
                    float t2 = (bounds.high()[i] - org[i]) * invDir[i];
                    if (t1 > t2)
                        std::swap(t1, t2);
                    if (t1 > intervalMin)
                        intervalMin = t1;
                    if (t2 < intervalMax || intervalMax < 0.f)
                        intervalMax = t2;
                    // intervalMax can only become smaller for other axis,
                    //  and intervalMin only larger respectively, so stop early
                    if (intervalMax <= 0 || intervalMin >= maxDist)
                        return;
                }
            }

            if (intervalMin > intervalMax)
                return;
            intervalMin = std::max(intervalMin, 0.f);
            intervalMax = std::min(intervalMax, maxDist);

            uint32 offsetFront[3];
            uint32 offsetBack[3];
            uint32 offsetFront3[3];
            uint32 offsetBack3[3];
            // compute custom offsets from direction sign bit

            for (int i = 0; i < 3; ++i)
            {
                offsetFront[i] = floatToRawIntBits(dir[i]) >> 31;
                offsetBack[i] = offsetFront[i] ^ 1;
                offsetFront3[i] = offsetFront[i] * 3;
                offsetBack3[i] = offsetBack[i] * 3;

                // avoid always adding 1 during the inner loop
                ++offsetFront[i];
                ++offsetBack[i];
            }

            StackNode stack[MAX_STACK_SIZE];
            int stackPos = 0;
            int node = 0;

            while (true)
            {
                while (true)
                {
                    uint32 tn = tree[node];
                    uint32 axis = (tn & (3 << 30)) >> 30;
                    const bool BVH2 = (tn & (1 << 29)) != 0;
                    int offset = tn & ~(7 << 29);
                    if (!BVH2)
                    {
                        if (axis < 3)
                        {
                            // "normal" interior node
                            float tf = (intBitsToFloat(tree[node + offsetFront[axis]]) - org[axis]) * invDir[axis];
                            float tb = (intBitsToFloat(tree[node + offsetBack[axis]]) - org[axis]) * invDir[axis];
                            // ray passes between clip zones
                            if (tf < intervalMin && tb > intervalMax)
                                break;
                            int back = offset + offsetBack3[axis];
                            node = back;
                            // ray passes through far node only
                            if (tf < intervalMin)
                            {
                                intervalMin = (tb >= intervalMin) ? tb : intervalMin;
                                continue;
                            }
                            node = offset + offsetFront3[axis]; // front
                                                                // ray passes through near node only
                            if (tb > intervalMax)
                            {
                                intervalMax = (tf <= intervalMax) ? tf : intervalMax;
                                continue;
                            }

                            // 栈深度溢出保护，防止越界覆写栈内存导致段错误 (PR #881)
                            if (stackPos >= MAX_STACK_SIZE)
                                return;

                            // ray passes through both nodes
                            // push back node
                            stack[stackPos].node = back;
                            stack[stackPos].tnear = (tb >= intervalMin) ? tb : intervalMin;
                            stack[stackPos].tfar = intervalMax;
                            ++stackPos;
                            // update ray interval for front node
                            intervalMax = (tf <= intervalMax) ? tf : intervalMax;
                        }
                        else
                        {
                            // leaf - test some objects
                            int n = tree[node + 1];
                            while (n > 0)
                            {
                                bool hit = intersectCallback(r, objects[offset], maxDist, stopAtFirst, ignoreM2Model);
                                if (stopAtFirst && hit) return;
                                --n;
                                ++offset;
                            }
                            break;
                        }
                    }
                    else
                    {
                        if (axis > 2)
                            return; // should not happen
                        float tf = (intBitsToFloat(tree[node + offsetFront[axis]]) - org[axis]) * invDir[axis];
                        float tb = (intBitsToFloat(tree[node + offsetBack[axis]]) - org[axis]) * invDir[axis];
                        node = offset;
                        intervalMin = (tf >= intervalMin) ? tf : intervalMin;
                        intervalMax = (tb <= intervalMax) ? tb : intervalMax;
                        if (intervalMin > intervalMax)
                            break;
                    }
                } // traversal loop
                do
                {
                    // stack is empty?
                    if (stackPos == 0)
                        return;
                    // move back up the stack
                    --stackPos;
                    intervalMin = stack[stackPos].tnear;
                    if (maxDist < intervalMin)
                        continue;
                    node = stack[stackPos].node;
                    intervalMax = stack[stackPos].tfar;
                    break;
                } while (true);
            }
        }

        // 空间点查询：遍历包含点 p 的所有叶节点图元并触发回调
        template<typename IsectCallback>
        void intersectPoint(const Vector3& p, IsectCallback& intersectCallback) const
        {
            if (!bounds.contains(p))
                return;

            StackNode stack[MAX_STACK_SIZE];
            int stackPos = 0;
            int node = 0;

            while (true)
            {
                while (true)
                {
                    uint32 tn = tree[node];
                    uint32 axis = (tn & (3 << 30)) >> 30;
                    const bool BVH2 = (tn & (1 << 29)) != 0;
                    int offset = tn & ~(7 << 29);
                    if (!BVH2)
                    {
                        if (axis < 3)
                        {
                            // "normal" interior node
                            float tl = intBitsToFloat(tree[node + 1]);
                            float tr = intBitsToFloat(tree[node + 2]);
                            // point is between clip zones
                            if (tl < p[axis] && tr > p[axis])
                                break;
                            int right = offset + 3;
                            node = right;
                            // point is in right node only
                            if (tl < p[axis])
                            {
                                continue;
                            }
                            node = offset; // left
                            // point is in left node only
                            if (tr > p[axis])
                            {
                                continue;
                            }
                            // point is in both nodes
                            // push back right node
                            stack[stackPos].node = right;
                            ++stackPos;
                        }
                        else
                        {
                            // leaf - test some objects
                            int n = tree[node + 1];
                            while (n > 0)
                            {
                                intersectCallback(p, objects[offset]); // !!!
                                --n;
                                ++offset;
                            }
                            break;
                        }
                    }
                    else // BVH2 node (empty space cut off left and right)
                    {
                        if (axis > 2)
                            return; // should not happen
                        float tl = intBitsToFloat(tree[node + 1]);
                        float tr = intBitsToFloat(tree[node + 2]);
                        node = offset;
                        if (tl > p[axis] || tr < p[axis])
                            break;
                    }
                } // traversal loop

                // stack is empty?
                if (stackPos == 0)
                    return;
                // move back up the stack
                --stackPos;
                node = stack[stackPos].node;
            }
        }

        bool writeToFile(FILE* wf) const;                             // 序列化 BIH 树与图元索引到文件
        bool readFromFile(FILE* rf);                                    // 从文件反序列化加载 BIH 树与图元索引

    protected:
        std::vector<uint32> tree;                                       // 紧致编码的节点数组（前2位存划分轴/叶标识，第29位存 BVH2 标记）
        std::vector<uint32> objects;                                    // 重排后的图元索引表
        AABox bounds;                                                   // 整棵树的总包围盒

        struct buildData                                                // 构建期辅助数据结构
        {
            uint32* indices;
            AABox* primBound;
            uint32 numPrims;
            int maxPrims;
        };
        struct StackNode                                                // 非递归遍历栈帧
        {
            uint32 node;
            float tnear;
            float tfar;
        };

        // 构建统计信息收集器
        class BuildStats
        {
            private:
                int numNodes;
                int numLeaves;
                int sumObjects;
                int minObjects;
                int maxObjects;
                int sumDepth;
                int minDepth;
                int maxDepth;
                int numLeavesN[6];
                int numBVH2;

            public:
                BuildStats():
                    numNodes(0), numLeaves(0), sumObjects(0), minObjects(0x0FFFFFFF),
                    maxObjects(0xFFFFFFFF), sumDepth(0), minDepth(0x0FFFFFFF),
                    maxDepth(0xFFFFFFFF), numBVH2(0)
                {
                    for (int& i : numLeavesN) i = 0;
                }

                void updateInner() { ++numNodes; }
                void updateBVH2() { ++numBVH2; }
                void updateLeaf(int depth, int n);
                void printStats();
        };

        void buildHierarchy(std::vector<uint32>& tempTree, buildData& dat, BuildStats& stats); // 自顶向下构建 BIH 层次树

        void createNode(std::vector<uint32>& tempTree, int nodeIndex, uint32 left, uint32 right) const
        {
            // write leaf node (写入叶节点编码：轴位=3 表示叶子，记录起始索引与图元数量)
            tempTree[nodeIndex + 0] = (3 << 30) | left;
            tempTree[nodeIndex + 1] = right - left + 1;
        }

        void subdivide(int left, int right, std::vector<uint32>& tempTree, buildData& dat, AABound& gridBox, AABound& nodeBox, int nodeIndex, int depth, BuildStats& stats); // 递归空间切分节点
};

#endif // _BIH_H
