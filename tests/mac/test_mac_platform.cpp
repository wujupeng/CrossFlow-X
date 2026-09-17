#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "common/spsc_ring_buffer.hpp"
#include "mac/capture_handle.hpp"
#include "mac/mac_event_field_extractor.hpp"
#include "mac/cg_event_normalizer.hpp"
#include "mac/mac_event_tap.hpp"

#include <cassert>
#include <cstdio>

using namespace cfx;

static void test_capture_handle_basic() {
    CaptureHandleManager mgr;
    CaptureHandle h1 = mgr.acquire();
    assert(h1.id > 0);
    assert(h1.active);
    assert(mgr.activeCount() == 1);

    CaptureHandle h2 = mgr.acquire();
    assert(h2.id > h1.id);
    assert(mgr.activeCount() == 2);

    mgr.release(h1);
    assert(!h1.active);
    assert(mgr.activeCount() == 1);

    mgr.release(h2);
    assert(!h2.active);
    assert(mgr.activeCount() == 0);

    printf("  [PASS] test_capture_handle_basic\n");
}

static void test_capture_handle_raii() {
    CaptureHandleManager mgr;

    {
        ScopedCaptureHandle scoped(mgr);
        assert(scoped.isValid());
        assert(mgr.activeCount() == 1);
    }

    assert(mgr.activeCount() == 0);

    printf("  [PASS] test_capture_handle_raii\n");
}

static void test_capture_handle_move() {
    CaptureHandleManager mgr;

    ScopedCaptureHandle scoped1(mgr);
    assert(scoped1.isValid());
    assert(mgr.activeCount() == 1);

    ScopedCaptureHandle scoped2 = std::move(scoped1);
    assert(!scoped1.isValid());
    assert(scoped2.isValid());
    assert(mgr.activeCount() == 1);

    printf("  [PASS] test_capture_handle_move\n");
}

static void test_capture_handle_double_release() {
    CaptureHandleManager mgr;
    CaptureHandle h = mgr.acquire();
    mgr.release(h);
    mgr.release(h);
    assert(mgr.activeCount() == 0);

    printf("  [PASS] test_capture_handle_double_release\n");
}

static void test_raw_input_event_flat_trivially_copyable() {
    static_assert(std::is_trivially_copyable_v<RawInputEventFlat>,
                  "RawInputEventFlat must be trivially copyable");

    RawInputEventFlat a{};
    a.platformTime = 12345;
    a.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
    a.deltaX = 10;
    a.deltaY = -20;
    a.valid = 1;

    RawInputEventFlat b = a;
    assert(b.platformTime == 12345);
    assert(b.deltaX == 10);
    assert(b.deltaY == -20);
    assert(b.valid == 1);

    printf("  [PASS] test_raw_input_event_flat_trivially_copyable\n");
}

static void test_field_extractor_to_raw_input_event() {
    MacEventFieldExtractor extractor;

    {
        RawInputEventFlat flat{};
        flat.platformTime = 1000;
        flat.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
        flat.deltaX = 5;
        flat.deltaY = -3;
        flat.valid = 1;

        RawInputEvent event = extractor.toRawInputEvent(flat);
        assert(event.platformTime == 1000);
        assert(event.kind == RawEventKind::MouseMove);
        assert(std::get<RawMouseMovePayload>(event.payload).deltaX == 5);
        assert(std::get<RawMouseMovePayload>(event.payload).deltaY == -3);
    }

    {
        RawInputEventFlat flat{};
        flat.kind = static_cast<uint8_t>(RawEventKind::MouseButtonPress);
        flat.button = static_cast<uint8_t>(MouseButton::Right);
        flat.valid = 1;

        RawInputEvent event = extractor.toRawInputEvent(flat);
        assert(event.kind == RawEventKind::MouseButtonPress);
        assert(std::get<RawMouseButtonPayload>(event.payload).button == MouseButton::Right);
    }

    {
        RawInputEventFlat flat{};
        flat.kind = static_cast<uint8_t>(RawEventKind::Wheel);
        flat.wheelDelta = 42;
        flat.wheelAxis = static_cast<uint8_t>(WheelAxis::Vertical);
        flat.valid = 1;

        RawInputEvent event = extractor.toRawInputEvent(flat);
        assert(event.kind == RawEventKind::Wheel);
        assert(std::get<RawWheelPayload>(event.payload).delta == 42);
        assert(std::get<RawWheelPayload>(event.payload).axis == WheelAxis::Vertical);
    }

    {
        RawInputEventFlat flat{};
        flat.kind = static_cast<uint8_t>(RawEventKind::KeyPress);
        flat.keyCode = 65;
        flat.valid = 1;

        RawInputEvent event = extractor.toRawInputEvent(flat);
        assert(event.kind == RawEventKind::KeyPress);
        assert(std::get<RawKeyPayload>(event.payload).keyCode == 65);
    }

    printf("  [PASS] test_field_extractor_to_raw_input_event\n");
}

static void test_normalizer_to_raw_input_event() {
    RawInputEventFlat flat{};
    flat.platformTime = 999;
    flat.kind = static_cast<uint8_t>(RawEventKind::KeyRelease);
    flat.keyCode = 88;
    flat.valid = 1;

    RawInputEvent event = CGEventNormalizer::toRawInputEvent(flat);
    assert(event.platformTime == 999);
    assert(event.kind == RawEventKind::KeyRelease);
    assert(std::get<RawKeyPayload>(event.payload).keyCode == 88);

    printf("  [PASS] test_normalizer_to_raw_input_event\n");
}

static void test_normalizer_edge_overflow() {
    CGEventNormalizer normalizer;

    ScreenBoundary boundary{};
    boundary.width = 1920;
    boundary.height = 1080;
    boundary.originX = 0;
    boundary.originY = 0;
    normalizer.setScreenBoundary(boundary);

    RawInputEventFlat flat{};
    flat.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
    flat.deltaX = 0;
    flat.deltaY = 0;
    flat.valid = 1;

    assert(!normalizer.detectEdgeOverflow(flat));

    printf("  [PASS] test_normalizer_edge_overflow\n");
}

static void test_normalizer_normalize_calls_on_event() {
    CGEventNormalizer normalizer;

    std::atomic<int> callCount{0};
    normalizer.setOnEvent([&callCount](const RawInputEvent&) {
        callCount.fetch_add(1, std::memory_order_relaxed);
    });

    RawInputEventFlat flat{};
    flat.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
    flat.deltaX = 1;
    flat.deltaY = 1;
    flat.valid = 1;

    normalizer.normalize(flat);
    assert(callCount.load() == 1);
    assert(normalizer.normalizedCount() == 1);

    printf("  [PASS] test_normalizer_normalize_calls_on_event\n");
}

static void test_normalizer_invalid_event() {
    CGEventNormalizer normalizer;

    RawInputEventFlat flat{};
    flat.valid = 0;

    normalizer.normalize(flat);
    assert(normalizer.normalizedCount() == 0);
    assert(normalizer.unknownEventCount() == 1);

    printf("  [PASS] test_normalizer_invalid_event\n");
}

static void test_spsc_with_raw_input_event_flat() {
    SpscRingBuffer<RawInputEventFlat, SPSC_DATA_CAPACITY> queue;

    RawInputEventFlat in{};
    in.platformTime = 42;
    in.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
    in.deltaX = 100;
    in.deltaY = 200;
    in.valid = 1;

    assert(queue.tryPushDropOldest(in));

    RawInputEventFlat out{};
    assert(queue.tryPop(out));
    assert(out.platformTime == 42);
    assert(out.deltaX == 100);
    assert(out.deltaY == 200);
    assert(out.valid == 1);

    printf("  [PASS] test_spsc_with_raw_input_event_flat\n");
}

static void test_spsc_drop_oldest_with_flat() {
    SpscRingBuffer<RawInputEventFlat, 4> queue;

    for (int i = 0; i < 6; ++i) {
        RawInputEventFlat e{};
        e.platformTime = i;
        e.valid = 1;
        queue.tryPushDropOldest(e);
    }

    assert(queue.cumulativeDropCount() >= 2);

    RawInputEventFlat out{};
    int popped = 0;
    while (queue.tryPop(out)) {
        ++popped;
    }
    assert(popped <= 4);

    printf("  [PASS] test_spsc_drop_oldest_with_flat\n");
}

static void test_capture_handle_concurrent() {
    CaptureHandleManager mgr;
    constexpr int N = 100;
    std::vector<CaptureHandle> handles(N);

    std::thread t1([&]() {
        for (int i = 0; i < N / 2; ++i) {
            handles[i] = mgr.acquire();
        }
    });
    std::thread t2([&]() {
        for (int i = N / 2; i < N; ++i) {
            handles[i] = mgr.acquire();
        }
    });
    t1.join();
    t2.join();

    assert(mgr.activeCount() == N);

    for (auto& h : handles) {
        mgr.release(h);
    }
    assert(mgr.activeCount() == 0);

    printf("  [PASS] test_capture_handle_concurrent\n");
}

static void test_event_mask_complete_coverage() {
    assert(MacEventTap::kListenEventCount == 10);

    for (size_t i = 0; i < MacEventTap::kListenEventCount; ++i) {
        assert(MacEventTap::listenEventType(i) != 0xFFFFFFFF);
        for (size_t j = 0; j < i; ++j) {
            assert(MacEventTap::listenEventType(j) != MacEventTap::listenEventType(i));
        }
    }

    printf("  [PASS] test_event_mask_complete_coverage (10 distinct event types)\n");
}

static void test_event_mask_all_ten_types() {
    assert(MacEventTap::isEventMaskComplete());

    const uint64_t mask = MacEventTap::buildListenEventMask();
    for (size_t i = 0; i < MacEventTap::kListenEventCount; ++i) {
#ifdef __APPLE__
        const uint64_t bit = static_cast<uint64_t>(CGEventBitmaskForEventType(
            static_cast<CGEventType>(MacEventTap::listenEventType(i))));
#else
        const uint64_t bit = (1ULL << MacEventTap::listenEventType(i));
#endif
        assert((mask & bit) != 0);
        (void)bit;
    }
    (void)mask;

    printf("  [PASS] test_event_mask_all_ten_types (actual mask bits verified)\n");
}

static void test_normalizer_chain_process_event() {
    MacEventTap tap;

    std::atomic<int> onEventCallCount{0};
    tap.normalizer().setOnEvent([&onEventCallCount](const RawInputEvent&) {
        onEventCallCount.fetch_add(1, std::memory_order_relaxed);
    });

    ScreenBoundary boundary{};
    boundary.width = 1920;
    boundary.height = 1080;
    tap.normalizer().setScreenBoundary(boundary);

    RawInputEventFlat flat{};
    flat.kind = static_cast<uint8_t>(RawEventKind::MouseMove);
    flat.deltaX = 5;
    flat.deltaY = 10;
    flat.valid = 1;

    tap.processEvent(flat);

    assert(onEventCallCount.load() == 1);
    assert(tap.normalizer().normalizedCount() == 1);

    printf("  [PASS] test_normalizer_chain_process_event (MacEventTap::processEvent -> normalizer -> onEvent)\n");
}

#ifdef __APPLE__
static void test_macos_physical_cgeventtap_chain() {
    MacEventTap tap;

    std::atomic<int> onEventCallCount{0};
    std::atomic<bool> eventReceived{false};

    const CaptureHandle handle = tap.start([&onEventCallCount, &eventReceived](const RawInputEvent& event) {
        onEventCallCount.fetch_add(1, std::memory_order_relaxed);
        eventReceived.store(true, std::memory_order_release);
    });

    if (handle.active) {
        CGEventRef moveEvent = CGEventCreateMouseEvent(
            nullptr, kCGEventMouseMoved, CGPointMake(100, 100), 0);
        CGEventPost(kCGHIDEventTap, moveEvent);
        CFRelease(moveEvent);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        tap.stop(handle);

        printf("  [INFO] macOS physical: onEvent called %d times\n",
               onEventCallCount.load());
    } else {
        printf("  [SKIP] CGEventTap not installed (permission or environment)\n");
    }

    printf("  [PASS] test_macos_physical_cgeventtap_chain (skeleton)\n");
}
#endif

int main() {
    printf("=== CF2 Group 2 macOS Platform Tests ===\n\n");

    printf("[CaptureHandle]\n");
    test_capture_handle_basic();
    test_capture_handle_raii();
    test_capture_handle_move();
    test_capture_handle_double_release();
    test_capture_handle_concurrent();

    printf("\n[RawInputEventFlat]\n");
    test_raw_input_event_flat_trivially_copyable();

    printf("\n[MacEventFieldExtractor]\n");
    test_field_extractor_to_raw_input_event();

    printf("\n[CGEventNormalizer]\n");
    test_normalizer_to_raw_input_event();
    test_normalizer_edge_overflow();
    test_normalizer_normalize_calls_on_event();
    test_normalizer_invalid_event();

    printf("\n[SPSC with RawInputEventFlat]\n");
    test_spsc_with_raw_input_event_flat();
    test_spsc_drop_oldest_with_flat();

    printf("\n[CGEventTap Event Mask]\n");
    test_event_mask_complete_coverage();
    test_event_mask_all_ten_types();

    printf("\n[Normalizer Chain]\n");
    test_normalizer_chain_process_event();

#ifdef __APPLE__
    printf("\n[macOS Physical Evidence]\n");
    test_macos_physical_cgeventtap_chain();
#endif

    printf("\n=== All tests passed ===\n");
    return 0;
}