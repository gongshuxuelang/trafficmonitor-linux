#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_NETWORKINTERFACE_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_NETWORKINTERFACE_H_

#include "pch.h"

namespace tm_linux {

/**
 * @brief 网络采集的通用抽象接口。
 *
 * 平台相关的网络采集实现继承本接口并实现各纯虚函数，向上层提供统一的
 * 网卡累计流量与实时速率查询能力。上层代码只依赖本接口，不依赖具体
 * 实现，便于替换实现或在测试中注入桩对象。
 */
class NetworkInterface {
public:
    /**
     * @brief 虚析构函数，保证通过基类指针析构派生类对象时行为正确。
     */
    virtual ~NetworkInterface() = default;

    /**
     * @brief 查询网卡名称。
     * @return 网卡名称，例如 "eth0"。
     */
    virtual std::string interfaceName() const = 0;

    /**
     * @brief 查询累计接收字节数。
     * @return 累计接收字节数；无法读取时返回 std::nullopt。
     */
    virtual std::optional<std::uint64_t> rxBytes() const = 0;

    /**
     * @brief 查询累计发送字节数。
     * @return 累计发送字节数；无法读取时返回 std::nullopt。
     */
    virtual std::optional<std::uint64_t> txBytes() const = 0;

    /**
     * @brief 采集一次接收速率。
     *
     * 速率依赖相邻两次累计字节采样，首次调用通常没有历史数据。
     *
     * @return 接收速率，单位字节/秒；首次调用或无法采集时返回 std::nullopt。
     */
    virtual std::optional<double> rxBytesPerSec() = 0;

    /**
     * @brief 采集一次发送速率。
     *
     * 速率依赖相邻两次累计字节采样，首次调用通常没有历史数据。
     *
     * @return 发送速率，单位字节/秒；首次调用或无法采集时返回 std::nullopt。
     */
    virtual std::optional<double> txBytesPerSec() = 0;

    /**
     * @brief 判断网卡是否为回环接口。
     * @return 回环接口返回 true，否则返回 false。
     */
    virtual bool isLoopback() const = 0;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_NETWORKINTERFACE_H_
