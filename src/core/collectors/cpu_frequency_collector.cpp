#include "tm/collectors.h"

#include <charconv>
#include <cctype>
#include <fstream>
#include <string>
#include <system_error>

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

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream) {
        return {};
    }
    std::string contents;
    std::getline(stream, contents);
    return contents;
}

bool isCpuDirectoryName(std::string_view name)
{
    if (!name.starts_with("cpu") || name.size() <= 3) {
        return false;
    }
    for (std::size_t i = 3; i < name.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<std::uint64_t> parseKilohertz(std::string_view text)
{
    const std::string_view trimmed = trim(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::uint64_t value = 0;
    const auto result = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
    if (result.ec != std::errc{} || result.ptr != trimmed.data() + trimmed.size()) {
        return std::nullopt;
    }
    return value;
}

double averageFrequencyMHz(const std::vector<std::uint64_t>& khz_values)
{
    if (khz_values.empty()) {
        return 0.0;
    }

    std::uint64_t total = 0;
    for (const std::uint64_t value : khz_values) {
        total += value;
    }
    return static_cast<double>(total) / static_cast<double>(khz_values.size()) / 1000.0;
}

std::optional<double> CpuFrequencyCollector::scan(const std::filesystem::path& cpu_root) const
{
    std::vector<std::uint64_t> values;

    std::error_code error;
    std::filesystem::directory_iterator iterator(cpu_root, error);
    if (error) {
        return std::nullopt;
    }

    for (const auto& entry : iterator) {
        if (!entry.is_directory(error)) {
            continue;
        }
        if (!isCpuDirectoryName(entry.path().filename().string())) {
            continue;
        }

        const std::filesystem::path frequency_path =
            entry.path() / "cpufreq" / "scaling_cur_freq";
        const auto value = parseKilohertz(readFile(frequency_path));
        if (value.has_value()) {
            values.push_back(value.value());
        }
    }

    if (values.empty()) {
        return std::nullopt;
    }
    return averageFrequencyMHz(values);
}

std::optional<double> parseCpuInfoAverageMHz(std::string_view content)
{
    std::vector<double> values;

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
        if (trim(line.substr(0, colon)) != "cpu MHz") {
            continue;
        }

        const std::string_view number = trim(line.substr(colon + 1));
        double value = 0.0;
        const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
        if (result.ec == std::errc{} && result.ptr == number.data() + number.size()) {
            values.push_back(value);
        }
    }

    if (values.empty()) {
        return std::nullopt;
    }

    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

} // namespace tm_linux
