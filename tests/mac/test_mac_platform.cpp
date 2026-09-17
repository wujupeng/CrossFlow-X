#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "common/spsc_ring_buffer.hpp"
#include "mac/capture_handle.hpp"
#include "mac/mac_event_field_extractor.hpp"
#include "mac/cg_event_normalizer.hpp"
#include "mac/mac_event_tap.hpp"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>
#endif

#include <cassert>
#include <cstdio>
#include <cstdlib>

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

static void test_capture_handle_basic() {
    CaptureHandleManager mgr;
    CaptureHandle h1 = mgr.acquire();
    CFX_TEST_CHECK(h1.id > 0);
    CFX_TEST_CHECK(h1.active);
    CFX_TEST_CHECK(mgr.activeCount() == 1);

    CaptureHandle h2 = mgr.acquire();
    CFX_TEST_CHECK(h2.id > h1.id);
    CFX_TEST_CHECK(mgr.activeCount() == 2);

    mgr.release(h1);
    CFX_TEST_CHECK(!h1.active);
    CFX_TEST_CHECK(mgr.activeCount() == 1);

    mgr.release(h2);
    CFX_TEST_CHECK(!h2.active);
    CFX_TEST_CHECK(mgr.activeCount() == 0);

    printf("  [PASS] test_capture_handle_basic\n");
}

static void test_capture_handle_raii() {
    CaptureHandleManager mgr;

    {
        ScopedCaptureHandle scoped(mgr);
        CFX_TEST_CHECK(scoped.isValid());
        CFX_TEST_CHECK(mgr.activeCount() == 1);
    }

    CFX_TEST_CHECK(mgr.activeCount() == 0);

    printf("  [PASS] test_capture_handle_raii\n");
}

static void test_capture_handle_move() {
    CaptureHandleManager mgr;

    ScopedCaptureHandle scoped1(mgr);
    CFX_TEST_CHECK(scoped1.isValid());
    CFX_TEST_CHECK(mgr.activeCount() == 1);

    ScopedCaptureHandle scoped2 = std::move(scoped1);
    CFX_TEST_CHECK(!scoped1.isValid());
    CFX_TEST_CHECK(scoped2.isValid());
    CFX_TEST_CHECK(mgr.activeCount() == 1);

    printf("  [PASS] test_capture_handle_move\n");
}

static void test_capture_handle_double_release() {
    CaptureHandleManager mgr;
    CaptureHandle h = mgr.acquire();
    mgr.release(h);
    mgr.release(h);
    CFX_TEST_CHECK(mgr.activeCount() == 0);

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
    CFX_TEST_CHECK(b.platformTime == 12345);
    CFX_TEST_CHECK(b.deltaX == 10);
    CFX_TEST_CHECK(b.deltaY == -20);
    CFX_TEST_CHECK(b.valid == 1);

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
        CFX_TEST_CHECK(event.platformTime == 1000);
        CFX_TEST_CHECK(event.kind == RawEventKind::MouseMove);
        CFX_TEST_CHECK(std::get<RawMouseMovePayload>(event.payload).deltaX == 5);
        CFX_TEST_CHECK(std::get<RawMouseMovePayload>(event.payload).deltaY == -3);
    }

    {
        RawInputEventFlat flat{};
        flat.kind = static_cast<uint8_t>(RawEventKind::MouseButtonPress);
        flat.button = static_cast<uint8_t>(MouseButton::Right);
        flat.valid = 1;

        RawInputEvent event = extractor.toRawInputEvent(flat);
        CFX_TEST_CHECK(event.kind == RawEventKind::MouseButtonPress);
        CFX_TEST_CHECK(std::get<RawMouseButtonPayload>(event.payload).button == MouseButton::Right);
    }

    {
        RawInputEventFlat flat{};
        flat.kind = static_cast<uint8_t>(RawEventKind::Wheel);
        flat.wheelDelta = 42;
        flat.wheelAxis = static_cast<uint8_t>(WheelAxis::Vertical);
        flat.valid = 1;

        RawInputEvent event = extractor.toRawInputEvent(flat);
        CFX_TEST_CHECK(event.kind == RawEventKind::Wheel);
        CFX_TEST_CHECK(std::get<RawWheelPayload>(event.payload).delta == 42);
        CFX_TEST_CHECK(std::get<RawWheelPayload>(event.payload).axis == WheelAxis::Vertical);
    }

    {
        RawInputEventFlat flat{};
        flat.kind = static_cast<uint8_t>(RawEventKind::KeyPress);
        flat.keyCode = 65;
        flat.valid = 1;

        RawInputEvent event = extractor.toRawInputEvent(flat);
        CFX_TEST_CHECK(event.kind == RawEventKind::KeyPress);
        CFX_TEST_CHECK(std::get<RawKeyPayload>(event.payload).keyCode == 65);
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
    CFX_TEST_CHECK(event.platformTime == 999);
    CFX_TEST_CHECK(event.kind == RawEventKind::KeyRelease);
    CFX_TEST_CHECK(std::get<RawKeyPayload>(event.payload).keyCode == 88);


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

    CFX_TEST_CHECK(!normalizer.detectEdgeOverflow(flat));

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
    CFX_TEST_CHECK(callCount.load() == 1);
    CFX_TEST_CHECK(normalizer.normalizedCount() == 1);

    printf("  [PASS] test_normalizer_normalize_calls_on_event\n");
}

static void test_normalizer_invalid_event() {
    CGEventNormalizer normalizer;

    RawInputEventFlat flat{};
    flat.valid = 0;

    normalizer.normalize(flat);
    CFX_TEST_CHECK(normalizer.normalizedCount() == 0);
    CFX_TEST_CHECK(normalizer.unknownEventCount() == 1);

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

    CFX_TEST_CHECK(queue.tryPushDropOldest(in));

    RawInputEventFlat out{};
    CFX_TEST_CHECK(queue.tryPop(out));
    CFX_TEST_CHECK(out.platformTime == 42);
    CFX_TEST_CHECK(out.deltaX == 100);
    CFX_TEST_CHECK(out.deltaY == 200);
    CFX_TEST_CHECK(out.valid == 1);


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

    CFX_TEST_CHECK(queue.cumulativeDropCount() >= 2);

    RawInputEventFlat out{};
    int popped = 0;
    while (queue.tryPop(out)) {
        ++popped;
    }
    CFX_TEST_CHECK(popped <= 4);


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

    CFX_TEST_CHECK(mgr.activeCount() == N);

    for (auto& h : handles) {
        mgr.release(h);
    }
    CFX_TEST_CHECK(mgr.activeCount() == 0);

    printf("  [PASS] test_capture_handle_concurrent\n");
}

static void test_event_mask_complete_coverage() {
    CFX_TEST_CHECK(MacEventTap::kListenEventCount == 10);

    for (size_t i = 0; i < MacEventTap::kListenEventCount; ++i) {
        CFX_TEST_CHECK(MacEventTap::listenEventType(i) != 0xFFFFFFFF);
        for (size_t j = 0; j < i; ++j) {
            CFX_TEST_CHECK(MacEventTap::listenEventType(j) != MacEventTap::listenEventType(i));
        }
    }

    printf("  [PASS] test_event_mask_complete_coverage (10 distinct event types)\n");
}

static void test_event_mask_all_ten_types() {
    CFX_TEST_CHECK(MacEventTap::isEventMaskComplete());

    const uint64_t mask = MacEventTap::buildListenEventMask();
    for (size_t i = 0; i < MacEventTap::kListenEventCount; ++i) {
        const uint64_t bit = (1ULL << MacEventTap::listenEventType(i));
        CFX_TEST_CHECK((mask & bit) != 0);
    }

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

    CFX_TEST_CHECK(onEventCallCount.load() == 1);
    CFX_TEST_CHECK(tap.normalizer().normalizedCount() == 1);

    printf("  [PASS] test_normalizer_chain_process_event (MacEventTap::processEvent -> normalizer -> onEvent)\n");
}

#ifdef __APPLE__
static int test_macos_physical_cgeventtap_chain() {
    MacEventTap tap;

    printf("  [DIAG] === Permission Diagnostics ===\n");
    bool axTrusted = AXIsProcessTrustedWithOptions(NULL);
    printf("  [DIAG] AXIsProcessTrustedWithOptions(NULL): %s\n", axTrusted ? "YES" : "NO");

    bool inputMon = CGPreflightListenEventAccess();
    printf("  [DIAG] CGPreflightListenEventAccess(): %s\n", inputMon ? "YES" : "NO");

    A11yPermissionStatus permStatus = tap.permissionGuard().check();
    const char* permStr = "Unknown";
    switch (permStatus) {
        case A11yPermissionStatus::Granted:               permStr = "Granted"; break;
        case A11yPermissionStatus::BothDenied:            permStr = "BothDenied"; break;
        case A11yPermissionStatus::AccessibilityDenied:   permStr = "AccessibilityDenied"; break;
        case A11yPermissionStatus::InputMonitoringDenied: permStr = "InputMonitoringDenied"; break;
    }
    printf("  [DIAG] A11yPermissionGuard::check(): %s\n", permStr);
    printf("  [DIAG] checkAccessibility(): %s\n", tap.permissionGuard().checkAccessibility() ? "YES" : "NO");
    printf("  [DIAG] checkInputMonitoring(): %s\n", tap.permissionGuard().checkInputMonitoring() ? "YES" : "NO");
    printf("  [DIAG] === End Permission Diagnostics ===\n");

    std::atomic<int> onEventCallCount{0};
    std::atomic<uint32_t> receivedKinds{0};

    const CaptureHandle handle = tap.start([&onEventCallCount, &receivedKinds](const RawInputEvent& event) {
        onEventCallCount.fetch_add(1, std::memory_order_relaxed);
        receivedKinds.fetch_or(1u << static_cast<uint32_t>(event.kind),
                               std::memory_order_relaxed);
    });

    if (!handle.active) {
        printf("  [FAIL] CGEventTap installation failed — cannot provide physical evidence\n");
        printf("  [DIAG] tap.state() = %d (0=Inactive, 1=Active, 2=Degraded)\n", (int)tap.state());
        printf("  [DIAG] tap.isDegraded() = %s\n", tap.isDegraded() ? "YES" : "NO");
        if (!axTrusted) {
            printf("  [DIAG] ROOT CAUSE: Accessibility permission NOT granted.\n");
            printf("  [DIAG] FIX: System Settings > Privacy & Security > Accessibility > add Terminal/iTerm\n");
        }
        if (!inputMon) {
            printf("  [DIAG] ROOT CAUSE: Input Monitoring permission NOT granted.\n");
            printf("  [DIAG] FIX: System Settings > Privacy & Security > Input Monitoring > add Terminal/iTerm\n");
        }
        return 1;
    }

    CGEventRef moveEvent = CGEventCreateMouseEvent(
        nullptr, kCGEventMouseMoved, CGPointMake(100, 100), kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, moveEvent);
    CFRelease(moveEvent);

    CGEventRef leftDown = CGEventCreateMouseEvent(
        nullptr, kCGEventLeftMouseDown, CGPointMake(100, 100), kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, leftDown);
    CFRelease(leftDown);

    CGEventRef leftUp = CGEventCreateMouseEvent(
        nullptr, kCGEventLeftMouseUp, CGPointMake(100, 100), kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, leftUp);
    CFRelease(leftUp);

    CGEventRef scroll = CGEventCreateScrollWheelEvent(
        nullptr, kCGScrollEventUnitLine, 1, 1);
    CGEventPost(kCGHIDEventTap, scroll);
    CFRelease(scroll);

    CGEventRef keyDown = CGEventCreateKeyboardEvent(nullptr, 0, true);
    CGEventPost(kCGHIDEventTap, keyDown);
    CFRelease(keyDown);

    CGEventRef keyUp = CGEventCreateKeyboardEvent(nullptr, 0, false);
    CGEventPost(kCGHIDEventTap, keyUp);
    CFRelease(keyUp);

    for (int i = 0; i < 50 && onEventCallCount.load() < 6; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    tap.stop(handle);

    const int count = onEventCallCount.load();
    const uint32_t kinds = receivedKinds.load();

    printf("  [INFO] macOS physical: onEvent called %d times, kinds=0x%08x\n",
           count, kinds);

    CFX_TEST_CHECK(count > 0);

    printf("  [PASS] test_macos_physical_cgeventtap_chain (%d events received)\n", count);
    return 0;
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
    if (test_macos_physical_cgeventtap_chain() != 0) {
        printf("\n=== FAILED: macOS physical evidence test ===\n");
        return 1;
    }
#endif

    printf("\n=== All tests passed ===\n");
    return 0;
}