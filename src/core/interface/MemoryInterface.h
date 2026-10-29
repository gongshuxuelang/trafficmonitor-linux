#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_MEMORYINTERFACE_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_MEMORYINTERFACE_H_

#include "pch.h"

namespace tm_linux {

/**
 * @brief 内存采集的通用抽象接口。
 *
 * 平台相关的内存采集实现继承本接口并实现各纯虚函数，向上层提供统一的
 * 物理内存与交换分区查询能力。上层代码只依赖本接口，不依赖具体实现，
 * 便于替换实现或在测试中注入桩对象。
 */
class MemoryInterface {
public:
    /**
     * @brief 虚析构函数，保证通过基类指针析构派生类对象时行为正确。
     */
    virtual ~MemoryInterface() = default;

    /**
     * @brief 查询物理内存总量。
     * @return 物理内存总量，单位字节；无法读取时返回 std::nullopt。
     */
    virtual std::optional<std::uint64_t> totalBytes() const = 0;

    /**
     * @brief 查询可用物理内存。
     * @return 可用内存，单位字节；无法读取时返回 std::nullopt。
     */
    virtual std::optional<std::uint64_t> availableBytes() const = 0;

    /**
     * @brief 查询已用物理内存。
     * @return 已用内存，单位字节；无法读取时返回 std::nullopt。
     */
    virtual std::optional<std::uint64_t> usedBytes() const = 0;

    /**
     * @brief 查询物理内存使用率。
     * @return 使用率百分比 [0, 100]；无法读取时返回 std::nullopt。
     */
    virtual std::optional<double> usedPercent() const = 0;

    /**
     * @brief 查询交换分区总量。
     * @return 交换分区总量，单位字节；无法读取时返回 std::nullopt。
     */
    virtual std::optional<std::uint64_t> swapTotalBytes() const = 0;

    /**
     * @brief 查询交换分区剩余空间。
     * @return 交换分区剩余空间，单位字节；无法读取时返回 std::nullopt。
     */
    virtual std::optional<std::uint64_t> swapFreeBytes() const = 0;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_MEMORYINTERFACE_H_
