#include "integration/e2e_latency_probe.hpp"

namespace cfx {

void E2ELatencyProbe::recordCapture(u64 eventId, u64 timestampUs) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& rec = records_[eventId];
    rec.eventId = eventId;
    rec.t1CaptureUs = timestampUs;
    rec.hasCapture = true;
}

void E2ELatencyProbe::recordQueueProcessing(u64 eventId, u64 timestampUs) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& rec = records_[eventId];
    rec.eventId = eventId;
    rec.t2QueueProcessingUs = timestampUs;
    rec.hasQueueProcessing = true;
}

void E2ELatencyProbe::recordTransportReceive(u64 eventId, u64 timestampUs) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& rec = records_[eventId];
    rec.eventId = eventId;
    rec.t3TransportReceiveUs = timestampUs;
    rec.hasTransportReceive = true;
}

void E2ELatencyProbe::recordInjection(u64 eventId, u64 timestampUs) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& rec = records_[eventId];
    rec.eventId = eventId;
    rec.t4InjectionUs = timestampUs;
    rec.hasInjection = true;
}

std::optional<SegmentedLatencyRecord> E2ELatencyProbe::getRecord(u64 eventId) const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = records_.find(eventId);
    if (it == records_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<SegmentedLatencyRecord> E2ELatencyProbe::allRecords() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SegmentedLatencyRecord> result;
    result.reserve(records_.size());
    for (const auto& [_, rec] : records_) {
        result.push_back(rec);
    }
    return result;
}

LatencyStats E2ELatencyProbe::computeStats(std::vector<u64> values) noexcept {
    LatencyStats stats{};
    if (values.empty()) {
        return stats;
    }

    std::sort(values.begin(), values.end());
    stats.count = values.size();
    stats.min = values.front();
    stats.max = values.back();

    size_t p50Idx = values.size() / 2;
    stats.p50 = values[p50Idx];

    size_t p99Idx = static_cast<size_t>(static_cast<double>(values.size()) * 0.99);
    if (p99Idx >= values.size()) p99Idx = values.size() - 1;
    stats.p99 = values[p99Idx];

    return stats;
}

SegmentedLatencyReport E2ELatencyProbe::generateReport() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    SegmentedLatencyReport report{};
    report.totalRecords = records_.size();

    std::vector<u64> seg1, seg2, seg3, total;

    for (const auto& [_, rec] : records_) {
        if (!rec.isComplete()) {
            continue;
        }
        report.completeRecords++;
        seg1.push_back(rec.captureToQueueUs());
        seg2.push_back(rec.queueToTransportUs());
        seg3.push_back(rec.receiveToInjectionUs());
        total.push_back(rec.totalE2ELatencyUs());
    }

    report.captureToQueue = computeStats(std::move(seg1));
    report.queueToTransport = computeStats(std::move(seg2));
    report.receiveToInjection = computeStats(std::move(seg3));
    report.totalE2E = computeStats(std::move(total));

    return report;
}

void E2ELatencyProbe::clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.clear();
}

}  // namespace cfx