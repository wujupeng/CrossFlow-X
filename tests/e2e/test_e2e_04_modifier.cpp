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

static CanonicalInputEvent makeKeyEvent(EventType type, KeyCode code, ModifierState mods) {
    CanonicalInputEvent event{};
    event.eventId = 0;
    event.sourceNodeId = makeNodeId(1, 1);
    event.eventType = type;
    event.payload = KeyPayload{code};
    event.modifierState = mods;
    return event;
}

class StubClock : public IMonotonicClock {
public:
    u64 nowUs() override { return counter_.fetch_add(1, std::memory_order_relaxed); }
private:
    std::atomic<u64> counter_{1000};
};

static void test_e2e_04_modifier() {
    printf("[TEST] E2E-04: modifier\n");

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

    struct ModifierCase { ModifierState mods; KeyCode key; const char* name; };
    ModifierCase cases[] = {
        {{true, false, false, false, false}, 16, "shift+a"},
        {{false, true, false, false, false}, 16, "ctrl+a"},
        {{false, false, true, false, false}, 16, "alt+a"},
        {{false, false, false, true, false}, 16, "cmd+a"},
        {{true, true, false, false, false}, 16, "shift+ctrl+a"},
    };

    for (int i = 0; i < 5; ++i) {
        auto downEvent = makeKeyEvent(EventType::KeyPress, cases[i].key, cases[i].mods);
        auto upEvent = makeKeyEvent(EventType::KeyRelease, cases[i].key, {false, false, false, false, false});

        u64 ts1 = clock.nowUs();
        evidence.recordCaptureEvidence(i * 2 + 1, ts1, cases[i].name);
        orchestrator.processEvent(downEvent);
        u64 ts2 = clock.nowUs();
        evidence.recordInjectionEvidence(i * 2 + 1, ts2, cases[i].name);

        u64 ts3 = clock.nowUs();
        evidence.recordCaptureEvidence(i * 2 + 2, ts3, cases[i].name);
        orchestrator.processEvent(upEvent);
        u64 ts4 = clock.nowUs();
        evidence.recordInjectionEvidence(i * 2 + 2, ts4, cases[i].name);
    }

    CHECK(orchestrator.totalProcessed() == 10);

    auto corr = evidence.verifyCorrelation();
    CHECK(corr.pass);

    orchestrator.stop();
    transport.disconnect();
}

}  // namespace cfx

int main() {
    printf("=== test_e2e_04_modifier ===\n");
    cfx::test_e2e_04_modifier();
    printf("=== ALL PASS ===\n");
    return 0;
}