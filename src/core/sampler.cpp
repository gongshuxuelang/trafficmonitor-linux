#include "core/sampler.h"

namespace tm_linux {

Sampler::Sampler(SystemMetricsCollector& collector)
    : collector_(collector)
{
}

Sampler::~Sampler()
{
    stop();
}

void Sampler::start(int interval_ms)
{
    if (thread_.joinable()) {
        return;
    }
    setInterval(interval_ms);
    stop_.store(false);
    thread_ = std::thread(&Sampler::run, this);
}

void Sampler::stop()
{
    stop_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Sampler::setInterval(int interval_ms)
{
    interval_ms_.store(std::clamp(interval_ms, 200, 5000));
}

void Sampler::setSampleCallback(std::function<void(const Snapshot&)> callback)
{
    callback_ = std::move(callback);
}

std::shared_ptr<const Snapshot> Sampler::latest() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
}

void Sampler::run()
{
    using namespace std::chrono_literals;

    while (!stop_.load()) {
        auto snapshot = std::make_shared<Snapshot>(collector_.sample());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            latest_ = snapshot;
        }
        if (callback_) {
            callback_(*snapshot);
        }

        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms_.load());
        while (!stop_.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(50ms);
        }
    }
}

} // namespace tm_linux
