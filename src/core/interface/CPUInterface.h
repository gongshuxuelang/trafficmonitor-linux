#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_CPUINTERFACE_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_CPUINTERFACE_H_

#include "pch.h"

namespace tm_linux {

/**
 * @brief CPU 采集的通用抽象接口。
 *
 * 平台相关的 CPU 采集实现继承本接口并实现各纯虚函数，向上层提供统一的
 * 使用率、主频与核心数查询能力。上层代码只依赖本接口，不依赖具体实现，
 * 便于替换实现或在测试中注入桩对象。
 */
class CPUInterface {
public:
    /**
     * @brief 虚析构函数，保证通过基类指针析构派生类对象时行为正确。
     */
    virtual ~CPUInterface() = default;

    /**
     * @brief 采集一次 CPU 总体使用率。
     * @return 使用率百分比 [0, 100]；首次调用（无历史采样）或无法采样时
     *         返回 std::nullopt。
     */
    virtual std::optional<double> usagePercent() = 0;

    /**
     * @brief 查询 CPU 当前平均主频。
     * @return 各核心平均主频，单位 MHz；无法读取时返回 std::nullopt。
     */
    virtual std::optional<double> frequencyMHz() const = 0;

    /**
     * @brief 查询在线逻辑核心数。
     * @return 在线逻辑核心数；无法读取时返回 0。
     */
    virtual std::uint32_t coreCount() const = 0;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_CPUINTERFACE_H_
