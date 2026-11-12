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

#ifndef MANGOSSERVER_METRIC_H
#define MANGOSSERVER_METRIC_H

#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

#include "Measurement.h"
#include "Common.h"

/// InfluxDB 时序数据库 HTTP 连接配置
struct MetricConnectionInfo
{
    std::string hostname;
    int32 port;
    std::string database;
    std::string username;
    std::string password;
};

namespace metric
{
    /// RAII 作用域指标采集器：构造时记录标签/字段，析构时若条件满足则异步上报
    class measurement
    {
        public:
            measurement(std::string name, std::function<bool()> condition = [] { return true; });
            measurement(std::string name, std::string key, boost::any value, std::function<bool()> condition = [] { return true; });
            measurement(std::string name, std::string key, boost::any value, std::map<std::string, std::string> tags, std::function<bool()> condition = [] { return true; });
            measurement(std::string name, std::map<std::string, std::string> tags, std::function<bool()> condition = [] { return true; });
            /// 析构时检查上报条件，满足则投递至 metric 单例队列
            virtual ~measurement();

            /// 添加索引标签
            void add_tag(std::string key, std::string value);
            /// 添加指标数值字段
            void add_field(std::string key, boost::any value);
            /// 获取已添加的字段值（不存在时返回空 boost::any）
            const boost::any get_field(std::string key);

        protected:
            /// 设置析构时是否上报的判定谓词
            void set_condition(std::function<bool()> condition);

        private:
            std::string m_name;
            std::map<std::string, std::string> m_tags;
            std::map<std::string, boost::any> m_fields;
            std::chrono::system_clock::time_point m_timestamp;
            std::function<bool()> m_condition;
    };

    /// RAII 耗时统计指标类：自动测量作用域执行时间（支持按时间精度模板化及阈值过滤）
    template <class precision>
    class duration : public measurement
    {
        public:
            duration(std::string name)
                : measurement(name), m_startTime(std::chrono::high_resolution_clock::now())
            {}

            duration(std::string name, std::map<std::string, std::string> tags)
                : measurement(name, tags), m_startTime(std::chrono::high_resolution_clock::now())
            {}

            /// 带最小耗时阈值的构造：仅当耗时 >= threshold 时才触发上报
            duration(std::string name, std::map<std::string, std::string> tags, int64 threshold)
                : measurement(name, tags), m_startTime(std::chrono::high_resolution_clock::now())
            {
                auto condition = [&, threshold] {
                    auto value = get_field("duration");
                    if (value.empty())
                        return false;

                    auto duration = boost::any_cast<int64>(value);
                    return duration >= threshold;
                };

                set_condition(std::move(condition));
            }

            ~duration() override
            {
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<precision>(endTime - m_startTime).count();

                add_field("duration", static_cast<int64>(duration));
            }

        private:
            std::chrono::high_resolution_clock::time_point m_startTime;
    };

    /// 性能指标异步采集与批量上报管理器（通过双 io_context 线程周期性推送至 InfluxDB）
    class metric
    {
        public:
            metric();
            ~metric();

            /// 读取 Metric.* 配置并启动后台采集/发送工作线程
            void initialize();
            /// 获取全局唯一 metric 单例
            static metric& instance();

            /// 热重载 InfluxDB 连接配置
            void reload_config();

            /// 上报单字段指标
            void report(std::string measurement, std::string key, boost::any value, std::map<std::string, std::string> tags = {});
            /// 上报多字段指标
            void report(std::string measurement, std::map<std::string, boost::any> fields, std::map<std::string, std::string> tags = {});

        private:
            boost::asio::io_context m_queueContext;     ///< 入队线程上下文
            boost::asio::io_context m_writeContext;     ///< 网络写线程上下文

            std::unique_ptr<boost::asio::system_timer> m_sendTimer;
            std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_queueContextWork;
            std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> m_writeContextWork;
            std::thread m_queueServiceThread;
            std::thread m_writeServiceThread;

            bool m_enabled;                             ///< 是否启用性能指标采集
            MetricConnectionInfo m_connectionInfo;

            std::mutex m_queueWriteLock;
            std::vector<std::unique_ptr<Measurement>> m_measurementQueue;

            /// 调度下一次 1 秒周期发送定时器
            void schedule_timer();
            /// 定时器到期回调：触发批量发送并重新调度
            void prepare_send(const boost::system::error_code& ec);
            /// 将缓存队列中的指标通过 HTTP POST 批量写入 InfluxDB
            void send();
    };
}

#endif // MANGOSSERVER_METRIC_H
