#ifndef TRAFFICMONITOR_LINUX_SRC_CORE_SAMPLER_H_
#define TRAFFICMONITOR_LINUX_SRC_CORE_SAMPLER_H_

#include "pch.h"

#include "common/DTO/metrics.h"
#include "core/system_metrics.h"

namespace tm_linux {

/**
 * @brief 在独立线程中按固定间隔采集指标，并保存最新快照。
 */
class Sampler {
public:
    /**
     * @brief 绑定系统指标采集器。
     * @param collector 采集器引用，生命周期须长于 Sampler。
     */
    explicit Sampler(SystemMetricsCollector& collector);

    /**
     * @brief 停止采样线程。
     */
    ~Sampler();

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    /**
     * @brief 启动采样线程；线程已在运行时直接返回。
     * @param interval_ms 采样间隔（毫秒），会被限制在 200–5000 之间。
     */
    void start(int interval_ms);

    /**
     * @brief 请求停止并等待采样线程退出。
     */
    void stop();

    /**
     * @brief 调整采样间隔。
     * @param interval_ms 间隔（毫秒），会被限制在 200–5000 之间。
     */
    void setInterval(int interval_ms);

    /**
     * @brief 设置每次采样完成后的回调。
     * @param callback 回调函数，在采样线程中执行。
     */
    void setSampleCallback(std::function<void(const Snapshot&)> callback);

    /**
     * @brief 获取最近一次快照。
     * @return 快照的共享指针；尚未采样时为 nullptr。
     */
    std::shared_ptr<const Snapshot> latest() const;

private:
    /**
     * @brief 采样线程主循环：按间隔采样、更新最新值并触发回调。
     */
    void run();

    SystemMetricsCollector& collector_;
    std::atomic<int> interval_ms_{1000};
    std::atomic<bool> stop_{false};
    std::function<void(const Snapshot&)> callback_;
    mutable std::mutex mutex_;
    std::shared_ptr<const Snapshot> latest_;
    std::thread thread_;
};

} // namespace tm_linux

#endif  // TRAFFICMONITOR_LINUX_SRC_CORE_SAMPLER_H_
