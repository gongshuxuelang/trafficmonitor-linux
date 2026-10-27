#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

#include "tm/metrics.h"
#include "tm/system_metrics.h"

namespace tm_linux {

class Sampler {
public:
    explicit Sampler(SystemMetricsCollector& collector);
    ~Sampler();

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    void start(int interval_ms);
    void stop();
    void setInterval(int interval_ms);
    void setSampleCallback(std::function<void(const Snapshot&)> callback);

    std::shared_ptr<const Snapshot> latest() const;

private:
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
