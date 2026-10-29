#ifndef TRAFFICMONITOR_LINUX_SRC_COMMON_DTO_METRICS_H_
#define TRAFFICMONITOR_LINUX_SRC_COMMON_DTO_METRICS_H_

#include "pch.h"

namespace tm_linux {

/**
 * @brief CPU 总体统计。
 */
struct CpuInfo {
    double usage_percent{};          /**< 整机 CPU 使用率百分比（0–100）。 */
    double frequency_mhz{};          /**< 各核心平均主频，单位 MHz；无法读取时为 0。 */
    std::uint32_t core_count{};      /**< 在线逻辑核心数。 */
};

/**
 * @brief 物理内存与交换分区统计。
 */
struct MemoryInfo {
    std::uint64_t total_bytes{};       /**< 物理内存总量，单位字节。 */
    std::uint64_t available_bytes{};   /**< 可用内存，单位字节。 */
    std::uint64_t used_bytes{};        /**< 已用内存，单位字节。 */
    double used_percent{};             /**< 内存使用率百分比（0–100）。 */
    std::uint64_t swap_total_bytes{};  /**< 交换分区总量，单位字节。 */
    std::uint64_t swap_free_bytes{};   /**< 交换分区剩余空间，单位字节。 */
};

/**
 * @brief 单个文件系统的容量与 IO 统计。
 */
struct DiskInfo {
    std::string mount_point;        /**< 挂载点路径。 */
    std::string device;             /**< 设备名称。 */
    std::uint64_t total_bytes{};    /**< 文件系统容量，单位字节。 */
    std::uint64_t used_bytes{};     /**< 已用空间，单位字节。 */
    double used_percent{};          /**< 空间使用率百分比（0–100）。 */
    double io_util_percent{};       /**< IO 利用率百分比（0–100）。 */
    bool io_available{};            /**< 是否成功采集到 IO 利用率统计。 */
};

/**
 * @brief 各类硬件的温度汇总，单位摄氏度。
 */
struct TemperatureInfo {
    std::optional<double> cpu_c;        /**< CPU 温度；无可用传感器时为空。 */
    std::optional<double> gpu_c;        /**< GPU 温度；无可用传感器时为空。 */
    std::optional<double> hdd_c;        /**< 硬盘温度；无可用传感器时为空。 */
    std::optional<double> mainboard_c;  /**< 主板温度；无可用传感器时为空。 */
};

/**
 * @brief 单个 GPU 的利用率、显存与温度统计。
 */
struct GpuInfo {
    std::string vendor;                              /**< 厂商名称。 */
    std::string name;                                /**< 设备名称。 */
    std::optional<double> utilization_percent;       /**< 利用率百分比；不可用时为空。 */
    std::optional<std::uint64_t> memory_used_bytes;  /**< 已用显存，单位字节；不可用时为空。 */
    std::optional<std::uint64_t> memory_total_bytes; /**< 显存总量，单位字节；不可用时为空。 */
    std::optional<double> temperature_c;             /**< 温度，单位摄氏度；不可用时为空。 */
};

/**
 * @brief 选中网卡的网络统计。
 */
struct NetworkInfo {
    std::string interface;            /**< 网卡名称。 */
    std::uint64_t rx_bytes{};         /**< 累计接收字节数。 */
    std::uint64_t tx_bytes{};         /**< 累计发送字节数。 */
    double rx_bytes_per_sec{};        /**< 接收速率，单位字节/秒。 */
    double tx_bytes_per_sec{};        /**< 发送速率，单位字节/秒。 */
    bool is_loopback{};               /**< 是否为回环接口。 */
    bool is_selected{};               /**< 是否为当前选中的采样接口。 */
};

/**
 * @brief 一次采样得到的完整系统指标快照。
 */
struct Snapshot {
    std::int64_t timestamp_unix_ms{};        /**< 采样时间，Unix 毫秒时间戳。 */
    std::optional<NetworkInfo> network;      /**< 选中网卡统计；无可用网卡时为空。 */
    CpuInfo cpu;                             /**< CPU 统计。 */
    std::optional<MemoryInfo> memory;        /**< 内存统计；读取失败时为空。 */
    std::optional<DiskInfo> disk;            /**< 磁盘统计；读取失败时为空。 */
    TemperatureInfo temperature;             /**< 温度统计。 */
    std::vector<GpuInfo> gpus;               /**< 各 GPU 统计。 */
    std::uint64_t today_up_bytes{};          /**< 当日累计上传字节数。 */
    std::uint64_t today_down_bytes{};        /**< 当日累计下载字节数。 */
    std::uint64_t sample_sequence{};         /**< 采样序号，每次采样递增，可用于判断是否为最新快照。 */
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_COMMON_DTO_METRICS_H_
