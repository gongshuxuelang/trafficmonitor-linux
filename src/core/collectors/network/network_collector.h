#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_NETWORK_NETWORK_COLLECTOR_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_NETWORK_NETWORK_COLLECTOR_H_

#include "pch.h"

namespace tm_linux {

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

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_NETWORK_NETWORK_COLLECTOR_H_
