#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "common/spsc_ring_buffer.hpp"
#include "mac/dual_channel_spsc.hpp"
#include "mac/authoritative_resync.hpp"

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

static RawInputEventFlat makeEvent(RawEventKind kind) {
    RawInputEventFlat e{};
    e.kind = static_cast<uint8_t>(kind);
    e.valid = 1;
    return e;
}

static void simulateCaptureThreadCycle(DualChannelSpsc& dc, AuthoritativeResync& resync) {
    if (dc.isStateChannelSaturated()) {
        if (resync.state() == ResyncState::Idle ||
            resync.state() == ResyncState::Recovered) {
            resync.resynchronize();
        }
        RawInputEventFlat flat{};
        while (dc.tryPopState(flat)) {}
        if (dc.stateEmpty()) {
            if (resync.state() == ResyncState::RecoveryPending) {
                resync.confirmRecovery();
            }
            dc.clearStateChannelSaturated();
        }
    }
}

static void test_saturation_to_resync_full_chain() {
    DualChannelSpsc dc;
    AuthoritativeResync resync;

    CFX_TEST_CHECK(resync.state() == ResyncState::Idle);

    for (uint64_t i = 0; i < SPSC_STATE_CAPACITY; ++i) {
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }
    CFX_TEST_CHECK(!dc.isStateChannelSaturated());

    auto result = dc.enqueue(makeEvent(RawEventKind::KeyPress));
    CFX_TEST_CHECK(result == DualChannelSpsc::ChannelResult::StateSaturated);
    CFX_TEST_CHECK(dc.isStateChannelSaturated());

    CFX_TEST_CHECK(resync.state() == ResyncState::Idle);

    simulateCaptureThreadCycle(dc, resync);

    CFX_TEST_CHECK(resync.state() == ResyncState::Recovered);
    CFX_TEST_CHECK(!dc.isStateChannelSaturated());
    CFX_TEST_CHECK(resync.resyncCount() == 1);
    CFX_TEST_CHECK(dc.stateEmpty());

    printf("  [PASS] test_saturation_to_resync_full_chain\n");
}

static void test_multiple_saturation_recovery_cycles() {
    DualChannelSpsc dc;
    AuthoritativeResync resync;

    for (int cycle = 0; cycle < 5; ++cycle) {
        for (uint64_t i = 0; i < SPSC_STATE_CAPACITY; ++i) {
            dc.enqueue(makeEvent(RawEventKind::KeyPress));
        }
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
        CFX_TEST_CHECK(dc.isStateChannelSaturated());

        simulateCaptureThreadCycle(dc, resync);

        CFX_TEST_CHECK(resync.state() == ResyncState::Recovered);
        CFX_TEST_CHECK(!dc.isStateChannelSaturated());
        CFX_TEST_CHECK(dc.stateEmpty());
    }

    CFX_TEST_CHECK(resync.resyncCount() == 5);

    printf("  [PASS] test_multiple_saturation_recovery_cycles (resyncCount=%llu)\n",
           (unsigned long long)resync.resyncCount());
}

static void test_data_channel_unaffected_by_state_saturation() {
    DualChannelSpsc dc;
    AuthoritativeResync resync;

    dc.enqueue(makeEvent(RawEventKind::MouseMove));
    dc.enqueue(makeEvent(RawEventKind::Wheel));

    for (uint64_t i = 0; i < SPSC_STATE_CAPACITY + 1; ++i) {
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }
    CFX_TEST_CHECK(dc.isStateChannelSaturated());

    CFX_TEST_CHECK(!dc.dataEmpty());

    simulateCaptureThreadCycle(dc, resync);

    CFX_TEST_CHECK(!dc.dataEmpty());
    CFX_TEST_CHECK(dc.stateEmpty());

    RawInputEventFlat out{};
    CFX_TEST_CHECK(dc.tryPopData(out));
    CFX_TEST_CHECK(static_cast<RawEventKind>(out.kind) == RawEventKind::MouseMove);
    CFX_TEST_CHECK(dc.tryPopData(out));
    CFX_TEST_CHECK(static_cast<RawEventKind>(out.kind) == RawEventKind::Wheel);

    printf("  [PASS] test_data_channel_unaffected_by_state_saturation\n");
}

static void test_resync_ground_truthModifiers() {
    AuthoritativeResync resync;

    ModifierState mods = resync.resynchronize();
    (void)mods;

    ModifierState stored = resync.groundTruthModifiers();
    CFX_TEST_CHECK(resync.state() == ResyncState::RecoveryPending);
    CFX_TEST_CHECK(resync.bitmapStale());

    resync.confirmRecovery();
    CFX_TEST_CHECK(resync.state() == ResyncState::Recovered);
    CFX_TEST_CHECK(!resync.bitmapStale());

    printf("  [PASS] test_resync_ground_truthModifiers\n");
}

static void test_saturation_during_active_capture() {
    DualChannelSpsc dc;
    AuthoritativeResync resync;

    for (uint64_t i = 0; i < 10; ++i) {
        dc.enqueue(makeEvent(RawEventKind::MouseMove));
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }

    RawInputEventFlat out{};
    int popped = 0;
    while (dc.tryPopData(out)) { ++popped; }
    while (dc.tryPopState(out)) { ++popped; }
    CFX_TEST_CHECK(popped == 20);

    for (uint64_t i = 0; i < SPSC_STATE_CAPACITY + 1; ++i) {
        dc.enqueue(makeEvent(RawEventKind::MouseButtonPress));
    }
    CFX_TEST_CHECK(dc.isStateChannelSaturated());

    simulateCaptureThreadCycle(dc, resync);

    CFX_TEST_CHECK(resync.state() == ResyncState::Recovered);
    CFX_TEST_CHECK(!dc.isStateChannelSaturated());

    dc.enqueue(makeEvent(RawEventKind::KeyPress));
    CFX_TEST_CHECK(!dc.isStateChannelSaturated());
    CFX_TEST_CHECK(dc.tryPopState(out));
    CFX_TEST_CHECK(static_cast<RawEventKind>(out.kind) == RawEventKind::KeyPress);

    printf("  [PASS] test_saturation_during_active_capture\n");
}

static void test_state_channel_no_drop_under_normal_load() {
    DualChannelSpsc dc;

    for (int burst = 0; burst < 10; ++burst) {
        for (int i = 0; i < 6; ++i) {
            auto result = dc.enqueue(makeEvent(RawEventKind::KeyPress));
            CFX_TEST_CHECK(result == DualChannelSpsc::ChannelResult::Success);
        }
        RawInputEventFlat out{};
        while (dc.tryPopState(out)) {}
    }

    CFX_TEST_CHECK(!dc.isStateChannelSaturated());
    CFX_TEST_CHECK(dc.stateChannelSaturatedCount() == 0);

    printf("  [PASS] test_state_channel_no_drop_under_normal_load\n");
}

int main() {
    printf("=== CF2 Group 3 STATE Saturation + RECOVERY Integration Tests ===\n\n");

    printf("[Saturation → Resync → RECOVERY]\n");
    test_saturation_to_resync_full_chain();
    test_multiple_saturation_recovery_cycles();
    test_data_channel_unaffected_by_state_saturation();
    test_resync_ground_truthModifiers();
    test_saturation_during_active_capture();
    test_state_channel_no_drop_under_normal_load();

    printf("\n=== All tests passed ===\n");
    return 0;
}