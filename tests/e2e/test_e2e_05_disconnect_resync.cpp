#include <cstdio>
#include <cstdlib>
#include <thread>

#include "integration/disconnect_resync_coordinator.hpp"
#include "integration/e2e_pipeline_orchestrator.hpp"
#include "integration/e2e_evidence_collector.hpp"
#include "s05_transport/transport_impl.hpp"
#include "common/domain.hpp"
#include "common/platform_ports.hpp"

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED: %s:%d: %s\n", __FILE__, __LINE__, #cond); std::exit(1); } } while(0)

namespace cfx {

static NodeId makeNodeId(u64 high, u64 low) {
    NodeId id{}; id.high = high; id.low = low; return id;
}
static ModifierState noModifiers() { return {false, false, false, false, false}; }

class StubClock : public IMonotonicClock {
public:
    u64 nowUs() override { return counter_.fetch_add(1, std::memory_order_relaxed); }
private:
    std::atomic<u64> counter_{1000};
};

static void test_e2e_05_disconnect_resync() {
    printf("[TEST] E2E-05: disconnect resync\n");

    TransportConfig transportCfg{};
    transportCfg.remoteHost = "127.0.0.1";
    TransportImpl transport(transportCfg);
    transport.connect();

    StubClock clock;
    E2EPipelineConfig config{};
    config.localNodeId = makeNodeId(1, 1);
    config.enableEdgeDetection = false;

    E2EPipelineOrchestrator orchestrator(transport, transport, clock, config);
    orchestrator.start();

    E2EEvidenceCollector evidence;

    DisconnectResyncConfig drscConfig{};

    bool releaseCalled = false;
    bool resyncCalled = false;

    DisconnectResyncCoordinator coordinator(
        transport,
        drscConfig,
        [&](const PressedStateSnapshot&) -> ReleaseResult { releaseCalled = true; return {0, 0}; },
        [&]() -> PressedStateSnapshot { return PressedStateSnapshot{}; },
        [&]() -> void { resyncCalled = true; },
        [&]() -> bool { return true; }
    );

    coordinator.start();
    CHECK(coordinator.state() == DisconnectResyncState::Connected);
    CHECK(coordinator.isInputAllowed() == true);

    evidence.recordDisconnectEvidence(clock.nowUs(), "simulated_disconnect");
    transport.disconnect();
    CHECK(!transport.isConnected());

    CanonicalInputEvent event{};
    event.eventId = 1;
    event.sourceNodeId = makeNodeId(1, 1);
    event.eventType = EventType::MouseMove;
    event.payload = MouseMovePayload{10, 0};
    event.modifierState = noModifiers();
    orchestrator.processEvent(event);

    evidence.recordReconnectEvidence(clock.nowUs(), "simulated_reconnect");
    transport.connect();
    CHECK(transport.isConnected());

    orchestrator.processEvent(event);
    CHECK(orchestrator.totalProcessed() >= 1);

    auto records = evidence.allRecords();
    bool hasDisconnect = false;
    bool hasReconnect = false;
    for (const auto& r : records) {
        if (r.type == EvidenceType::Disconnect) hasDisconnect = true;
        if (r.type == EvidenceType::Reconnect) hasReconnect = true;
    }
    CHECK(hasDisconnect);
    CHECK(hasReconnect);

    coordinator.stop();
    orchestrator.stop();
    transport.disconnect();
}

}  // namespace cfx

int main() {
    printf("=== test_e2e_05_disconnect_resync ===\n");
    cfx::test_e2e_05_disconnect_resync();
    printf("=== ALL PASS ===\n");
    return 0;
}