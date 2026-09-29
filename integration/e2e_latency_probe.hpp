#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

#include "common/domain.hpp"

namespace cfx {

struct SegmentedLatencyRecord {
    u64 eventId{0};
    u64 t1CaptureUs{0};
    u64 t2QueueProcessingUs{0};
    u64 t3TransportReceiveUs{0};
    u64 t4InjectionUs{0};
    bool hasCapture{false};
    bool hasQueueProcessing{false};
    bool hasTransportReceive{false};
    bool hasInjection{false};

    bool isComplete() const noexcept {
        return hasCapture && hasQueueProcessing && hasTransportReceive && hasInjection;
    }

    u64 captureToQueueUs() const noexcept { return t2QueueProcessingUs > t1CaptureUs ? t2QueueProcessingUs - t1CaptureUs : 0; }
    u64 queueToTransportUs() const noexcept { return t3TransportReceiveUs > t2QueueProcessingUs ? t3TransportReceiveUs - t2QueueProcessingUs : 0; }
    u64 receiveToInjectionUs() const noexcept { return t4InjectionUs > t3TransportReceiveUs ? t4InjectionUs - t3TransportReceiveUs : 0; }
    u64 totalE2ELatencyUs() const noexcept { return t4InjectionUs > t1CaptureUs ? t4InjectionUs - t1CaptureUs : 0; }
};

struct LatencyStats {
    u64 min{0};
    u64 max{0};
    u64 p50{0};
    u64 p99{0};
    u64 count{0};
};

struct SegmentedLatencyReport {
    LatencyStats captureToQueue;
    LatencyStats queueToTransport;
    LatencyStats receiveToInjection;
    LatencyStats totalE2E;
    u64 totalRecords{0};
    u64 completeRecords{0};
};

class E2ELatencyProbe {
public:
    E2ELatencyProbe() noexcept = default;
    ~E2ELatencyProbe() noexcept = default;

    E2ELatencyProbe(const E2ELatencyProbe&) = delete;
    E2ELatencyProbe& operator=(const E2ELatencyProbe&) = delete;

    void recordCapture(u64 eventId, u64 timestampUs) noexcept;
    void recordQueueProcessing(u64 eventId, u64 timestampUs) noexcept;
    void recordTransportReceive(u64 eventId, u64 timestampUs) noexcept;
    void recordInjection(u64 eventId, u64 timestampUs) noexcept;

    std::optional<SegmentedLatencyRecord> getRecord(u64 eventId) const noexcept;
    std::vector<SegmentedLatencyRecord> allRecords() const noexcept;
    SegmentedLatencyReport generateReport() const noexcept;
    void clear() noexcept;

private:
    static LatencyStats computeStats(std::vector<u64> values) noexcept;

    mutable std::mutex mutex_;
    std::map<u64, SegmentedLatencyRecord> records_;
};

}  // namespace cfx