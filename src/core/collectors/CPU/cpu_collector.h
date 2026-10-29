#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_CPU_CPU_COLLECTOR_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_CPU_CPU_COLLECTOR_H_

#include "pch.h"

namespace tm_linux {

/**
 * @brief /proc/stat 中汇总 "cpu" 行的各时间计数。
 */
struct CpuTimes {
    std::uint64_t user{};
    std::uint64_t nice{};
    std::uint64_t system{};
    std::uint64_t idle{};
    std::uint64_t iowait{};
    std::uint64_t irq{};
    std::uint64_t softirq{};
    std::uint64_t steal{};
};

/**
 * @brief 解析 /proc/stat 中的汇总 "cpu" 行。
 * @param content /proc/stat 文件内容。
 * @return 各 CPU 时间计数；找不到 cpu 行或格式错误时返回 std::nullopt。
 */
std::optional<CpuTimes> parseProcStat(std::string_view content);

/**
 * @brief 计算两次采样之间的 CPU 使用率。
 * @param previous 上一次采样。
 * @param current 当前采样。
 * @return 使用率百分比 [0, 100]；总时间差为零或计数器回退时返回 std::nullopt。
 */
std::optional<double> computeCpuUsagePercent(const CpuTimes& previous, const CpuTimes& current);

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_CPU_CPU_COLLECTOR_H_
