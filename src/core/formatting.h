#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace tm_linux {

/**
 * @brief 字节速率的单位显示模式。
 */
enum class UnitMode {
    Auto, /**< 根据数值自动选择单位。 */
    Kb,   /**< 固定以 KB/s 或 Kb/s 显示。 */
    Mb,   /**< 固定以 MB/s 或 Mb/s 显示。 */
};

/**
 * @brief 速率按字节还是比特换算。
 */
enum class UnitBase {
    Byte, /**< 以字节为单位。 */
    Bit,  /**< 以比特为单位（数值乘以 8）。 */
};

/**
 * @brief 将每秒字节数格式化为带单位的可读速率文本。
 * @param bytes_per_sec 每秒字节数，负值按 0 处理。
 * @param mode 单位模式。
 * @param base 按字节或比特显示。
 * @return 形如 "1.2 MB/s" 的文本。
 */
std::string formatBytesPerSecond(double bytes_per_sec, UnitMode mode, UnitBase base);

/**
 * @brief 将百分比格式化为不带小数位的百分比文本。
 * @param percent 百分比数值，超出 [0, 100] 会被截断。
 * @return 形如 "42%" 的文本。
 */
std::string formatPercent(double percent);

/**
 * @brief 格式化摄氏温度。
 * @param celsius 摄氏度，无值表示传感器不可用。
 * @return 形如 "45.0°C" 的文本；无值时返回 "--"。
 */
std::string formatTemperature(std::optional<double> celsius);

/**
 * @brief 格式化 CPU 频率。
 * @param mhz 频率（MHz）。
 * @return 大于等于 1000 MHz 时以 GHz 显示，否则以 MHz 显示。
 */
std::string formatFrequencyMHz(double mhz);

/**
 * @brief 格式化内存已用量与总量。
 * @param used_bytes 已用字节数。
 * @param total_bytes 总字节数。
 * @return 形如 "3.2 / 15.6 GiB" 的文本，容量不足 1 GiB 时使用 MiB。
 */
std::string formatMemory(std::uint64_t used_bytes, std::uint64_t total_bytes);

/**
 * @brief 将字节数格式化为 GiB/MiB/KiB 文本。
 * @param bytes 字节数。
 * @return 形如 "1.5 GiB" 的文本。
 */
std::string formatBytes(std::uint64_t bytes);

} // namespace tm_linux
