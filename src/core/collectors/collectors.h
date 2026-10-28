#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/metrics.h"

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

/**
 * @brief 解析 /proc/meminfo。
 *
 * 要求存在 MemTotal；优先使用 MemAvailable，缺失时回退到
 * MemFree + Buffers + Cached。
 *
 * @param content /proc/meminfo 文件内容。
 * @return 内存信息；缺少 MemTotal 或 MemTotal 为 0 时返回 std::nullopt。
 */
std::optional<MemoryInfo> parseMemInfo(std::string_view content);

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

/**
 * @brief 温度传感器类别。
 */
enum class TemperatureKind {
    Cpu,       /**< CPU 温度。 */
    Gpu,       /**< GPU 温度。 */
    Hdd,       /**< 硬盘温度。 */
    Mainboard, /**< 主板温度。 */
    Unknown,   /**< 无法识别的传感器。 */
};

/**
 * @brief 根据 hwmon 名称判断温度传感器类别。
 * @param name hwmon 目录下 name 文件的内容。
 * @return 温度类别，无法识别时返回 TemperatureKind::Unknown。
 */
TemperatureKind classifyHwmonName(std::string_view name);

/**
 * @brief 解析以毫摄氏度表示的传感器读数。
 * @param text 传感器输入文本。
 * @return 摄氏温度；为空或格式非法时返回 std::nullopt。
 */
std::optional<double> parseMilliCelsius(std::string_view text);

/**
 * @brief 扫描 hwmon 目录树，返回每类传感器的最高温度。
 */
class TemperatureCollector {
public:
    /**
     * @brief 扫描指定 hwmon 根目录。
     * @param root hwmon 根目录（类似 /sys/class/hwmon）。
     * @return 各类温度；目录不可读或传感器缺失时对应项为空。
     */
    TemperatureInfo scan(const std::filesystem::path& root) const;
};

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

/**
 * @brief 单个网络接口的累计收发字节数。
 */
struct NetworkSample {
    std::string interface;
    std::uint64_t rx_bytes{};
    std::uint64_t tx_bytes{};
};

/**
 * @brief 单个网络接口的收发速率。
 */
struct NetworkRate {
    std::optional<double> rx_bytes_per_sec;
    std::optional<double> tx_bytes_per_sec;
};

/**
 * @brief 解析 /proc/net/dev 内容。
 * @param content /proc/net/dev 文件内容。
 * @return 各接口的累计收发字节；表头和格式错误的行会被忽略。
 */
std::vector<NetworkSample> parseNetDev(std::string_view content);

/**
 * @brief 根据相邻 /proc/net/dev 采样计算各接口的字节速率。
 */
class NetworkRateCalculator {
public:
    /**
     * @brief 计算本轮各接口的收发速率。
     * @param samples 当前各接口的累计字节数。
     * @param now 当前单调时钟时间。
     * @return 接口名到速率的映射；首次采样无速率，计数器回退时该周期按 0 处理。
     */
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
