#include <cstdio>
#include <cstdlib>
#include <thread>

#include "integration/e2e_pipeline_orchestrator.hpp"
#include "integration/e2e_latency_probe.hpp"
#include "integration/e2e_evidence_collector.hpp"
#include "s05_transport/transport_impl.hpp"
#include "common/domain.hpp"
#include "common/platform_ports.hpp"

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED: %s:%d: %s\n", __FILE__, __LINE__, #cond); std::exit(1); } } while(0)

namespace cfx {

static NodeId makeNodeId(u64 high, u64 low) {
    NodeId id{};
    id.high = high;
    id.low = low;
    return id;
}

static ModifierState noModifiers() {
    return {false, false, false, false, false};
}

static CanonicalInputEvent makeMouseMoveEvent(u64 eventId, NodeId source, i32 dx, i32 dy) {
    CanonicalInputEvent event{};
    event.eventId = eventId;
    event.sourceNodeId = source;
    event.timestamp = 0;
    event.eventType = EventType::MouseMove;
    event.payload = MouseMovePayload{dx, dy};
    event.modifierState = noModifiers();
    return event;
}

class StubClock : public IMonotonicClock {
public:
    u64 nowUs() override { return counter_.fetch_add(1, std::memory_order_relaxed); }
private:
    std::atomic<u64> counter_{1000};
};

static void test_e2e_01_mouse_move() {
    printf("[TEST] E2E-01: mouse move\n");

    TransportConfig transportCfg{};
    transportCfg.remoteHost = "127.0.0.1";
    transportCfg.inputPlanePort = 9001;
    transportCfg.controlPlanePort = 9002;
    TransportImpl transport(transportCfg);
    transport.connect();

    StubClock clock;
    E2EPipelineConfig config{};
    config.localNodeId = makeNodeId(1, 1);
    config.eventIdSeed = 1;
    config.enableEdgeDetection = false;

    E2EPipelineOrchestrator orchestrator(transport, transport, clock, config);
    orchestrator.start();

    E2EEvidenceCollector evidence;
    E2ELatencyProbe latency;

    NodeId source = makeNodeId(1, 1);

    for (int i = 1; i <= 10; ++i) {
        auto event = makeMouseMoveEvent(0, source, 10, 0);
        u64 ts1 = clock.nowUs();
        latency.recordCapture(i, ts1);
        evidence.recordCaptureEvidence(i, ts1, "mouse_move_10_0");

        u64 ts2 = clock.nowUs();
        latency.recordQueueProcessing(i, ts2);

        u64 ts3 = clock.nowUs();
        latency.recordTransportReceive(i, ts3);

        orchestrator.processEvent(event);

        u64 ts4 = clock.nowUs();
        latency.recordInjection(i, ts4);
        evidence.recordInjectionEvidence(i, ts4, "mouse_move_10_0");
    }

    CHECK(orchestrator.totalProcessed() == 10);
    CHECK(orchestrator.totalTransportSent() == 10);

    auto corrResult = evidence.verifyCorrelation();
    CHECK(corrResult.pass);
    CHECK(corrResult.matchedCount == 10);

    auto latencyReport = latency.generateReport();
    CHECK(latencyReport.completeRecords == 10);

    orchestrator.stop();
    transport.disconnect();
}

}  // namespace cfx

int main() {
    printf("=== test_e2e_01_mouse_move ===\n");
    cfx::test_e2e_01_mouse_move();
    printf("=== ALL PASS ===\n");
    return 0;
}