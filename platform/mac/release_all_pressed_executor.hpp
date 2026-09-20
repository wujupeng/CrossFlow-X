#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"

namespace cfx {

using ClockFn = std::function<std::chrono::high_resolution_clock::time_point()>;

class ReleaseAllPressedExecutor {
public:
    static constexpr uint32_t kTotalDeadlineMs = 100;

    enum class Result : uint8_t {
        Success,
        PartialRelease,
        DeadlineReached,
        Degraded,
    };

    ReleaseAllPressedExecutor() noexcept = default;
    ~ReleaseAllPressedExecutor() noexcept = default;

    ReleaseAllPressedExecutor(const ReleaseAllPressedExecutor&) = delete;
    ReleaseAllPressedExecutor& operator=(const ReleaseAllPressedExecutor&) = delete;

    Result execute(const PressedStateSnapshot& snapshot,
                   std::function<InjectResult(const CanonicalInputEvent&)> injectFn,
                   NodeId sourceNodeId = {},
                   ClockFn clockFn = []() { return std::chrono::high_resolution_clock::now(); }) noexcept;

    Result lastResult() const noexcept {
        return static_cast<Result>(lastResult_.load(std::memory_order_acquire));
    }

    uint32_t releasedCount() const noexcept {
        return releasedCount_.load(std::memory_order_relaxed);
    }

    uint32_t totalElapsedMs() const noexcept {
        return totalElapsedMs_.load(std::memory_order_relaxed);
    }

    uint64_t executionCount() const noexcept {
        return executionCount_.load(std::memory_order_relaxed);
    }

    bool degraded() const noexcept {
        return lastResult() == Result::Degraded;
    }

    static const char* resultString(Result r) noexcept;

private:
    std::atomic<uint8_t> lastResult_{static_cast<uint8_t>(Result::Success)};
    std::atomic<uint32_t> releasedCount_{0};
    std::atomic<uint32_t> totalElapsedMs_{0};
    std::atomic<uint64_t> executionCount_{0};
};

}  // namespace cfx