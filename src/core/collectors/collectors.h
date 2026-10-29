#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_COLLECTORS_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_COLLECTORS_H_

#include "pch.h"

#include "common/DTO/metrics.h"
#include "core/collectors/CPU/cpu_collector.h"
#include "core/collectors/CPU/cpu_frequency_collector.h"
#include "core/collectors/disk/disk_collector.h"
#include "core/collectors/memory/memory_collector.h"
#include "core/collectors/network/network_collector.h"

namespace tm_linux {

// ---------------------------------------------------------------------------
// 温度采集（temperature_collector.cpp）
// ---------------------------------------------------------------------------

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

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_COLLECTORS_COLLECTORS_H_
