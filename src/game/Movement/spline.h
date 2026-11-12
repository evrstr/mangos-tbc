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

#ifndef MANGOSSERVER_SPLINE_H
#define MANGOSSERVER_SPLINE_H

#include "typedefs.h"
#include <G3D/Vector3.h>

namespace Movement
{
    /// 样条曲线基类，支持线性、Catmull-Rom 与三次 Bezier 位置及导数求值
    class SplineBase
    {
        public:
            typedef int index_type;
            typedef std::vector<Vector3> ControlArray;

            /// 样条曲线插值求值模式
            enum EvaluationMode
            {
                ModeLinear,          // 分段线性插值
                ModeCatmullrom,      // Catmull-Rom 平滑样条插值
                ModeBezier3_Unused,  // 三次 Bezier 曲线插值（当前未使用）
                UninitializedMode,   // 未初始化状态
                ModesEnd
            };

        protected:
            ControlArray points;     // 控制点数组（含首尾虚拟辅助控制点）

            index_type index_lo;     // 有效段起始索引（闭区间）
            index_type index_hi;     // 有效段结束索引（开区间上限）

            uint8 m_mode;            // 当前插值模式
            bool cyclic;             // 是否为闭合循环样条

            enum
            {
                // could be modified, affects segment length evaluation precision
                // lesser value saves more performance in cost of lover precision
                // minimal value is 1
                // client's value is 20, blizzs use 2-3 steps to compute length
                // 弧长数值积分采样步数：影响段长计算精度与开销（客户端为 20，暴雪服务端用 2~3 步）
                STEPS_PER_SEGMENT = 3,
            };
            static_assert(STEPS_PER_SEGMENT > 0, "shouldn't be lesser than 1");

        protected:
            // 各模式的位置求值函数表
            void EvaluateLinear(index_type, float, Vector3&) const;
            void EvaluateCatmullRom(index_type, float, Vector3&) const;
            void EvaluateBezier3(index_type, float, Vector3&) const;
            typedef void (SplineBase::*EvaluationMethtod)(index_type, float, Vector3&) const;
            static EvaluationMethtod evaluators[ModesEnd];

            // 各模式的一阶导数（切线方向）求值函数表
            void EvaluateDerivativeLinear(index_type, float, Vector3&) const;
            void EvaluateDerivativeCatmullRom(index_type, float, Vector3&) const;
            void EvaluateDerivativeBezier3(index_type, float, Vector3&) const;
            static EvaluationMethtod derivative_evaluators[ModesEnd];

            // 各模式的单段长度计算函数表
            float SegLengthLinear(index_type) const;
            float SegLengthCatmullRom(index_type) const;
            float SegLengthBezier3(index_type) const;
            typedef float(SplineBase::*SegLenghtMethtod)(index_type) const;
            static SegLenghtMethtod seglengths[ModesEnd];

            // 各模式的控制点数组初始化函数表
            void InitLinear(const Vector3*, index_type, bool, index_type);
            void InitCatmullRom(const Vector3*, index_type, bool, index_type);
            void InitBezier3(const Vector3*, index_type, bool, index_type);
            typedef void (SplineBase::*InitMethtod)(const Vector3*, index_type, bool, index_type);
            static InitMethtod initializers[ModesEnd];

            void UninitializedSpline() const { MANGOS_ASSERT(false);}

        public:

            explicit SplineBase() : index_lo(0), index_hi(0), m_mode(UninitializedMode), cyclic(false) {}

            /** Caclulates the position for given segment Idx, and percent of segment length t
                @param t - percent of segment length, assumes that t in range [0, 1]
                @param Idx - spline segment index, should be in range [first, last)
                计算第 Idx 段在局部进度 u ∈ [0, 1] 处的空间位置
             */
            void evaluate_percent(index_type Idx, float u, Vector3& c) const {(this->*evaluators[m_mode])(Idx, u, c);}

            /** Caclulates derivation in index Idx, and percent of segment length t
                @param Idx - spline segment index, should be in range [first, last)
                @param t  - percent of spline segment length, assumes that t in range [0, 1]
                计算第 Idx 段在局部进度 u ∈ [0, 1] 处的一阶导数（切向量，用于计算朝向）
             */
            void evaluate_derivative(index_type Idx, float u, Vector3& hermite) const {(this->*derivative_evaluators[m_mode])(Idx, u, hermite);}

            /**  Bounds for spline indexes. All indexes should be in range [first, last). */
            /// 样条有效段索引边界，所有段索引满足 [first, last)
            index_type first() const { return index_lo;}
            index_type last()  const { return index_hi;}

            bool empty() const { return index_lo == index_hi;}
            EvaluationMode mode() const { return (EvaluationMode)m_mode;}
            bool isCyclic() const { return cyclic;}

            const ControlArray& getPoints() const { return points;}
            index_type getPointCount() const { return points.size();}
            const Vector3& getPoint(index_type i) const { return points[i];}

            /** Initializes spline. Don't call other methods while spline not initialized. */
            /// 初始化非循环样条（调用其他求值方法前必须先初始化）
            void init_spline(const Vector3* controls, index_type count, EvaluationMode m);
            /// 初始化循环样条，cyclic_point 为闭合回绕的起始控制点下标
            void init_cyclic_spline(const Vector3* controls, index_type count, EvaluationMode m, index_type cyclic_point);

            /** As i can see there are a lot of ways how spline can be initialized
                would be no harm to have some custom initializers. */
            /// 使用自定义初始化器构造样条控制点
            template<class Init> inline void init_spline(Init& initializer)
            {
                initializer(m_mode, cyclic, points, index_lo, index_hi);
            }

            void clear();

            /** Calculates distance between [i; i+1] points, assumes that index i is in bounds. */
            /// 计算第 i 段（节点 i 到 i+1 之间）的弧长
            float SegLength(index_type i) const { return (this->*seglengths[m_mode])(i);}

            std::string ToString() const;
    };

    /// 带累计弧长/时间戳缓存表的泛型样条类
    template<typename length_type>
    class Spline : public SplineBase
    {
        public:
            typedef length_type LengthType;
            typedef std::vector<length_type> LengthArray;
        protected:

            LengthArray lengths; // 各节点处的前缀累计长度（或累计时间戳）

            /// 根据累计长度查找所属的样条段索引
            index_type computeIndexInBounds(length_type length) const;
        public:

            explicit Spline() {}

            /** Calculates the position for given t
                @param t - percent of spline's length, assumes that t in range [0, 1].
                根据整体样条进度 t ∈ [0, 1] 计算空间位置 */
            void evaluate_percent(float t, Vector3& c) const;

            /** Calculates derivation for given t
                @param t - percent of spline's length, assumes that t in range [0, 1].
                根据整体样条进度 t ∈ [0, 1] 计算一阶导数 */
            void evaluate_derivative(float t, Vector3& hermite) const;

            /** Calculates the position for given segment Idx, and percent of segment length t
                @param t = partial_segment_length / whole_segment_length
                @param Idx - spline segment index, should be in range [first, last). */
            void evaluate_percent(index_type Idx, float u, Vector3& c) const { SplineBase::evaluate_percent(Idx, u, c);}

            /** Caclulates derivation for index Idx, and percent of segment length t
                @param Idx - spline segment index, should be in range [first, last)
                @param t  - percent of spline segment length, assumes that t in range [0, 1]. */
            void evaluate_derivative(index_type Idx, float u, Vector3& c) const { SplineBase::evaluate_derivative(Idx, u, c);}

            // Assumes that t in range [0, 1] (假定 t 在 [0, 1] 范围内，将全局进度转换为段索引与段内比例)
            index_type computeIndexInBounds(float t) const;
            void computeIndex(float t, index_type& out_idx, float& out_u) const;

            /** Initializes spline. Don't call other methods while spline not initialized. */
            void init_spline(const Vector3* controls, index_type count, EvaluationMode m) { SplineBase::init_spline(controls, count, m);}
            void init_cyclic_spline(const Vector3* controls, index_type count, EvaluationMode m, index_type cyclic_point) { SplineBase::init_cyclic_spline(controls, count, m, cyclic_point);}

            /** As i can see there are a lot of ways how spline can be initialized
                would be no harm to have some custom initializers. */
            template<class Init> inline void init_spline_custom(Init& initializer)
            {
                initializer(m_mode, cyclic, points, index_lo, index_hi);
            }

            /**  Initializes lengths with SplineBase::SegLength method. */
            /// 使用几何段长初始化累计长度数组
            void initLengths();

            /** Initializes lengths in some custom way
                Note that value returned by cacher must be greater or equal to previous value.
                使用自定义累加器（如按速度换算时间戳）初始化 lengths，返回值必须单调非递减 */
            template<class T> inline void initLengths(T& cacher)
            {
                index_type i = index_lo;
                lengths.resize(index_hi + 1);
                length_type prev_length = 0, new_length = 0;
                while (i < index_hi)
                {
                    new_length = cacher(*this, i);

                    if (new_length < 0)         // length overflowed, assign to max positive value (stop case only?) (长度溢出时钳位到最大正值)
                        new_length = std::numeric_limits<length_type>::max();

                    lengths[++i] = new_length;

                    MANGOS_ASSERT(prev_length <= new_length);
                    prev_length = new_length;
                }
            }

            /** Returns length of the whole spline. */
            /// 返回整条样条的总长度（或总时长）
            length_type length() const { return lengths[index_hi];}
            /** Returns length between given nodes. */
            /// 返回节点 first 到 last 之间的累计长度差
            length_type length(index_type first, index_type last) const { return lengths[last] - lengths[first];}
            length_type length(index_type Idx) const { return lengths[Idx];}

            void set_length(index_type i, length_type length) { lengths[i] = length;}
            using SplineBase::clear;
            void clear();
    };
}

#include "spline.impl.h"

#endif // MANGOSSERVER_SPLINE_H
