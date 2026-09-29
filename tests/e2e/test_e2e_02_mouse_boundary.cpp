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
    NodeId id{}; id.high = high; id.low = low; return id;
}
static ModifierState noModifiers() { return {false, false, false, false, false}; }

static CanonicalInputEvent makeMouseMoveEvent(u64 eventId, NodeId source, i32 dx, i32 dy) {
    CanonicalInputEvent event{};
    event.eventId = eventId;
    event.sourceNodeId = source;
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

static void test_e2e_02_mouse_boundary() {
    printf("[TEST] E2E-02: mouse boundary\n");

    TransportConfig transportCfg{};
    transportCfg.remoteHost = "127.0.0.1";
    TransportImpl transport(transportCfg);
    transport.connect();

    StubClock clock;
    E2EPipelineConfig config{};
    config.localNodeId = makeNodeId(1, 1);
    config.enableEdgeDetection = true;

    E2EPipelineOrchestrator orchestrator(transport, transport, clock, config);
    orchestrator.start();

    NodeId source = makeNodeId(1, 1);

    struct BoundaryCase { i32 dx; i32 dy; const char* name; };
    BoundaryCase cases[] = {
        {-100, 0, "left"}, {100, 0, "right"},
        {0, -100, "top"}, {0, 100, "bottom"},
    };

    for (auto& c : cases) {
        auto event = makeMouseMoveEvent(0, source, c.dx, c.dy);
        orchestrator.processEvent(event);
    }

    CHECK(orchestrator.totalProcessed() == 4);
    CHECK(orchestrator.totalEdgeOverflow() == 4);

    orchestrator.stop();
    transport.disconnect();
}

}  // namespace cfx

int main() {
    printf("=== test_e2e_02_mouse_boundary ===\n");
    cfx::test_e2e_02_mouse_boundary();
    printf("=== ALL PASS ===\n");
    return 0;
}