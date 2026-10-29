#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_DISK_DISK_COLLECTOR_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_DISK_DISK_COLLECTOR_H_

#include "pch.h"

namespace tm_linux {

/**
 * @brief 单个磁盘设备的 I/O 统计采样。
 */
struct DiskStatsSample {
    std::uint64_t io_ticks_ms{};
};

/**
 * @brief 解析 /proc/diskstats，以设备名作为键。
 * @param content /proc/diskstats 文件内容。
 * @return 设备名到 I/O 统计采样的映射。
 */
std::unordered_map<std::string, DiskStatsSample> parseDiskStats(std::string_view content);

/**
 * @brief 判断设备名是否为物理磁盘。
 * @param name 设备名。
 * @return 排除 loop、ram、zram 和 dm- 虚拟设备后返回 true。
 */
bool isPhysicalDiskName(std::string_view name);

/**
 * @brief 根据相邻 io_ticks 采样计算各设备的 I/O 利用率。
 */
class DiskIoRateCalculator {
public:
    /**
     * @brief 计算本轮各设备的 I/O 利用率。
     * @param samples 设备名到当前 io_ticks 的映射。
     * @param now 当前单调时钟时间。
     * @return 设备名到利用率百分比 [0, 100] 的映射；首次采样无速率，
     *         计数器回退时按 0% 处理。
     */
    std::unordered_map<std::string, double> update(
        const std::unordered_map<std::string, DiskStatsSample>& samples,
        std::chrono::steady_clock::time_point now);

private:
    struct Previous {
        std::uint64_t io_ticks_ms{};
        std::chrono::steady_clock::time_point time{};
    };

    std::unordered_map<std::string, Previous> previous_;
};

/**
 * @brief 计算磁盘使用率。
 * @param total_bytes 总容量（字节）。
 * @param available_bytes 可用容量（字节），超过总量时按总量计。
 * @return 使用率百分比 [0, 100]；总量为 0 时返回 0。
 */
double computeDiskUsagePercent(std::uint64_t total_bytes, std::uint64_t available_bytes);

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_DISK_DISK_COLLECTOR_H_
