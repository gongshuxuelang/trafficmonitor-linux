#include "tm/formatting.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tm_linux {

namespace {

std::string formatWithUnit(double value, const char* unit, int decimals)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f %s", decimals, value, unit);
    return buffer;
}

} // namespace

std::string formatBytesPerSecond(double bytes_per_sec, UnitMode mode, UnitBase base)
{
    const double multiplier = base == UnitBase::Bit ? 8.0 : 1.0;
    const double value = std::max(0.0, bytes_per_sec) * multiplier;
    const bool bits = base == UnitBase::Bit;

    switch (mode) {
    case UnitMode::Kb:
        return formatWithUnit(value / 1024.0, bits ? "Kb/s" : "KB/s", 1);
    case UnitMode::Mb:
        return formatWithUnit(value / (1024.0 * 1024.0), bits ? "Mb/s" : "MB/s", 2);
    case UnitMode::Auto:
        break;
    }

    if (value < 1024.0) {
        return formatWithUnit(value, bits ? "b/s" : "B/s", 0);
    }
    if (value < 1024.0 * 1024.0) {
        return formatWithUnit(value / 1024.0, bits ? "Kb/s" : "KB/s", 1);
    }
    if (value < 1024.0 * 1024.0 * 1024.0) {
        return formatWithUnit(value / (1024.0 * 1024.0), bits ? "Mb/s" : "MB/s", 1);
    }
    return formatWithUnit(value / (1024.0 * 1024.0 * 1024.0), bits ? "Gb/s" : "GB/s", 1);
}

std::string formatPercent(double percent)
{
    const double clamped = std::clamp(percent, 0.0, 100.0);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", clamped);
    return buffer;
}

std::string formatTemperature(std::optional<double> celsius)
{
    if (!celsius.has_value()) {
        return "--";
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f°C", celsius.value());
    return buffer;
}

std::string formatFrequencyMHz(double mhz)
{
    if (mhz >= 1000.0) {
        return formatWithUnit(mhz / 1000.0, "GHz", 2);
    }
    return formatWithUnit(mhz, "MHz", 0);
}

std::string formatMemory(std::uint64_t used_bytes, std::uint64_t total_bytes)
{
    const double gib = 1024.0 * 1024.0 * 1024.0;
    if (static_cast<double>(total_bytes) >= gib) {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.1f / %.1f GiB",
                      static_cast<double>(used_bytes) / gib,
                      static_cast<double>(total_bytes) / gib);
        return buffer;
    }

    const double mib = 1024.0 * 1024.0;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.1f / %.1f MiB",
                  static_cast<double>(used_bytes) / mib,
                  static_cast<double>(total_bytes) / mib);
    return buffer;
}

std::string formatBytes(std::uint64_t bytes)
{
    const double gib = 1024.0 * 1024.0 * 1024.0;
    if (static_cast<double>(bytes) >= gib) {
        return formatWithUnit(static_cast<double>(bytes) / gib, "GiB", 1);
    }
    const double mib = 1024.0 * 1024.0;
    if (static_cast<double>(bytes) >= mib) {
        return formatWithUnit(static_cast<double>(bytes) / mib, "MiB", 1);
    }
    return formatWithUnit(static_cast<double>(bytes) / 1024.0, "KiB", 1);
}

} // namespace tm_linux
