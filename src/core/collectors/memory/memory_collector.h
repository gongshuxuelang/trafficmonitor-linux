#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_MEMORY_MEMORY_COLLECTOR_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_MEMORY_MEMORY_COLLECTOR_H_

#include "pch.h"

#include "common/DTO/metrics.h"

namespace tm_linux {

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

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_MEMORY_MEMORY_COLLECTOR_H_
