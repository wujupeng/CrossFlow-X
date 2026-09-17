#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

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
    e.platformTime = 0;
    e.modifierFlags = 0;
    e.kind = static_cast<uint8_t>(kind);
    e.deltaX = 0;
    e.deltaY = 0;
    e.button = 0;
    e.wheelDelta = 0;
    e.wheelAxis = 0;
    e.keyCode = 0;
    e.valid = 1;
    return e;
}

static void test_dual_channel_capacity() {
    DualChannelSpsc dc;
    CFX_TEST_CHECK(dc.dataCapacity() == SPSC_DATA_CAPACITY);
    CFX_TEST_CHECK(dc.stateCapacity() == SPSC_STATE_CAPACITY);
    CFX_TEST_CHECK(SPSC_DATA_CAPACITY == 256);
    CFX_TEST_CHECK(SPSC_STATE_CAPACITY == 64);
    printf("  [PASS] test_dual_channel_capacity (DATA=%llu, STATE=%llu)\n",
           (unsigned long long)dc.dataCapacity(),
           (unsigned long long)dc.stateCapacity());
}

static void test_dual_channel_data_routing() {
    DualChannelSpsc dc;

    dc.enqueue(makeEvent(RawEventKind::MouseMove));
    dc.enqueue(makeEvent(RawEventKind::Wheel));

    CFX_TEST_CHECK(!dc.dataEmpty());
    CFX_TEST_CHECK(dc.stateEmpty());

    RawInputEventFlat out{};
    CFX_TEST_CHECK(dc.tryPopData(out));
    CFX_TEST_CHECK(static_cast<RawEventKind>(out.kind) == RawEventKind::MouseMove);
    CFX_TEST_CHECK(dc.tryPopData(out));
    CFX_TEST_CHECK(static_cast<RawEventKind>(out.kind) == RawEventKind::Wheel);
    CFX_TEST_CHECK(dc.dataEmpty());

    printf("  [PASS] test_dual_channel_data_routing\n");
}

static void test_dual_channel_state_routing() {
    DualChannelSpsc dc;

    dc.enqueue(makeEvent(RawEventKind::KeyPress));
    dc.enqueue(makeEvent(RawEventKind::KeyRelease));
    dc.enqueue(makeEvent(RawEventKind::MouseButtonPress));
    dc.enqueue(makeEvent(RawEventKind::MouseButtonRelease));

    CFX_TEST_CHECK(dc.dataEmpty());
    CFX_TEST_CHECK(!dc.stateEmpty());

    int count = 0;
    RawInputEventFlat out{};
    while (dc.tryPopState(out)) { ++count; }
    CFX_TEST_CHECK(count == 4);

    printf("  [PASS] test_dual_channel_state_routing\n");
}

static void test_dual_channel_data_drop_oldest() {
    DualChannelSpsc dc;

    for (uint64_t i = 0; i < SPSC_DATA_CAPACITY + 10; ++i) {
        RawInputEventFlat e = makeEvent(RawEventKind::MouseMove);
        e.platformTime = i;
        dc.enqueue(e);
    }

    CFX_TEST_CHECK(dc.droppedOldestCount() >= 10);

    uint64_t minTime = UINT64_MAX;
    RawInputEventFlat out{};
    while (dc.tryPopData(out)) {
        if (out.platformTime < minTime) minTime = out.platformTime;
    }
    CFX_TEST_CHECK(minTime >= 10);

    printf("  [PASS] test_dual_channel_data_drop_oldest (dropped=%llu)\n",
           (unsigned long long)dc.droppedOldestCount());
}

static void test_dual_channel_state_saturation() {
    DualChannelSpsc dc;

    for (uint64_t i = 0; i < SPSC_STATE_CAPACITY; ++i) {
        auto result = dc.enqueue(makeEvent(RawEventKind::KeyPress));
        CFX_TEST_CHECK(result == DualChannelSpsc::ChannelResult::Success);
    }
    CFX_TEST_CHECK(!dc.isStateChannelSaturated());

    auto result = dc.enqueue(makeEvent(RawEventKind::KeyPress));
    CFX_TEST_CHECK(result == DualChannelSpsc::ChannelResult::StateSaturated);
    CFX_TEST_CHECK(dc.isStateChannelSaturated());
    CFX_TEST_CHECK(dc.stateChannelSaturatedCount() >= 1);

    printf("  [PASS] test_dual_channel_state_saturation (saturatedCount=%llu)\n",
           (unsigned long long)dc.stateChannelSaturatedCount());
}

static void test_dual_channel_saturation_nonblocking() {
    DualChannelSpsc dc;

    for (uint64_t i = 0; i < SPSC_STATE_CAPACITY; ++i) {
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    CFX_TEST_CHECK(elapsed.count() < 10000);
    CFX_TEST_CHECK(dc.isStateChannelSaturated());

    printf("  [PASS] test_dual_channel_saturation_nonblocking (%lld us for 1000 overflow enqueues)\n",
           (long long)elapsed.count());
}

static void test_dual_channel_mixed_routing() {
    DualChannelSpsc dc;

    dc.enqueue(makeEvent(RawEventKind::MouseMove));
    dc.enqueue(makeEvent(RawEventKind::KeyPress));
    dc.enqueue(makeEvent(RawEventKind::Wheel));
    dc.enqueue(makeEvent(RawEventKind::KeyRelease));
    dc.enqueue(makeEvent(RawEventKind::MouseButtonPress));

    CFX_TEST_CHECK(!dc.dataEmpty());
    CFX_TEST_CHECK(!dc.stateEmpty());

    int dataCount = 0, stateCount = 0;
    RawInputEventFlat out{};
    while (dc.tryPopData(out)) { ++dataCount; }
    while (dc.tryPopState(out)) { ++stateCount; }
    CFX_TEST_CHECK(dataCount == 2);
    CFX_TEST_CHECK(stateCount == 3);

    printf("  [PASS] test_dual_channel_mixed_routing (data=%d, state=%d)\n", dataCount, stateCount);
}

static void test_dual_channel_clear_saturation() {
    DualChannelSpsc dc;

    for (uint64_t i = 0; i < SPSC_STATE_CAPACITY + 1; ++i) {
        dc.enqueue(makeEvent(RawEventKind::KeyPress));
    }
    CFX_TEST_CHECK(dc.isStateChannelSaturated());

    dc.clearStateChannelSaturated();
    CFX_TEST_CHECK(!dc.isStateChannelSaturated());

    printf("  [PASS] test_dual_channel_clear_saturation\n");
}

static void test_authoritative_resync_initial_state() {
    AuthoritativeResync ar;
    CFX_TEST_CHECK(ar.state() == ResyncState::Idle);
    CFX_TEST_CHECK(!ar.isResynchronizing());
    CFX_TEST_CHECK(!ar.bitmapStale());
    CFX_TEST_CHECK(ar.resyncCount() == 0);
    printf("  [PASS] test_authoritative_resync_initial_state\n");
}

static void test_authoritative_resync_trigger() {
    AuthoritativeResync ar;
    ar.trigger();
    CFX_TEST_CHECK(ar.state() == ResyncState::Resynchronizing);
    CFX_TEST_CHECK(ar.isResynchronizing());
    printf("  [PASS] test_authoritative_resync_trigger\n");
}

static void test_authoritative_resync_resynchronize() {
    AuthoritativeResync ar;

    ModifierState mods = ar.resynchronize();

    CFX_TEST_CHECK(ar.state() == ResyncState::RecoveryPending);
    CFX_TEST_CHECK(ar.bitmapStale());
    CFX_TEST_CHECK(ar.resyncCount() == 1);
    (void)mods;

    printf("  [PASS] test_authoritative_resync_resynchronize\n");
}

static void test_authoritative_resync_confirm_recovery() {
    AuthoritativeResync ar;

    ar.resynchronize();
    CFX_TEST_CHECK(ar.state() == ResyncState::RecoveryPending);

    bool confirmed = ar.confirmRecovery();
    CFX_TEST_CHECK(confirmed);
    CFX_TEST_CHECK(ar.state() == ResyncState::Recovered);
    CFX_TEST_CHECK(!ar.bitmapStale());

    bool secondConfirm = ar.confirmRecovery();
    CFX_TEST_CHECK(!secondConfirm);

    printf("  [PASS] test_authoritative_resync_confirm_recovery\n");
}

static void test_authoritative_resync_reset() {
    AuthoritativeResync ar;

    ar.resynchronize();
    ar.confirmRecovery();
    CFX_TEST_CHECK(ar.state() == ResyncState::Recovered);

    ar.reset();
    CFX_TEST_CHECK(ar.state() == ResyncState::Idle);
    CFX_TEST_CHECK(!ar.bitmapStale());

    printf("  [PASS] test_authoritative_resync_reset\n");
}

static void test_authoritative_resync_mark_bitmap_stale() {
    AuthoritativeResync ar;
    PressedStateSnapshot snapshot{};

    CFX_TEST_CHECK(!ar.bitmapStale());
    ar.markBitmapStale(snapshot);
    CFX_TEST_CHECK(ar.bitmapStale());
    CFX_TEST_CHECK(snapshot.stale);

    printf("  [PASS] test_authoritative_resync_mark_bitmap_stale\n");
}

static void test_authoritative_resync_multiple_cycles() {
    AuthoritativeResync ar;

    for (int i = 0; i < 5; ++i) {
        ar.resynchronize();
        ar.confirmRecovery();
        ar.reset();
    }
    CFX_TEST_CHECK(ar.resyncCount() == 5);
    CFX_TEST_CHECK(ar.state() == ResyncState::Idle);

    printf("  [PASS] test_authoritative_resync_multiple_cycles (resyncCount=%llu)\n",
           (unsigned long long)ar.resyncCount());
}

int main() {
    printf("=== CF2 Group 3 DualChannelSpsc + AuthoritativeResync Tests ===\n\n");

    printf("[DualChannelSpsc]\n");
    test_dual_channel_capacity();
    test_dual_channel_data_routing();
    test_dual_channel_state_routing();
    test_dual_channel_data_drop_oldest();
    test_dual_channel_state_saturation();
    test_dual_channel_saturation_nonblocking();
    test_dual_channel_mixed_routing();
    test_dual_channel_clear_saturation();

    printf("\n[AuthoritativeResync]\n");
    test_authoritative_resync_initial_state();
    test_authoritative_resync_trigger();
    test_authoritative_resync_resynchronize();
    test_authoritative_resync_confirm_recovery();
    test_authoritative_resync_reset();
    test_authoritative_resync_mark_bitmap_stale();
    test_authoritative_resync_multiple_cycles();

    printf("\n=== All tests passed ===\n");
    return 0;
}