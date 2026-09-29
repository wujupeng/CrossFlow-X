#include "integration/e2e_pipeline_orchestrator.hpp"

#include <cstdio>

namespace cfx {

E2EPipelineOrchestrator::E2EPipelineOrchestrator(IInputPlaneChannel& inputPlane,
                                                   IControlPlaneChannel& controlPlane,
                                                   IMonotonicClock& clock,
                                                   const E2EPipelineConfig& config) noexcept
    : inputPlane_(inputPlane)
    , controlPlane_(controlPlane)
    , clock_(clock)
    , config_(config)
    , eventIdCounter_(config.eventIdSeed) {}

E2EPipelineOrchestrator::~E2EPipelineOrchestrator() noexcept {
    stop();
}

bool E2EPipelineOrchestrator::start() noexcept {
    if (running_.load(std::memory_order_relaxed)) {
        return true;
    }

    stopRequested_.store(false, std::memory_order_relaxed);
    running_.store(true, std::memory_order_relaxed);

    HeartbeatMessage heartbeat{};
    heartbeat.timestamp = clock_.nowUs();
    controlPlane_.send(ControlMessage{heartbeat});

    try {
        processingThread_ = std::thread([this]() { processingLoop(); });
    } catch (...) {
        running_.store(false, std::memory_order_relaxed);
        fprintf(stderr, "[CFX-E-E2E-START-FAIL] failed to start processing thread\n");
        return false;
    }

    return true;
}

void E2EPipelineOrchestrator::stop() noexcept {
    if (!running_.load(std::memory_order_relaxed)) {
        return;
    }

    stopRequested_.store(true, std::memory_order_relaxed);
    running_.store(false, std::memory_order_relaxed);

    if (processingThread_.joinable()) {
        processingThread_.join();
    }
}

void E2EPipelineOrchestrator::processingLoop() noexcept {
    while (!stopRequested_.load(std::memory_order_acquire)) {
        u32 bl = inputPlane_.backlog();
        if (bl > config_.backpressureThreshold) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

CanonicalInputEvent E2EPipelineOrchestrator::enrichEvent(const CanonicalInputEvent& event) noexcept {
    CanonicalInputEvent enriched = event;
    enriched.eventId = eventIdCounter_.fetch_add(1, std::memory_order_relaxed);
    enriched.sourceNodeId = config_.localNodeId;
    enriched.timestamp = clock_.nowUs();
    return enriched;
}

void E2EPipelineOrchestrator::processEvent(const CanonicalInputEvent& event) noexcept {
    if (!running_.load(std::memory_order_relaxed)) {
        return;
    }

    CanonicalInputEvent enriched = enrichEvent(event);

    if (config_.enableEdgeDetection) {
        totalEdgeOverflow_.fetch_add(1, std::memory_order_relaxed);
    }

    u32 bl = inputPlane_.backlog();
    if (bl > config_.backpressureThreshold) {
        totalDroppedOldest_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    inputPlane_.send(enriched);
    totalTransportSent_.fetch_add(1, std::memory_order_relaxed);
    totalProcessed_.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace cfx