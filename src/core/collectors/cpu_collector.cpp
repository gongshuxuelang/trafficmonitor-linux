#include "core/collectors/collectors.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string>

namespace tm_linux {

namespace {

/**
 * @brief 计算 CPU 各时间计数之和。
 * @param times CPU 时间计数。
 * @return 总时间。
 */
std::uint64_t totalTime(const CpuTimes& times)
{
    return times.user + times.nice + times.system + times.idle + times.iowait + times.irq
        + times.softirq + times.steal;
}

/**
 * @brief 计算 CPU 空闲时间（idle + iowait）。
 * @param times CPU 时间计数。
 * @return 空闲时间。
 */
std::uint64_t idleTime(const CpuTimes& times)
{
    return times.idle + times.iowait;
}

/**
 * @brief 解析无符号整数文本。
 * @param value 输入文本。
 * @param output 解析结果输出。
 * @return 解析成功且无剩余字符时返回 true。
 */
bool parseUnsigned(std::string_view value, std::uint64_t& output)
{
    if (value.empty()) {
        return false;
    }
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

} // namespace

std::optional<CpuTimes> parseProcStat(std::string_view content)
{
    std::size_t position = 0;
    while (position < content.size()) {
        const std::size_t line_end = content.find('\n', position);
        const std::size_t end = line_end == std::string_view::npos ? content.size() : line_end;
        const std::string_view line = content.substr(position, end - position);
        position = end == content.size() ? content.size() : end + 1;

        if (line.size() < 4 || line.substr(0, 3) != "cpu" || line[3] != ' ') {
            continue;
        }

        std::string_view fields[8]{};
        std::size_t field_count = 0;
        std::size_t cursor = 4;
        while (cursor < line.size() && field_count < 8) {
            while (cursor < line.size()
                   && std::isspace(static_cast<unsigned char>(line[cursor]))) {
                ++cursor;
            }
            if (cursor >= line.size()) {
                break;
            }
            const std::size_t start = cursor;
            while (cursor < line.size()
                   && !std::isspace(static_cast<unsigned char>(line[cursor]))) {
                ++cursor;
            }
            fields[field_count++] = line.substr(start, cursor - start);
        }

        if (field_count < 4) {
            return std::nullopt;
        }

        std::uint64_t values[8]{};
        for (std::size_t i = 0; i < field_count; ++i) {
            if (!parseUnsigned(fields[i], values[i])) {
                return std::nullopt;
            }
        }

        CpuTimes times;
        times.user = values[0];
        times.nice = values[1];
        times.system = values[2];
        times.idle = values[3];
        times.iowait = values[4];
        times.irq = values[5];
        times.softirq = values[6];
        times.steal = values[7];
        return times;
    }

    return std::nullopt;
}

std::optional<double> computeCpuUsagePercent(const CpuTimes& previous, const CpuTimes& current)
{
    const std::uint64_t previous_total = totalTime(previous);
    const std::uint64_t current_total = totalTime(current);
    const std::uint64_t previous_idle = idleTime(previous);
    const std::uint64_t current_idle = idleTime(current);

    if (current_total <= previous_total || current_idle < previous_idle) {
        return std::nullopt;
    }

    const std::uint64_t total_delta = current_total - previous_total;
    const std::uint64_t idle_delta = current_idle - previous_idle;
    if (idle_delta > total_delta) {
        return std::nullopt;
    }

    const double usage = 100.0 * static_cast<double>(total_delta - idle_delta)
        / static_cast<double>(total_delta);
    return std::clamp(usage, 0.0, 100.0);
}

} // namespace tm_linux
