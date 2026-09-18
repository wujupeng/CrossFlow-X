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

static void test_callback_boundary_no_resync_in_callback() {
    DualChannelSpsc dc;
    AuthoritativeResync resync;

    for (int i = 0; i < 100; ++i) {
        dc.enqueue(makeEvent(RawEventKind::MouseMove));
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
        dc.enqueue(makeEvent(RawEventKind::Wheel));
        dc.enqueue(makeEvent(RawEventKind::KeyRelease));
    }

    CFX_TEST_CHECK(resync.state() == ResyncState::Idle);
    CFX_TEST_CHECK(!resync.isResynchronizing());
    CFX_TEST_CHECK(resync.resyncCount() == 0);

    printf("  [PASS] test_callback_boundary_no_resync_in_callback\n");
}

static void test_callback_boundary_no_blocking_on_saturation() {
    DualChannelSpsc dc;

    for (uint64_t i = 0; i < SPSC_STATE_CAPACITY; ++i) {
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    CFX_TEST_CHECK(us < 50000);
    CFX_TEST_CHECK(dc.isStateChannelSaturated());

    printf("  [PASS] test_callback_boundary_no_blocking_on_saturation (%lld us for 10000 overflow)\n",
           (long long)us);
}

static void test_callback_boundary_enqueue_is_only_callback_action() {
    DualChannelSpsc dc;
    AuthoritativeResync resync;

    dc.enqueue(makeEvent(RawEventKind::MouseMove));
    dc.enqueue(makeEvent(RawEventKind::KeyPress));

    CFX_TEST_CHECK(!dc.dataEmpty());
    CFX_TEST_CHECK(!dc.stateEmpty());
    CFX_TEST_CHECK(resync.state() == ResyncState::Idle);
    CFX_TEST_CHECK(!resync.bitmapStale());

    printf("  [PASS] test_callback_boundary_enqueue_is_only_callback_action\n");
}

static void test_callback_boundary_data_state_separation() {
    DualChannelSpsc dc;

    dc.enqueue(makeEvent(RawEventKind::MouseMove));
    dc.enqueue(makeEvent(RawEventKind::Wheel));
    dc.enqueue(makeEvent(RawEventKind::KeyPress));
    dc.enqueue(makeEvent(RawEventKind::KeyRelease));
    dc.enqueue(makeEvent(RawEventKind::MouseButtonPress));
    dc.enqueue(makeEvent(RawEventKind::MouseButtonRelease));

    int dataCount = 0, stateCount = 0;
    RawInputEventFlat out{};
    while (dc.tryPopData(out)) { ++dataCount; }
    while (dc.tryPopState(out)) { ++stateCount; }

    CFX_TEST_CHECK(dataCount == 2);
    CFX_TEST_CHECK(stateCount == 4);

    printf("  [PASS] test_callback_boundary_data_state_separation (data=%d, state=%d)\n",
           dataCount, stateCount);
}

static void test_capture_thread_resync_not_callback() {
    DualChannelSpsc dc;
    AuthoritativeResync resync;

    for (uint64_t i = 0; i < SPSC_STATE_CAPACITY + 5; ++i) {
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }

    CFX_TEST_CHECK(dc.isStateChannelSaturated());
    CFX_TEST_CHECK(resync.state() == ResyncState::Idle);

    if (dc.isStateChannelSaturated()) {
        if (resync.state() == ResyncState::Idle) {
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

    CFX_TEST_CHECK(resync.state() == ResyncState::Recovered);
    CFX_TEST_CHECK(!dc.isStateChannelSaturated());
    CFX_TEST_CHECK(resync.resyncCount() == 1);

    printf("  [PASS] test_capture_thread_resync_not_callback\n");
}

static void test_callback_boundary_rapid_fire_nonblocking() {
    DualChannelSpsc dc;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; ++i) {
        dc.enqueue(makeEvent(RawEventKind::MouseMove));
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    CFX_TEST_CHECK(us < 100000);

    printf("  [PASS] test_callback_boundary_rapid_fire_nonblocking (%lld us for 200K enqueues)\n",
           (long long)us);
}

int main() {
    printf("=== CF2 Group 3 Callback Boundary Integration Tests ===\n\n");

    printf("[Callback Boundary]\n");
    test_callback_boundary_no_resync_in_callback();
    test_callback_boundary_no_blocking_on_saturation();
    test_callback_boundary_enqueue_is_only_callback_action();
    test_callback_boundary_data_state_separation();
    test_capture_thread_resync_not_callback();
    test_callback_boundary_rapid_fire_nonblocking();

    printf("\n=== All tests passed ===\n");
    return 0;
}