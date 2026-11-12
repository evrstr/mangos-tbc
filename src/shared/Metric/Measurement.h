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

#ifndef MANGOSSERVER_MEASUREMENT_H
#define MANGOSSERVER_MEASUREMENT_H

#include <boost/any.hpp>
#include <iostream>
#include <map>

#include "Common.h"

/// InfluxDB 指标采样点数据结构（包含指标名、标签、字段及纳秒时间戳）
struct Measurement
{
    /// 构造一个指标采样点；若 timestamp 为 0 则自动取当前系统纳秒时间戳
    Measurement(std::string measurement, std::map<std::string, std::string> tags, std::map<std::string, boost::any> fields, uint64 timestamp = 0);

    /// 序列化为 InfluxDB Line Protocol 行文本格式
    friend std::stringstream& operator<<(std::stringstream &out, const Measurement &measurement);

    std::string _measurement;                   ///< 指标名称
    std::map<std::string, std::string> _tags;   // tags are used for selecting queries | 标签键值对（用于索引与查询过滤）
    std::map<std::string, boost::any> _fields;  // fields are used for displaying data | 字段键值对（用于存储与展示具体指标数值）
    uint64 _timestamp;                          ///< 纳秒级 UNIX 时间戳
};

#endif // MANGOSSERVER_MEASUREMENT_H