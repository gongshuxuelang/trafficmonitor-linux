#include "core/collectors/collectors.h"

namespace tm_linux {

namespace {

/**
 * @brief 去除字符串两端的空白字符。
 * @param value 输入文本。
 * @return 去除空白后的视图。
 */
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

    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, output);
    return result.ec == std::errc{} && result.ptr == end;
}

} // namespace

std::vector<NetworkSample> parseNetDev(std::string_view content)
{
    std::vector<NetworkSample> samples;

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

        const std::string_view interface_name = trim(line.substr(0, colon));
        if (interface_name.empty()) {
            continue;
        }

        const std::string_view fields_text = line.substr(colon + 1);
        std::string_view fields[9]{};
        std::size_t field_count = 0;
        std::size_t cursor = 0;
        while (cursor < fields_text.size() && field_count < 9) {
            while (cursor < fields_text.size()
                   && std::isspace(static_cast<unsigned char>(fields_text[cursor]))) {
                ++cursor;
            }
            if (cursor >= fields_text.size()) {
                break;
            }

            const std::size_t start = cursor;
            while (cursor < fields_text.size()
                   && !std::isspace(static_cast<unsigned char>(fields_text[cursor]))) {
                ++cursor;
            }
            fields[field_count++] = fields_text.substr(start, cursor - start);
        }

        if (field_count < 9) {
            continue;
        }

        std::uint64_t rx_bytes = 0;
        std::uint64_t tx_bytes = 0;
        if (!parseUnsigned(fields[0], rx_bytes) || !parseUnsigned(fields[8], tx_bytes)) {
            continue;
        }

        samples.push_back(NetworkSample{std::string(interface_name), rx_bytes, tx_bytes});
    }

    return samples;
}

std::unordered_map<std::string, NetworkRate> NetworkRateCalculator::update(
    const std::vector<NetworkSample>& samples,
    std::chrono::steady_clock::time_point now)
{
    std::unordered_map<std::string, NetworkRate> rates;
    rates.reserve(samples.size());

    for (const auto& sample : samples) {
        NetworkRate rate;
        const auto previous = previous_.find(sample.interface);

        if (previous != previous_.end()) {
            const double elapsed_seconds =
                std::chrono::duration<double>(now - previous->second.time).count();
            if (elapsed_seconds > 0.0) {
                const std::uint64_t rx_delta = sample.rx_bytes >= previous->second.rx_bytes
                    ? sample.rx_bytes - previous->second.rx_bytes
                    : 0;
                const std::uint64_t tx_delta = sample.tx_bytes >= previous->second.tx_bytes
                    ? sample.tx_bytes - previous->second.tx_bytes
                    : 0;
                rate.rx_bytes_per_sec = static_cast<double>(rx_delta) / elapsed_seconds;
                rate.tx_bytes_per_sec = static_cast<double>(tx_delta) / elapsed_seconds;
            }
        }

        previous_[sample.interface] = Previous{sample.rx_bytes, sample.tx_bytes, now};
        rates.emplace(sample.interface, rate);
    }

    return rates;
}

} // namespace tm_linux
