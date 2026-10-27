#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tm_linux {

struct CpuInfo {
    double usage_percent{};
    double frequency_mhz{};
    std::uint32_t core_count{};
};

struct MemoryInfo {
    std::uint64_t total_bytes{};
    std::uint64_t available_bytes{};
    std::uint64_t used_bytes{};
    double used_percent{};
    std::uint64_t swap_total_bytes{};
    std::uint64_t swap_free_bytes{};
};

struct DiskInfo {
    std::string mount_point;
    std::string device;
    std::uint64_t total_bytes{};
    std::uint64_t used_bytes{};
    double used_percent{};
    double io_util_percent{};
    bool io_available{};
};

struct TemperatureInfo {
    std::optional<double> cpu_c;
    std::optional<double> gpu_c;
    std::optional<double> hdd_c;
    std::optional<double> mainboard_c;
};

struct GpuInfo {
    std::string vendor;
    std::string name;
    std::optional<double> utilization_percent;
    std::optional<std::uint64_t> memory_used_bytes;
    std::optional<std::uint64_t> memory_total_bytes;
    std::optional<double> temperature_c;
};

struct NetworkInfo {
    std::string interface;
    std::uint64_t rx_bytes{};
    std::uint64_t tx_bytes{};
    double rx_bytes_per_sec{};
    double tx_bytes_per_sec{};
    bool is_loopback{};
    bool is_selected{};
};

struct Snapshot {
    std::int64_t timestamp_unix_ms{};
    std::optional<NetworkInfo> network;
    CpuInfo cpu;
    std::optional<MemoryInfo> memory;
    std::optional<DiskInfo> disk;
    TemperatureInfo temperature;
    std::vector<GpuInfo> gpus;
    std::uint64_t today_up_bytes{};
    std::uint64_t today_down_bytes{};
    std::uint64_t sample_sequence{};
};

} // namespace tm_linux
