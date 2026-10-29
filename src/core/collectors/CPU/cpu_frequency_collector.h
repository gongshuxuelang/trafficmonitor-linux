#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_CPU_CPU_FREQUENCY_COLLECTOR_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_CPU_CPU_FREQUENCY_COLLECTOR_H_

#include "pch.h"

namespace tm_linux {

/**
 * @brief 解析 kHz 整数文本。
 * @param text 频率文本。
 * @return 频率（kHz）；为空或格式非法时返回 std::nullopt。
 */
std::optional<std::uint64_t> parseKilohertz(std::string_view text);

/**
 * @brief 计算各核心频率的平均值。
 * @param khz_values 各核心频率（kHz）。
 * @return 平均频率（MHz）；列表为空时返回 0。
 */
double averageFrequencyMHz(const std::vector<std::uint64_t>& khz_values);

/**
 * @brief 扫描 CPU 目录树中的 cpufreq/scaling_cur_freq 获取当前频率。
 */
class CpuFrequencyCollector {
public:
    /**
     * @brief 扫描指定 CPU 根目录。
     * @param cpu_root CPU 目录根（类似 /sys/devices/system/cpu）。
     * @return 平均频率（MHz）；目录不可读或无有效数据时返回 std::nullopt。
     */
    std::optional<double> scan(const std::filesystem::path& cpu_root) const;
};

/**
 * @brief 解析 /proc/cpuinfo 中 "cpu MHz" 行的平均值（频率降级方案）。
 * @param content /proc/cpuinfo 文件内容。
 * @return 平均频率（MHz）；没有匹配行时返回 std::nullopt。
 */
std::optional<double> parseCpuInfoAverageMHz(std::string_view content);

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_CPU_CPU_FREQUENCY_COLLECTOR_H_
