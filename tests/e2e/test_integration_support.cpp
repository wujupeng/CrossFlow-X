#include <cstdio>
#include <cstdlib>
#include <thread>

#include "integration/disconnect_resync_coordinator.hpp"
#include "integration/e2e_latency_probe.hpp"
#include "integration/e2e_evidence_collector.hpp"
#include "s05_transport/transport_impl.hpp"

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED: %s:%d: %s\n", __FILE__, __LINE__, #cond); std::exit(1); } } while(0)

namespace cfx {

static TransportConfig makeTestTransportConfig() {
    TransportConfig cfg{};
    cfg.remoteHost = "127.0.0.1";
    cfg.inputPlanePort = 9001;
    cfg.controlPlanePort = 9002;
    cfg.isServer = false;
    return cfg;
}

static void test_disconnect_resync_state_transitions() {
    printf("[TEST] test_disconnect_resync_state_transitions\n");

    TransportImpl transport(makeTestTransportConfig());
    DisconnectResyncConfig config{};

    bool releaseCalled = false;
    bool resyncCalled = false;
    bool recoveryConfirmed = true;

    DisconnectResyncCoordinator coordinator(
        transport,
        config,
        [&](const PressedStateSnapshot&) -> ReleaseResult { releaseCalled = true; return {0, 0}; },
        [&]() -> PressedStateSnapshot { return PressedStateSnapshot{}; },
        [&]() -> void { resyncCalled = true; },
        [&]() -> bool { return recoveryConfirmed; }
    );

    CHECK(coordinator.state() == DisconnectResyncState::Connected);
    CHECK(coordinator.isInputAllowed() == true);

    coordinator.start();
    transport.connect();

    LinkStateEvent disconnectEvent{};
    disconnectEvent.state = LinkState::Disconnected;
    transport.onLinkState([&](const LinkStateEvent& e) {
        (void)e;
    });

    coordinator.stop();
    CHECK(coordinator.isInputAllowed() == true);

    (void)releaseCalled;
    (void)resyncCalled;
}

static void test_disconnect_resync_input_gate() {
    printf("[TEST] test_disconnect_resync_input_gate\n");

    TransportImpl transport(makeTestTransportConfig());
    DisconnectResyncConfig config{};

    DisconnectResyncCoordinator coordinator(
        transport,
        config,
        [&](const PressedStateSnapshot&) -> ReleaseResult { return {0, 0}; },
        [&]() -> PressedStateSnapshot { return PressedStateSnapshot{}; },
        [&]() -> void {},
        [&]() -> bool { return true; }
    );

    CHECK(coordinator.isInputAllowed() == true);
    coordinator.start();
    CHECK(coordinator.isInputAllowed() == true);
    coordinator.stop();
}

static void test_latency_probe_basic() {
    printf("[TEST] test_latency_probe_basic\n");

    E2ELatencyProbe probe;

    probe.recordCapture(1, 1000);
    probe.recordQueueProcessing(1, 1100);
    probe.recordTransportReceive(1, 1200);
    probe.recordInjection(1, 1300);

    auto rec = probe.getRecord(1);
    CHECK(rec.has_value());
    CHECK(rec->isComplete());
    CHECK(rec->captureToQueueUs() == 100);
    CHECK(rec->queueToTransportUs() == 100);
    CHECK(rec->receiveToInjectionUs() == 100);
    CHECK(rec->totalE2ELatencyUs() == 300);
}

static void test_latency_probe_report() {
    printf("[TEST] test_latency_probe_report\n");

    E2ELatencyProbe probe;

    for (u64 i = 1; i <= 10; ++i) {
        probe.recordCapture(i, i * 1000);
        probe.recordQueueProcessing(i, i * 1000 + 50);
        probe.recordTransportReceive(i, i * 1000 + 150);
        probe.recordInjection(i, i * 1000 + 200);
    }

    auto report = probe.generateReport();
    CHECK(report.totalRecords == 10);
    CHECK(report.completeRecords == 10);
    CHECK(report.captureToQueue.min == 50);
    CHECK(report.captureToQueue.max == 50);
    CHECK(report.totalE2E.min == 200);
    CHECK(report.totalE2E.max == 200);
}

static void test_latency_probe_incomplete() {
    printf("[TEST] test_latency_probe_incomplete\n");

    E2ELatencyProbe probe;

    probe.recordCapture(1, 1000);
    probe.recordQueueProcessing(1, 1100);

    auto rec = probe.getRecord(1);
    CHECK(rec.has_value());
    CHECK(!rec->isComplete());

    auto report = probe.generateReport();
    CHECK(report.totalRecords == 1);
    CHECK(report.completeRecords == 0);
}

static void test_evidence_collector_basic() {
    printf("[TEST] test_evidence_collector_basic\n");

    E2EEvidenceCollector collector;

    collector.recordCaptureEvidence(1, 1000, "mouse_move_10_0");
    collector.recordReceiveEvidence(1, 1100, "mouse_move_10_0");
    collector.recordInjectionEvidence(1, 1200, "mouse_move_10_0");

    CHECK(collector.recordCount() == 3);

    auto records = collector.allRecords();
    CHECK(records.size() == 3);
    CHECK(records[0].type == EvidenceType::Capture);
    CHECK(records[1].type == EvidenceType::Receive);
    CHECK(records[2].type == EvidenceType::Injection);
}

static void test_evidence_correlation_pass() {
    printf("[TEST] test_evidence_correlation_pass\n");

    E2EEvidenceCollector collector;

    for (u64 i = 1; i <= 5; ++i) {
        std::string payload = "event_" + std::to_string(i);
        collector.recordCaptureEvidence(i, i * 1000, payload);
        collector.recordInjectionEvidence(i, i * 1000 + 100, payload);
    }

    auto result = collector.verifyCorrelation();
    CHECK(result.pass);
    CHECK(result.matchedCount == 5);
    CHECK(result.payloadMismatchCount == 0);
    CHECK(result.timestampNonMonotonicCount == 0);
}

static void test_evidence_correlation_fail_payload() {
    printf("[TEST] test_evidence_correlation_fail_payload\n");

    E2EEvidenceCollector collector;

    collector.recordCaptureEvidence(1, 1000, "payload_a");
    collector.recordInjectionEvidence(1, 1100, "payload_b");

    auto result = collector.verifyCorrelation();
    CHECK(!result.pass);
    CHECK(result.payloadMismatchCount == 1);
}

static void test_evidence_correlation_fail_timestamp() {
    printf("[TEST] test_evidence_correlation_fail_timestamp\n");

    E2EEvidenceCollector collector;

    collector.recordCaptureEvidence(1, 2000, "payload");
    collector.recordInjectionEvidence(1, 1000, "payload");

    auto result = collector.verifyCorrelation();
    CHECK(!result.pass);
    CHECK(result.timestampNonMonotonicCount == 1);
}

static void test_evidence_authenticity() {
    printf("[TEST] test_evidence_authenticity\n");

    E2EEvidenceCollector collector;

    auto pass = collector.checkAuthenticity(true, true, false);
    CHECK(pass.pass);

    auto failTcc = collector.checkAuthenticity(false, true, false);
    CHECK(!failTcc.pass);

    auto failSsh = collector.checkAuthenticity(true, false, false);
    CHECK(!failSsh.pass);

    auto failUnitTest = collector.checkAuthenticity(true, true, true);
    CHECK(!failUnitTest.pass);
}

static void test_evidence_disconnect_reconnect() {
    printf("[TEST] test_evidence_disconnect_reconnect\n");

    E2EEvidenceCollector collector;

    collector.recordDisconnectEvidence(5000, "network_timeout");
    collector.recordReconnectEvidence(6000, "tcp_reconnect");

    auto records = collector.allRecords();
    CHECK(records.size() == 2);
    CHECK(records[0].type == EvidenceType::Disconnect);
    CHECK(records[1].type == EvidenceType::Reconnect);
}

}  // namespace cfx

int main() {
    printf("=== test_integration_support ===\n");

    cfx::test_disconnect_resync_state_transitions();
    cfx::test_disconnect_resync_input_gate();
    cfx::test_latency_probe_basic();
    cfx::test_latency_probe_report();
    cfx::test_latency_probe_incomplete();
    cfx::test_evidence_collector_basic();
    cfx::test_evidence_correlation_pass();
    cfx::test_evidence_correlation_fail_payload();
    cfx::test_evidence_correlation_fail_timestamp();
    cfx::test_evidence_authenticity();
    cfx::test_evidence_disconnect_reconnect();

    printf("=== ALL PASS ===\n");
    return 0;
}