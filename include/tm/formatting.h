#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace tm_linux {

enum class UnitMode {
    Auto,
    Kb,
    Mb,
};

enum class UnitBase {
    Byte,
    Bit,
};

std::string formatBytesPerSecond(double bytes_per_sec, UnitMode mode, UnitBase base);
std::string formatPercent(double percent);
std::string formatTemperature(std::optional<double> celsius);
std::string formatFrequencyMHz(double mhz);
std::string formatMemory(std::uint64_t used_bytes, std::uint64_t total_bytes);
std::string formatBytes(std::uint64_t bytes);

} // namespace tm_linux
