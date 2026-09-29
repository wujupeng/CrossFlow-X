#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "s05_transport/i_input_plane_channel.hpp"
#include "s05_transport/i_control_plane_channel.hpp"

namespace cfx {

struct E2EPipelineConfig {
    NodeId localNodeId{};
    u64 eventIdSeed{1};
    bool enableEdgeDetection{true};
    bool enableLatencyProbe{false};
    bool enableEvidenceCollector{false};
    u32 backpressureThreshold{128};
};

class E2EPipelineOrchestrator {
public:
    E2EPipelineOrchestrator(IInputPlaneChannel& inputPlane,
                            IControlPlaneChannel& controlPlane,
                            IMonotonicClock& clock,
                            const E2EPipelineConfig& config) noexcept;
    ~E2EPipelineOrchestrator() noexcept;

    E2EPipelineOrchestrator(const E2EPipelineOrchestrator&) = delete;
    E2EPipelineOrchestrator& operator=(const E2EPipelineOrchestrator&) = delete;

    bool start() noexcept;
    void stop() noexcept;
    bool isRunning() const noexcept { return running_.load(std::memory_order_relaxed); }

    void processEvent(const CanonicalInputEvent& event) noexcept;

    uint64_t totalProcessed() const noexcept { return totalProcessed_.load(std::memory_order_relaxed); }
    uint64_t totalEdgeOverflow() const noexcept { return totalEdgeOverflow_.load(std::memory_order_relaxed); }
    uint64_t totalTransportSent() const noexcept { return totalTransportSent_.load(std::memory_order_relaxed); }
    uint64_t totalDroppedOldest() const noexcept { return totalDroppedOldest_.load(std::memory_order_relaxed); }

private:
    void processingLoop() noexcept;
    CanonicalInputEvent enrichEvent(const CanonicalInputEvent& event) noexcept;

    IInputPlaneChannel& inputPlane_;
    IControlPlaneChannel& controlPlane_;
    IMonotonicClock& clock_;
    E2EPipelineConfig config_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::thread processingThread_;

    std::atomic<uint64_t> eventIdCounter_{1};
    std::atomic<uint64_t> totalProcessed_{0};
    std::atomic<uint64_t> totalEdgeOverflow_{0};
    std::atomic<uint64_t> totalTransportSent_{0};
    std::atomic<uint64_t> totalDroppedOldest_{0};
};

}  // namespace cfx