#include "core/system_metrics.h"

#include <sys/statvfs.h>
#include <unistd.h>

namespace tm_linux {

namespace {

/**
 * @brief 读取文件全部内容。
 * @param path 文件路径。
 * @return 文件内容；无法打开时返回空字符串。
 */
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream) {
        return {};
    }

    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

/**
 * @brief 从 /proc/net/route 内容中查找默认路由使用的网卡。
 * @param route_content route 文件内容。
 * @return 默认路由网卡名；未找到时返回空字符串。
 */
std::string defaultRouteInterface(std::string_view route_content)
{
    std::istringstream lines{std::string(route_content)};
    std::string line;
    std::getline(lines, line); // header

    while (std::getline(lines, line)) {
        std::istringstream fields(line);
        std::string interface;
        std::string destination;
        std::string gateway;
        std::string flags;
        if (!(fields >> interface >> destination >> gateway >> flags)) {
            continue;
        }
        if (destination == "00000000" && flags.find("0003") != std::string::npos) {
            return interface;
        }
    }

    return {};
}

/**
 * @brief 在采样列表中按接口名查找。
 * @param samples 网络接口采样列表。
 * @param interface 接口名。
 * @return 找到的采样指针；未找到时返回 nullptr。
 */
const NetworkSample* findInterface(const std::vector<NetworkSample>& samples,
                                   std::string_view interface)
{
    const auto it = std::find_if(samples.begin(), samples.end(), [interface](const auto& sample) {
        return sample.interface == interface;
    });
    return it == samples.end() ? nullptr : &*it;
}

/**
 * @brief 选择要展示的网卡。
 *
 * 依次尝试：配置的接口、默认路由接口、首个有流量的非 lo 接口、
 * 首个非 lo 接口，最后回退到列表首项。
 *
 * @param samples 网络接口采样列表。
 * @param configured 配置的接口名或 "auto"。
 * @param proc_root procfs 根目录。
 * @return 选中的采样指针；列表为空时返回 nullptr。
 */
const NetworkSample* selectNetwork(const std::vector<NetworkSample>& samples,
                                   const std::string& configured,
                                   const std::filesystem::path& proc_root)
{
    if (configured != "auto") {
        if (const auto* sample = findInterface(samples, configured)) {
            return sample;
        }
    }

    const std::string default_route =
        defaultRouteInterface(readFile(proc_root / "net" / "route"));
    if (!default_route.empty()) {
        if (const auto* sample = findInterface(samples, default_route)) {
            return sample;
        }
    }

    for (const auto& sample : samples) {
        if (sample.interface != "lo" && (sample.rx_bytes > 0 || sample.tx_bytes > 0)) {
            return &sample;
        }
    }
    for (const auto& sample : samples) {
        if (sample.interface != "lo") {
            return &sample;
        }
    }
    return samples.empty() ? nullptr : &samples.front();
}

/**
 * @brief 获取文件系统的块大小。
 * @param stats statvfs 查询结果。
 * @return 优先返回 f_frsize，为 0 时回退到 f_bsize。
 */
std::uint64_t fragmentSize(const struct statvfs& stats)
{
    return stats.f_frsize != 0 ? stats.f_frsize : stats.f_bsize;
}

} // namespace

SystemMetricsCollector::SystemMetricsCollector(std::filesystem::path proc_root,
                                               std::filesystem::path sys_root)
    : proc_root_(std::move(proc_root))
    , sys_root_(std::move(sys_root))
{
}

void SystemMetricsCollector::setNetworkInterface(std::string interface)
{
    network_interface_ = std::move(interface);
}

void SystemMetricsCollector::setDiskMountPoint(std::string mount_point)
{
    disk_mount_point_ = std::move(mount_point);
}

void SystemMetricsCollector::setTodayTotals(std::uint64_t up_bytes, std::uint64_t down_bytes)
{
    today_up_bytes_ = up_bytes;
    today_down_bytes_ = down_bytes;
}

Snapshot SystemMetricsCollector::sample()
{
    Snapshot snapshot;
    snapshot.sample_sequence = ++sequence_;
    snapshot.timestamp_unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count();

    const auto now = std::chrono::steady_clock::now();

    // Network.
    const auto network_samples = parseNetDev(readFile(proc_root_ / "net" / "dev"));
    const auto network_rates = network_rates_.update(network_samples, now);
    if (const NetworkSample* selected =
            selectNetwork(network_samples, network_interface_, proc_root_)) {
        NetworkInfo network;
        network.interface = selected->interface;
        network.rx_bytes = selected->rx_bytes;
        network.tx_bytes = selected->tx_bytes;
        network.is_loopback = selected->interface == "lo";
        network.is_selected = true;

        if (const auto rate = network_rates.find(selected->interface);
            rate != network_rates.end()) {
            network.rx_bytes_per_sec = rate->second.rx_bytes_per_sec.value_or(0.0);
            network.tx_bytes_per_sec = rate->second.tx_bytes_per_sec.value_or(0.0);
        }

        const double elapsed_seconds =
            std::chrono::duration<double>(now - previous_network_time_).count();
        if (elapsed_seconds > 0.0) {
            const auto previous = previous_network_.find(selected->interface);
            if (previous != previous_network_.end()) {
                if (selected->rx_bytes >= previous->second.rx_bytes) {
                    today_down_bytes_ += selected->rx_bytes - previous->second.rx_bytes;
                }
                if (selected->tx_bytes >= previous->second.tx_bytes) {
                    today_up_bytes_ += selected->tx_bytes - previous->second.tx_bytes;
                }
            }
        }

        for (const auto& sample : network_samples) {
            previous_network_[sample.interface] = sample;
        }
        previous_network_time_ = now;

        snapshot.network = network;
    }

    // CPU.
    const auto cpu_times = parseProcStat(readFile(proc_root_ / "stat"));
    if (cpu_times.has_value()) {
        snapshot.cpu.core_count = static_cast<std::uint32_t>(::sysconf(_SC_NPROCESSORS_ONLN));
        if (previous_cpu_.has_value()) {
            if (const auto usage = computeCpuUsagePercent(*previous_cpu_, *cpu_times)) {
                snapshot.cpu.usage_percent = *usage;
            }
        }
        previous_cpu_ = cpu_times;
    }

    auto frequency = CpuFrequencyCollector{}.scan(sys_root_ / "devices" / "system" / "cpu");
    if (!frequency.has_value()) {
        frequency = parseCpuInfoAverageMHz(readFile(proc_root_ / "cpuinfo"));
    }
    if (frequency.has_value()) {
        snapshot.cpu.frequency_mhz = *frequency;
    }

    // Memory.
    snapshot.memory = parseMemInfo(readFile(proc_root_ / "meminfo"));

    // Disk.
    const auto disk_samples = parseDiskStats(readFile(proc_root_ / "diskstats"));
    std::unordered_map<std::string, DiskStatsSample> physical_disks;
    std::uint64_t total_io_ticks = 0;
    for (const auto& [name, sample] : disk_samples) {
        if (!isPhysicalDiskName(name)) {
            continue;
        }
        total_io_ticks += sample.io_ticks_ms;
        physical_disks["__total__"] = DiskStatsSample{total_io_ticks};
    }

    DiskInfo disk;
    disk.mount_point = disk_mount_point_;
    disk.device = "all";

    struct statvfs filesystem_stats {};
    if (::statvfs(disk_mount_point_.c_str(), &filesystem_stats) == 0) {
        const std::uint64_t fragment = fragmentSize(filesystem_stats);
        disk.total_bytes = static_cast<std::uint64_t>(filesystem_stats.f_blocks) * fragment;
        const std::uint64_t available =
            static_cast<std::uint64_t>(filesystem_stats.f_bavail) * fragment;
        disk.used_bytes = disk.total_bytes - std::min(available, disk.total_bytes);
        disk.used_percent = computeDiskUsagePercent(disk.total_bytes, available);
    }

    const auto disk_rates = disk_rates_.update(physical_disks, now);
    if (const auto rate = disk_rates.find("__total__"); rate != disk_rates.end()) {
        disk.io_util_percent = rate->second;
        disk.io_available = true;
    }
    snapshot.disk = disk;

    // Temperatures.
    snapshot.temperature = TemperatureCollector{}.scan(sys_root_ / "class" / "hwmon");

    snapshot.today_up_bytes = today_up_bytes_;
    snapshot.today_down_bytes = today_down_bytes_;

    return snapshot;
}

} // namespace tm_linux
