#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <vector>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "s05_transport/transport_impl.hpp"
#include "integration/e2e_pipeline_orchestrator.hpp"
#include "loopback_transport.hpp"

#ifdef _WIN32
#include "win/win_event_injector.hpp"
#include "win/win_input_plane_receiver.hpp"
#endif

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

class MockClock : public IMonotonicClock {
public:
    u64 nowUs() override { return ++counter_; }
private:
    u64 counter_{0};
};

static void test_win_event_injector_basic() {
    fprintf(stderr, "[TEST] test_win_event_injector_basic\n");
#ifdef _WIN32
    NodeId sourceId{1, 1};
    WinEventInjector injector(sourceId);

    CanonicalInputEvent event{};
    event.eventId = 1;
    event.sourceNodeId = sourceId;
    event.timestamp = 1000;
    event.eventType = EventType::MouseMove;
    event.payload = MouseMovePayload{10, 0};
    event.modifierState = ModifierState{false, false, false, false, false};

    auto result = injector.inject(event);
    CFX_TEST_CHECK(result.ok);
    CFX_TEST_CHECK(injector.totalInjected() == 1);
    CFX_TEST_CHECK(injector.totalRejected() == 0);
#else
    fprintf(stderr, "  [SKIP] Windows-only test\n");
#endif
}

static void test_win_event_injector_reject_unauthorized() {
    fprintf(stderr, "[TEST] test_win_event_injector_reject_unauthorized\n");
#ifdef _WIN32
    NodeId sourceId{1, 1};
    WinEventInjector injector(sourceId);

    CanonicalInputEvent event{};
    event.sourceNodeId = NodeId{2, 2};
    event.eventType = EventType::MouseMove;
    event.payload = MouseMovePayload{10, 0};

    auto result = injector.inject(event);
    CFX_TEST_CHECK(!result.ok);
    CFX_TEST_CHECK(injector.totalInjected() == 0);
    CFX_TEST_CHECK(injector.totalRejected() == 1);
#else
    fprintf(stderr, "  [SKIP] Windows-only test\n");
#endif
}

static void test_win_event_injector_batch() {
    fprintf(stderr, "[TEST] test_win_event_injector_batch\n");
#ifdef _WIN32
    NodeId sourceId{1, 1};
    WinEventInjector injector(sourceId);

    std::vector<CanonicalInputEvent> events(3);
    for (int i = 0; i < 3; ++i) {
        events[i].eventId = i + 1;
        events[i].sourceNodeId = sourceId;
        events[i].eventType = EventType::MouseMove;
        events[i].payload = MouseMovePayload{1, 0};
    }

    auto result = injector.injectBatch(events);
    CFX_TEST_CHECK(result.ok);
    CFX_TEST_CHECK(result.failedCount == 0);
    CFX_TEST_CHECK(injector.totalInjected() == 3);
#else
    fprintf(stderr, "  [SKIP] Windows-only test\n");
#endif
}

static void test_win_event_injector_release_all_pressed() {
    fprintf(stderr, "[TEST] test_win_event_injector_release_all_pressed\n");
#ifdef _WIN32
    WinEventInjector injector(NodeId{1, 1});

    PressedStateSnapshot snapshot{};
    snapshot.pressedMouseButtons.setPressed(MouseButton::Left);
    snapshot.pressedKeys.setPressed(65);

    auto result = injector.releaseAllPressed(snapshot);
    CFX_TEST_CHECK(result.releasedCount >= 2);
    CFX_TEST_CHECK(result.latencyMs <= 100);
#else
    fprintf(stderr, "  [SKIP] Windows-only test\n");
#endif
}

static void test_transport_impl_basic() {
    fprintf(stderr, "[TEST] test_transport_impl_basic\n");
    LoopbackTransportPair pair(11001, 11002);
    CFX_TEST_CHECK(pair.client.isConnected());
    CFX_TEST_CHECK(pair.server.isConnected());

    pair.client.disconnect();
    CFX_TEST_CHECK(!pair.client.isConnected());
}

static void test_transport_impl_send_backlog() {
    fprintf(stderr, "[TEST] test_transport_impl_send_backlog\n");

    TransportConfig cfg{};
    cfg.remoteHost = "127.0.0.1";
    cfg.inputPlanePort = 11003;
    cfg.controlPlanePort = 11004;
    TransportImpl unconnected(cfg);

    CanonicalInputEvent event{};
    event.eventType = EventType::MouseMove;
    event.payload = MouseMovePayload{10, 0};

    unconnected.send(event);
    CFX_TEST_CHECK(unconnected.backlog() == 1);
    CFX_TEST_CHECK(unconnected.totalSent() == 0);

    LoopbackTransportPair pair(11005, 11006);
    pair.client.send(event);
    CFX_TEST_CHECK(pair.client.totalSent() == 1);
}

static void test_e2e_pipeline_orchestrator_basic() {
    fprintf(stderr, "[TEST] test_e2e_pipeline_orchestrator_basic\n");
    LoopbackTransportPair pair(11007, 11008);
    MockClock clock;

    E2EPipelineConfig pipelineConfig{};
    pipelineConfig.localNodeId = NodeId{1, 1};
    pipelineConfig.eventIdSeed = 100;

    E2EPipelineOrchestrator orchestrator(pair.client, pair.client, clock, pipelineConfig);

    CFX_TEST_CHECK(!orchestrator.isRunning());
    CFX_TEST_CHECK(orchestrator.start());
    CFX_TEST_CHECK(orchestrator.isRunning());

    orchestrator.stop();
    CFX_TEST_CHECK(!orchestrator.isRunning());
}

static void test_e2e_pipeline_orchestrator_process_event() {
    fprintf(stderr, "[TEST] test_e2e_pipeline_orchestrator_process_event\n");
    LoopbackTransportPair pair(11009, 11010);
    MockClock clock;

    E2EPipelineConfig pipelineConfig{};
    pipelineConfig.localNodeId = NodeId{1, 1};

    E2EPipelineOrchestrator orchestrator(pair.client, pair.client, clock, pipelineConfig);
    orchestrator.start();

    CanonicalInputEvent event{};
    event.eventType = EventType::MouseMove;
    event.payload = MouseMovePayload{10, 0};

    orchestrator.processEvent(event);

    CFX_TEST_CHECK(orchestrator.totalProcessed() == 1);
    CFX_TEST_CHECK(orchestrator.totalTransportSent() == 1);

    orchestrator.stop();
}

static void test_e2e_pipeline_event_id_monotonic() {
    fprintf(stderr, "[TEST] test_e2e_pipeline_event_id_monotonic\n");
    LoopbackTransportPair pair(11011, 11012);
    MockClock clock;

    E2EPipelineConfig pipelineConfig{};
    pipelineConfig.localNodeId = NodeId{1, 1};
    pipelineConfig.eventIdSeed = 1000;

    E2EPipelineOrchestrator orchestrator(pair.client, pair.client, clock, pipelineConfig);
    orchestrator.start();

    CanonicalInputEvent event{};
    event.eventType = EventType::MouseMove;
    event.payload = MouseMovePayload{1, 0};

    orchestrator.processEvent(event);
    orchestrator.processEvent(event);
    orchestrator.processEvent(event);

    CFX_TEST_CHECK(orchestrator.totalProcessed() == 3);

    orchestrator.stop();
}

int main() {
    fprintf(stderr, "=== test_e2e_pipeline ===\n");

    test_win_event_injector_basic();
    test_win_event_injector_reject_unauthorized();
    test_win_event_injector_batch();
    test_win_event_injector_release_all_pressed();
    test_transport_impl_basic();
    test_transport_impl_send_backlog();
    test_e2e_pipeline_orchestrator_basic();
    test_e2e_pipeline_orchestrator_process_event();
    test_e2e_pipeline_event_id_monotonic();

    fprintf(stderr, "=== ALL PASS ===\n");
    return 0;
}
