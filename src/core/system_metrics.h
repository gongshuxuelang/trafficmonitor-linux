#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "core/collectors/collectors.h"
#include "core/metrics.h"

namespace tm_linux {

/**
 * @brief 汇总采集网络、CPU、内存、磁盘和温度指标，生成一次快照。
 */
class SystemMetricsCollector {
public:
    /**
     * @brief 创建采集器。
     * @param proc_root procfs 根目录，测试时可替换。
     * @param sys_root sysfs 根目录，测试时可替换。
     */
    explicit SystemMetricsCollector(std::filesystem::path proc_root = "/proc",
                                    std::filesystem::path sys_root = "/sys");

    /**
     * @brief 设置要采集的网卡。
     * @param interface 网卡名；"auto" 表示自动选择默认路由网卡。
     */
    void setNetworkInterface(std::string interface);

    /**
     * @brief 设置磁盘使用率统计的挂载点。
     * @param mount_point 挂载路径。
     */
    void setDiskMountPoint(std::string mount_point);

    /**
     * @brief 设置今日流量的初始累计值（通常从历史库恢复）。
     * @param up_bytes 今日已上传字节数。
     * @param down_bytes 今日已下载字节数。
     */
    void setTodayTotals(std::uint64_t up_bytes, std::uint64_t down_bytes);

    /**
     * @brief 采集一次系统指标快照。
     * @return 最新快照；无法获取的指标留空或保持默认值。
     */
    Snapshot sample();

private:
    std::filesystem::path proc_root_;
    std::filesystem::path sys_root_;
    std::string network_interface_{"auto"};
    std::string disk_mount_point_{"/"};
    NetworkRateCalculator network_rates_;
    DiskIoRateCalculator disk_rates_;
    std::unordered_map<std::string, NetworkSample> previous_network_;
    std::chrono::steady_clock::time_point previous_network_time_{};
    std::optional<CpuTimes> previous_cpu_;
    std::uint64_t today_up_bytes_{};
    std::uint64_t today_down_bytes_{};
    std::uint64_t sequence_{};
};

} // namespace tm_linux
