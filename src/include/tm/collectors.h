#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "tm/metrics.h"

namespace tm_linux {

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

// Parses the aggregate "cpu" line from /proc/stat.
std::optional<CpuTimes> parseProcStat(std::string_view content);

// Returns the CPU usage percentage between two samples. Returns nullopt when
// the total time delta is zero or a counter went backwards.
std::optional<double> computeCpuUsagePercent(const CpuTimes& previous, const CpuTimes& current);

// Parses /proc/meminfo. Requires MemTotal. Uses MemAvailable when present and
// falls back to MemFree + Buffers + Cached otherwise.
std::optional<MemoryInfo> parseMemInfo(std::string_view content);

struct DiskStatsSample {
    std::uint64_t io_ticks_ms{};
};

// Parses /proc/diskstats. Device names are used as keys.
std::unordered_map<std::string, DiskStatsSample> parseDiskStats(std::string_view content);

// Excludes loop, ram and device-mapper virtual devices.
bool isPhysicalDiskName(std::string_view name);

// Computes I/O utilization percentages from successive io_ticks samples.
// The first sample for a device has no rate. Counter resets yield 0%.
class DiskIoRateCalculator {
public:
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

double computeDiskUsagePercent(std::uint64_t total_bytes, std::uint64_t available_bytes);

enum class TemperatureKind {
    Cpu,
    Gpu,
    Hdd,
    Mainboard,
    Unknown,
};

TemperatureKind classifyHwmonName(std::string_view name);
std::optional<double> parseMilliCelsius(std::string_view text);

// Scans a /sys/class/hwmon-like directory tree and returns the maximum
// temperature per category. Missing or unreadable sensors are left empty.
class TemperatureCollector {
public:
    TemperatureInfo scan(const std::filesystem::path& root) const;
};

std::optional<std::uint64_t> parseKilohertz(std::string_view text);
double averageFrequencyMHz(const std::vector<std::uint64_t>& khz_values);

// Scans a /sys/devices/system/cpu-like tree for cpu*/cpufreq/scaling_cur_freq.
class CpuFrequencyCollector {
public:
    std::optional<double> scan(const std::filesystem::path& cpu_root) const;
};

// Fallback parser for "cpu MHz" lines in /proc/cpuinfo.
std::optional<double> parseCpuInfoAverageMHz(std::string_view content);

struct NetworkSample {
    std::string interface;
    std::uint64_t rx_bytes{};
    std::uint64_t tx_bytes{};
};

struct NetworkRate {
    std::optional<double> rx_bytes_per_sec;
    std::optional<double> tx_bytes_per_sec;
};

// Parses /proc/net/dev content. Header lines and malformed lines are ignored.
std::vector<NetworkSample> parseNetDev(std::string_view content);

// Computes per-interface byte rates from successive /proc/net/dev samples.
// The first sample for an interface has no rate. A counter reset (current
// value below the previous value) yields a zero rate for that cycle.
class NetworkRateCalculator {
public:
    std::unordered_map<std::string, NetworkRate> update(
        const std::vector<NetworkSample>& samples,
        std::chrono::steady_clock::time_point now);

private:
    struct Previous {
        std::uint64_t rx_bytes{};
        std::uint64_t tx_bytes{};
        std::chrono::steady_clock::time_point time{};
    };

    std::unordered_map<std::string, Previous> previous_;
};

} // namespace tm_linux
