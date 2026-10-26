#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "tm/collectors.h"
#include "tm/metrics.h"

namespace tm_linux {

class SystemMetricsCollector {
public:
    explicit SystemMetricsCollector(std::filesystem::path proc_root = "/proc",
                                    std::filesystem::path sys_root = "/sys");

    void setNetworkInterface(std::string interface);
    void setDiskMountPoint(std::string mount_point);
    void setTodayTotals(std::uint64_t up_bytes, std::uint64_t down_bytes);

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
