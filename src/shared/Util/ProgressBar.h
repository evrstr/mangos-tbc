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

#ifndef MANGOSSERVER_PROGRESSBAR_H
#define MANGOSSERVER_PROGRESSBAR_H

#include "Platform/Define.h"

/// 控制台文本进度条显示类（用于启动加载数据库/地图时展示进度百分比）
class BarGoLink
{
    public:                                                 // constructors | 构造函数与析构函数
        explicit BarGoLink(size_t row_count);
        ~BarGoLink();

    public:                                                 // modifiers | 进度推进与全局显示开关控制
        /// 前进一步记录数，达到新刻度时刷新控制台进度条
        void step();

        /// 全局开启或关闭进度条控制台输出
        static void SetOutputState(bool on);
    private:
        void init(size_t row_count);

        static bool m_showOutput;                           // not recommended change with existed active bar | 是否输出进度条（存在活跃进度条时不建议动态切换）
        static char const* const empty;
        static char const* const full;

        size_t rec_no;
        size_t rec_pos;
        size_t num_rec;
        size_t indic_len;
};
#endif
