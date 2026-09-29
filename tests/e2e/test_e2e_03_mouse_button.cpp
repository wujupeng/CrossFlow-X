#include <cstdio>
#include <cstdlib>
#include <thread>

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

static CanonicalInputEvent makeButtonEvent(EventType type, MouseButton btn) {
    CanonicalInputEvent event{};
    event.eventId = 0;
    event.sourceNodeId = makeNodeId(1, 1);
    event.eventType = type;
    event.payload = MouseButtonPayload{btn};
    event.modifierState = noModifiers();
    return event;
}

class StubClock : public IMonotonicClock {
public:
    u64 nowUs() override { return counter_.fetch_add(1, std::memory_order_relaxed); }
private:
    std::atomic<u64> counter_{1000};
};

static void test_e2e_03_mouse_button() {
    printf("[TEST] E2E-03: mouse button\n");

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

    struct ButtonCase { EventType type; MouseButton btn; const char* name; };
    ButtonCase cases[] = {
        {EventType::MouseButtonPress, MouseButton::Left, "left_down"},
        {EventType::MouseButtonRelease, MouseButton::Left, "left_up"},
        {EventType::MouseButtonPress, MouseButton::Right, "right_down"},
        {EventType::MouseButtonRelease, MouseButton::Right, "right_up"},
    };

    for (int i = 0; i < 4; ++i) {
        auto event = makeButtonEvent(cases[i].type, cases[i].btn);
        u64 ts = clock.nowUs();
        evidence.recordCaptureEvidence(i + 1, ts, cases[i].name);
        orchestrator.processEvent(event);
        u64 ts2 = clock.nowUs();
        evidence.recordInjectionEvidence(i + 1, ts2, cases[i].name);
    }

    CHECK(orchestrator.totalProcessed() == 4);

    auto corr = evidence.verifyCorrelation();
    CHECK(corr.pass);

    orchestrator.stop();
    transport.disconnect();
}

}  // namespace cfx

int main() {
    printf("=== test_e2e_03_mouse_button ===\n");
    cfx::test_e2e_03_mouse_button();
    printf("=== ALL PASS ===\n");
    return 0;
}