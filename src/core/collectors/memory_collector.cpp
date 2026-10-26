#include "tm/collectors.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <string>
#include <unordered_map>

namespace tm_linux {

namespace {

std::string_view trim(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

std::uint64_t kilobytesToBytes(std::uint64_t kilobytes)
{
    return kilobytes * 1024;
}

} // namespace

std::optional<MemoryInfo> parseMemInfo(std::string_view content)
{
    std::unordered_map<std::string, std::uint64_t> values;

    std::size_t position = 0;
    while (position < content.size()) {
        const std::size_t line_end = content.find('\n', position);
        const std::size_t end = line_end == std::string_view::npos ? content.size() : line_end;
        const std::string_view line = content.substr(position, end - position);
        position = end == content.size() ? content.size() : end + 1;

        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim(line.substr(0, colon));
        std::string_view rest = trim(line.substr(colon + 1));
        const std::size_t space = rest.find(' ');
        const std::string_view number = space == std::string_view::npos ? rest : rest.substr(0, space);

        std::uint64_t value = 0;
        const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
        if (result.ec == std::errc{} && result.ptr == number.data() + number.size()) {
            values[std::string(key)] = value;
        }
    }

    const auto total = values.find("MemTotal");
    if (total == values.end() || total->second == 0) {
        return std::nullopt;
    }

    const std::uint64_t total_bytes = kilobytesToBytes(total->second);
    std::uint64_t available_kb = 0;
    const auto available = values.find("MemAvailable");
    if (available != values.end()) {
        available_kb = available->second;
    } else {
        available_kb = values["MemFree"] + values["Buffers"] + values["Cached"];
    }

    available_kb = std::min(available_kb, total->second);
    const std::uint64_t available_bytes = kilobytesToBytes(available_kb);
    const std::uint64_t used_bytes = total_bytes - available_bytes;

    MemoryInfo info;
    info.total_bytes = total_bytes;
    info.available_bytes = available_bytes;
    info.used_bytes = used_bytes;
    info.used_percent = 100.0 * static_cast<double>(used_bytes) / static_cast<double>(total_bytes);
    info.swap_total_bytes = kilobytesToBytes(values["SwapTotal"]);
    info.swap_free_bytes = kilobytesToBytes(values["SwapFree"]);
    return info;
}

} // namespace tm_linux
