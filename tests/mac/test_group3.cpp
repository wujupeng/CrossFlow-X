#include <atomic>
#include <thread>
#include <vector>

#include "common/spsc_ring_buffer.hpp"
#include "mac/dual_channel_spsc.hpp"
#include "mac/authoritative_resync.hpp"

#include <cassert>
#include <cstdio>

using namespace cfx;

static void test_dual_channel_dispatch() {
    DualChannelSpsc queue;

    RawInputEventFlat mouseMove{};
    mouseMove.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
    mouseMove.deltaX = 10;
    mouseMove.deltaY = 20;
    mouseMove.valid = 1;

    RawInputEventFlat keyPress{};
    keyPress.kind = static_cast<uint8_t>(RawEventKind::KeyPress);
    keyPress.keyCode = 65;
    keyPress.valid = 1;

    RawInputEventFlat mouseDown{};
    mouseDown.kind = static_cast<uint8_t>(RawEventKind::MouseButtonPress);
    mouseDown.button = static_cast<uint8_t>(MouseButton::Left);
    mouseDown.valid = 1;

    RawInputEventFlat wheel{};
    wheel.kind = static_cast<uint8_t>(RawEventKind::Wheel);
    wheel.wheelDelta = 5;
    wheel.valid = 1;

    assert(queue.enqueue(mouseMove) == DualChannelSpsc::ChannelResult::Success);
    assert(queue.enqueue(wheel) == DualChannelSpsc::ChannelResult::Success);
    assert(queue.enqueue(keyPress) == DualChannelSpsc::ChannelResult::Success);
    assert(queue.enqueue(mouseDown) == DualChannelSpsc::ChannelResult::Success);

    assert(!queue.dataEmpty());
    assert(!queue.stateEmpty());

    RawInputEventFlat out{};
    assert(queue.tryPopData(out));
    assert(out.kind == static_cast<uint8_t>(RawEventKind::MouseMove));
    assert(out.deltaX == 10);

    assert(queue.tryPopData(out));
    assert(out.kind == static_cast<uint8_t>(RawEventKind::Wheel));

    assert(queue.dataEmpty());

    assert(queue.tryPopState(out));
    assert(out.kind == static_cast<uint8_t>(RawEventKind::KeyPress));
    assert(out.keyCode == 65);

    assert(queue.tryPopState(out));
    assert(out.kind == static_cast<uint8_t>(RawEventKind::MouseButtonPress));

    assert(queue.stateEmpty());

    printf("  [PASS] test_dual_channel_dispatch\n");
}

static void test_dual_channel_capacity() {
    static_assert(DualChannelSpsc::dataCapacity() == 256, "SPSC_DATA must be 256");
    static_assert(DualChannelSpsc::stateCapacity() == 64, "SPSC_STATE must be 64");

    printf("  [PASS] test_dual_channel_capacity\n");
}

static void test_dual_channel_data_drop_oldest() {
    DualChannelSpsc queue;

    for (int i = 0; i < 300; ++i) {
        RawInputEventFlat e{};
        e.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
        e.deltaX = i;
        e.valid = 1;
        queue.enqueue(e);
    }

    assert(queue.droppedOldestCount() >= 44);

    int popped = 0;
    RawInputEventFlat out{};
    while (queue.tryPopData(out)) {
        ++popped;
    }
    assert(popped <= 256);

    printf("  [PASS] test_dual_channel_data_drop_oldest\n");
}

static void test_dual_channel_state_saturation() {
    DualChannelSpsc queue;

    for (int i = 0; i < 64; ++i) {
        RawInputEventFlat e{};
        e.kind = static_cast<uint8_t>(RawEventKind::KeyPress);
        e.keyCode = static_cast<uint16_t>(i);
        e.valid = 1;
        assert(queue.enqueue(e) == DualChannelSpsc::ChannelResult::Success);
    }

    assert(!queue.isStateChannelSaturated());

    RawInputEventFlat extra{};
    extra.kind = static_cast<uint8_t>(RawEventKind::KeyRelease);
    extra.keyCode = 99;
    extra.valid = 1;
    assert(queue.enqueue(extra) == DualChannelSpsc::ChannelResult::StateSaturated);

    assert(queue.isStateChannelSaturated());
    assert(queue.stateChannelSaturatedCount() >= 1);

    printf("  [PASS] test_dual_channel_state_saturation\n");
}

static void test_dual_channel_saturation_non_blocking() {
    DualChannelSpsc queue;

    for (int i = 0; i < 100; ++i) {
        RawInputEventFlat e{};
        e.kind = static_cast<uint8_t>(RawEventKind::KeyPress);
        e.keyCode = static_cast<uint16_t>(i);
        e.valid = 1;
        (void)queue.enqueue(e);
    }

    assert(queue.isStateChannelSaturated());
    assert(queue.stateChannelSaturatedCount() >= 36);

    queue.clearStateChannelSaturated();
    assert(!queue.isStateChannelSaturated());

    printf("  [PASS] test_dual_channel_saturation_non_blocking\n");
}

static void test_dual_channel_no_heap_allocation() {
    static_assert(std::is_trivially_copyable_v<RawInputEventFlat>,
                  "RawInputEventFlat must be trivially copyable (no heap)");

    DualChannelSpsc queue;

    RawInputEventFlat e{};
    e.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
    e.valid = 1;
    queue.enqueue(e);

    RawInputEventFlat out{};
    queue.tryPopData(out);
    assert(out.valid == 1);

    printf("  [PASS] test_dual_channel_no_heap_allocation\n");
}

static void test_dual_channel_concurrent() {
    DualChannelSpsc queue;
    constexpr int N = 500;

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i) {
            RawInputEventFlat e{};
            e.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
            e.deltaX = i;
            e.valid = 1;
            queue.enqueue(e);
        }
    });

    std::atomic<int> popped{0};
    std::atomic<bool> producerDone{false};
    std::thread consumer([&]() {
        RawInputEventFlat out{};
        while (!producerDone.load() || !queue.dataEmpty()) {
            if (queue.tryPopData(out)) {
                popped.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    producer.join();
    producerDone.store(true, std::memory_order_release);
    consumer.join();

    assert(popped.load() <= N);

    printf("  [PASS] test_dual_channel_concurrent\n");
}

static void test_authoritative_resync_initial_state() {
    AuthoritativeResync resync;
    assert(resync.state() == ResyncState::Idle);
    assert(!resync.isResynchronizing());
    assert(!resync.bitmapStale());
    assert(resync.resyncCount() == 0);

    printf("  [PASS] test_authoritative_resync_initial_state\n");
}

static void test_authoritative_resync_trigger() {
    AuthoritativeResync resync;
    resync.trigger();
    assert(resync.isResynchronizing());
    assert(resync.state() == ResyncState::Resynchronizing);

    printf("  [PASS] test_authoritative_resync_trigger\n");
}

static void test_authoritative_resync_resynchronize() {
    AuthoritativeResync resync;

    const ModifierState result = resync.resynchronize();
    (void)result;

    assert(resync.state() == ResyncState::RecoveryPending);
    assert(resync.bitmapStale());
    assert(resync.resyncCount() == 1);

    printf("  [PASS] test_authoritative_resync_resynchronize\n");
}

static void test_authoritative_resync_mark_bitmap_stale() {
    AuthoritativeResync resync;

    PressedStateSnapshot snapshot{};
    snapshot.stale = false;

    resync.markBitmapStale(snapshot);
    assert(snapshot.stale == true);
    assert(resync.bitmapStale());

    printf("  [PASS] test_authoritative_resync_mark_bitmap_stale\n");
}

static void test_authoritative_resync_confirm_recovery() {
    AuthoritativeResync resync;

    resync.resynchronize();
    assert(resync.state() == ResyncState::RecoveryPending);

    assert(resync.confirmRecovery());
    assert(resync.state() == ResyncState::Recovered);
    assert(!resync.bitmapStale());

    assert(!resync.confirmRecovery());

    printf("  [PASS] test_authoritative_resync_confirm_recovery\n");
}

static void test_authoritative_resync_reset() {
    AuthoritativeResync resync;

    resync.resynchronize();
    resync.confirmRecovery();
    assert(resync.state() == ResyncState::Recovered);

    resync.reset();
    assert(resync.state() == ResyncState::Idle);
    assert(!resync.bitmapStale());

    printf("  [PASS] test_authoritative_resync_reset\n");
}

static void test_authoritative_resync_multiple_cycles() {
    AuthoritativeResync resync;

    for (int i = 0; i < 5; ++i) {
        resync.resynchronize();
        assert(resync.state() == ResyncState::RecoveryPending);
        resync.confirmRecovery();
        assert(resync.state() == ResyncState::Recovered);
        resync.reset();
        assert(resync.state() == ResyncState::Idle);
    }

    assert(resync.resyncCount() == 5);

    printf("  [PASS] test_authoritative_resync_multiple_cycles\n");
}

int main() {
    printf("=== CF2 Group 3 Tests ===\n\n");

    printf("[DualChannelSpsc]\n");
    test_dual_channel_dispatch();
    test_dual_channel_capacity();
    test_dual_channel_data_drop_oldest();
    test_dual_channel_state_saturation();
    test_dual_channel_saturation_non_blocking();
    test_dual_channel_no_heap_allocation();
    test_dual_channel_concurrent();

    printf("\n[AuthoritativeResync]\n");
    test_authoritative_resync_initial_state();
    test_authoritative_resync_trigger();
    test_authoritative_resync_resynchronize();
    test_authoritative_resync_mark_bitmap_stale();
    test_authoritative_resync_confirm_recovery();
    test_authoritative_resync_reset();
    test_authoritative_resync_multiple_cycles();

    printf("\n=== All tests passed ===\n");
    return 0;
}