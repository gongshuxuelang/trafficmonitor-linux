#include "tm/collectors.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <string>
#include <system_error>

namespace tm_linux {

namespace {

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

void updateMaximum(std::optional<double>& target, double candidate)
{
    if (!target.has_value() || candidate > target.value()) {
        target = candidate;
    }
}

} // namespace

TemperatureKind classifyHwmonName(std::string_view name)
{
    const std::string_view trimmed = trim(name);
    if (trimmed == "coretemp" || trimmed == "k10temp" || trimmed == "zenpower") {
        return TemperatureKind::Cpu;
    }
    if (trimmed == "amdgpu" || trimmed == "nouveau" || trimmed == "nvidia") {
        return TemperatureKind::Gpu;
    }
    if (trimmed == "nvme" || trimmed == "drivetemp") {
        return TemperatureKind::Hdd;
    }
    if (trimmed == "acpitz" || trimmed == "thinkpad" || trimmed == "asus"
        || trimmed == "dell") {
        return TemperatureKind::Mainboard;
    }
    return TemperatureKind::Unknown;
}

std::optional<double> parseMilliCelsius(std::string_view text)
{
    const std::string_view trimmed = trim(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::int64_t milli_celsius = 0;
    const auto result =
        std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), milli_celsius);
    if (result.ec != std::errc{} || result.ptr != trimmed.data() + trimmed.size()) {
        return std::nullopt;
    }

    return static_cast<double>(milli_celsius) / 1000.0;
}

TemperatureInfo TemperatureCollector::scan(const std::filesystem::path& root) const
{
    TemperatureInfo info;

    std::error_code error;
    std::filesystem::directory_iterator iterator(root, error);
    if (error) {
        return info;
    }

    for (const auto& entry : iterator) {
        if (!entry.is_directory(error)) {
            continue;
        }

        const TemperatureKind kind = classifyHwmonName(readFile(entry.path() / "name"));
        if (kind == TemperatureKind::Unknown) {
            continue;
        }

        std::filesystem::directory_iterator temperatures(entry.path(), error);
        if (error) {
            continue;
        }

        for (const auto& temperature : temperatures) {
            const std::string filename = temperature.path().filename().string();
            if (!filename.starts_with("temp") || !filename.ends_with("_input")) {
                continue;
            }

            const auto value = parseMilliCelsius(readFile(temperature.path()));
            if (!value.has_value()) {
                continue;
            }

            switch (kind) {
            case TemperatureKind::Cpu:
                updateMaximum(info.cpu_c, value.value());
                break;
            case TemperatureKind::Gpu:
                updateMaximum(info.gpu_c, value.value());
                break;
            case TemperatureKind::Hdd:
                updateMaximum(info.hdd_c, value.value());
                break;
            case TemperatureKind::Mainboard:
                updateMaximum(info.mainboard_c, value.value());
                break;
            case TemperatureKind::Unknown:
                break;
            }
        }
    }

    return info;
}

} // namespace tm_linux
