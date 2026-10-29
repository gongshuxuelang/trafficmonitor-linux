#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_DISKINTERFACE_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_DISKINTERFACE_H_

#include "pch.h"

namespace tm_linux {

/**
 * @brief 磁盘采集的通用抽象接口。
 *
 * 平台相关的磁盘采集实现继承本接口并实现各纯虚函数，向上层提供统一的
 * 容量、使用率与 I/O 利用率查询能力。上层代码只依赖本接口，不依赖具体
 * 实现，便于替换实现或在测试中注入桩对象。
 */
class DiskInterface {
public:
    /**
     * @brief 虚析构函数，保证通过基类指针析构派生类对象时行为正确。
     */
    virtual ~DiskInterface() = default;

    /**
     * @brief 查询文件系统挂载点。
     * @return 挂载点路径，例如 "/"。
     */
    virtual std::string mountPoint() const = 0;

    /**
     * @brief 查询文件系统对应的设备名称。
     * @return 设备名称，例如 "/dev/sda1"。
     */
    virtual std::string device() const = 0;

    /**
     * @brief 查询文件系统总容量。
     * @return 总容量，单位字节；无法读取时返回 std::nullopt。
     */
    virtual std::optional<std::uint64_t> totalBytes() const = 0;

    /**
     * @brief 查询文件系统已用空间。
     * @return 已用空间，单位字节；无法读取时返回 std::nullopt。
     */
    virtual std::optional<std::uint64_t> usedBytes() const = 0;

    /**
     * @brief 查询文件系统空间使用率。
     * @return 使用率百分比 [0, 100]；无法读取时返回 std::nullopt。
     */
    virtual std::optional<double> usedPercent() const = 0;

    /**
     * @brief 采集一次磁盘 I/O 利用率。
     *
     * 利用率依赖相邻两次 io_ticks 采样，首次调用通常没有历史数据。
     *
     * @return 利用率百分比 [0, 100]；首次调用或无法采集时返回 std::nullopt。
     */
    virtual std::optional<double> ioUtilPercent() = 0;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_INTERFACE_DISKINTERFACE_H_
