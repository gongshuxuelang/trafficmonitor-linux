#include "tm/collectors.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string>

namespace tm_linux {

namespace {

bool parseUnsigned(std::string_view value, std::uint64_t& output)
{
    if (value.empty()) {
        return false;
    }
    const auto result = std::from_chars(value.data(), value.data() + value.size(), output);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

} // namespace

std::unordered_map<std::string, DiskStatsSample> parseDiskStats(std::string_view content)
{
    std::unordered_map<std::string, DiskStatsSample> stats;

    std::size_t position = 0;
    while (position < content.size()) {
        const std::size_t line_end = content.find('\n', position);
        const std::size_t end = line_end == std::string_view::npos ? content.size() : line_end;
        const std::string_view line = content.substr(position, end - position);
        position = end == content.size() ? content.size() : end + 1;

        std::string_view fields[13]{};
        std::size_t field_count = 0;
        std::size_t cursor = 0;
        while (cursor < line.size() && field_count < 13) {
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

        if (field_count < 13) {
            continue;
        }

        std::uint64_t io_ticks = 0;
        if (!parseUnsigned(fields[12], io_ticks)) {
            continue;
        }

        stats[std::string(fields[2])] = DiskStatsSample{io_ticks};
    }

    return stats;
}

bool isPhysicalDiskName(std::string_view name)
{
    return !(name.starts_with("loop") || name.starts_with("ram") || name.starts_with("zram")
             || name.starts_with("dm-"));
}

std::unordered_map<std::string, double> DiskIoRateCalculator::update(
    const std::unordered_map<std::string, DiskStatsSample>& samples,
    std::chrono::steady_clock::time_point now)
{
    std::unordered_map<std::string, double> rates;
    rates.reserve(samples.size());

    for (const auto& [name, sample] : samples) {
        const auto previous = previous_.find(name);
        if (previous != previous_.end()) {
            const double elapsed_ms =
                std::chrono::duration<double, std::milli>(now - previous->second.time).count();
            if (elapsed_ms > 0.0) {
                const std::uint64_t delta = sample.io_ticks_ms >= previous->second.io_ticks_ms
                    ? sample.io_ticks_ms - previous->second.io_ticks_ms
                    : 0;
                const double utilization = 100.0 * static_cast<double>(delta) / elapsed_ms;
                rates[name] = std::clamp(utilization, 0.0, 100.0);
            }
        }

        previous_[name] = Previous{sample.io_ticks_ms, now};
    }

    return rates;
}

double computeDiskUsagePercent(std::uint64_t total_bytes, std::uint64_t available_bytes)
{
    if (total_bytes == 0) {
        return 0.0;
    }
    const std::uint64_t available = std::min(available_bytes, total_bytes);
    const std::uint64_t used = total_bytes - available;
    return 100.0 * static_cast<double>(used) / static_cast<double>(total_bytes);
}

} // namespace tm_linux
