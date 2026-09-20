#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "mac/mac_event_injector.hpp"
#include "mac/release_all_pressed_executor.hpp"

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

static NodeId makeNodeId(u64 high, u64 low) {
    NodeId id{};
    id.high = high;
    id.low = low;
    return id;
}

static CanonicalInputEvent makeMouseMoveEvent(NodeId src, u64 eid = 1) {
    CanonicalInputEvent e{};
    e.eventId = eid;
    e.sourceNodeId = src;
    e.timestamp = 1000;
    e.eventType = EventType::MouseMove;
    e.payload = MouseMovePayload{10, 20};
    e.modifierState = {false, false, false, false, false};
    return e;
}

static CanonicalInputEvent makeMouseButtonEvent(NodeId src, EventType type, MouseButton btn, u64 eid = 1) {
    CanonicalInputEvent e{};
    e.eventId = eid;
    e.sourceNodeId = src;
    e.timestamp = 1000;
    e.eventType = type;
    e.payload = MouseButtonPayload{btn};
    e.modifierState = {false, false, false, false, false};
    return e;
}

static CanonicalInputEvent makeWheelEvent(NodeId src, u64 eid = 1) {
    CanonicalInputEvent e{};
    e.eventId = eid;
    e.sourceNodeId = src;
    e.timestamp = 1000;
    e.eventType = EventType::Wheel;
    e.payload = WheelPayload{5, WheelAxis::Vertical};
    e.modifierState = {false, false, false, false, false};
    return e;
}

static CanonicalInputEvent makeKeyEvent(NodeId src, EventType type, KeyCode code, u64 eid = 1) {
    CanonicalInputEvent e{};
    e.eventId = eid;
    e.sourceNodeId = src;
    e.timestamp = 1000;
    e.eventType = type;
    e.payload = KeyPayload{code};
    e.modifierState = {false, false, false, false, false};
    return e;
}

static void test_injector_source_validation() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);

    CFX_TEST_CHECK(injector.sourceNodeId() == src);
    CFX_TEST_CHECK(!injector.isControllerMode());

    auto event = makeMouseMoveEvent(src);
    auto result = injector.inject(event);
    CFX_TEST_CHECK(result.ok);
    CFX_TEST_CHECK(injector.totalInjected() == 1);
    CFX_TEST_CHECK(injector.totalRejected() == 0);

    NodeId wrongSrc = makeNodeId(2, 200);
    auto event2 = makeMouseMoveEvent(wrongSrc);
    auto result2 = injector.inject(event2);
    CFX_TEST_CHECK(!result2.ok);
    CFX_TEST_CHECK(injector.totalInjected() == 1);
    CFX_TEST_CHECK(injector.totalRejected() == 1);

    printf("  [PASS] test_injector_source_validation\n");
}

static void test_injector_controller_mode_rejects_all() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);
    injector.setControllerMode(true);

    CFX_TEST_CHECK(injector.isControllerMode());

    auto event = makeMouseMoveEvent(src);
    auto result = injector.inject(event);
    CFX_TEST_CHECK(!result.ok);
    CFX_TEST_CHECK(injector.totalInjected() == 0);
    CFX_TEST_CHECK(injector.totalRejected() == 1);

    printf("  [PASS] test_injector_controller_mode_rejects_all\n");
}

static void test_injector_param_validation() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);

    auto event = makeMouseMoveEvent(src);
    event.eventId = 0;
    CFX_TEST_CHECK(!injector.inject(event).ok);

    event = makeMouseMoveEvent(src);
    event.timestamp = 0;
    CFX_TEST_CHECK(!injector.inject(event).ok);

    event = makeMouseMoveEvent(src);
    event.sourceNodeId = NodeId{};
    CFX_TEST_CHECK(!injector.inject(event).ok);

    event = makeMouseMoveEvent(src);
    event.eventType = EventType::KeyPress;
    event.payload = MouseMovePayload{1, 1};
    CFX_TEST_CHECK(!injector.inject(event).ok);

    CFX_TEST_CHECK(injector.totalInjected() == 0);
    CFX_TEST_CHECK(injector.totalRejected() == 4);

    printf("  [PASS] test_injector_param_validation\n");
}

static void test_injector_keycode_range_validation() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);

    CFX_TEST_CHECK(injector.inject(makeKeyEvent(src, EventType::KeyPress, 0)).ok);
    CFX_TEST_CHECK(injector.inject(makeKeyEvent(src, EventType::KeyPress, 255)).ok);

    auto event = makeKeyEvent(src, EventType::KeyPress, 256);
    CFX_TEST_CHECK(!injector.inject(event).ok);

    event = makeKeyEvent(src, EventType::KeyRelease, 999);
    CFX_TEST_CHECK(!injector.inject(event).ok);

    CFX_TEST_CHECK(injector.totalInjected() == 2);
    CFX_TEST_CHECK(injector.totalRejected() == 2);

    printf("  [PASS] test_injector_keycode_range_validation\n");
}

static void test_injector_screen_boundary_validation() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);

    ScreenBoundary sb{};
    sb.width = 1920;
    sb.height = 1080;
    sb.originX = 0;
    sb.originY = 0;
    injector.setScreenBoundary(sb);
    injector.setInjectionMethod(MacEventInjector::InjectionMethod::AbsolutePosition);

    CFX_TEST_CHECK(injector.screenBoundary().isValid());

    auto event = makeMouseMoveEvent(src);
    event.payload = MouseMovePayload{100, 200};
    CFX_TEST_CHECK(injector.inject(event).ok);

    event.payload = MouseMovePayload{1920, 1080};
    CFX_TEST_CHECK(injector.inject(event).ok);

    event.payload = MouseMovePayload{-1, 100};
    CFX_TEST_CHECK(!injector.inject(event).ok);

    event.payload = MouseMovePayload{100, -1};
    CFX_TEST_CHECK(!injector.inject(event).ok);

    event.payload = MouseMovePayload{2000, 100};
    CFX_TEST_CHECK(!injector.inject(event).ok);

    event.payload = MouseMovePayload{100, 2000};
    CFX_TEST_CHECK(!injector.inject(event).ok);

    CFX_TEST_CHECK(injector.totalInjected() == 2);
    CFX_TEST_CHECK(injector.totalRejected() == 4);

    printf("  [PASS] test_injector_screen_boundary_validation\n");
}

static void test_injector_injection_method() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);

    CFX_TEST_CHECK(injector.injectionMethod() == MacEventInjector::InjectionMethod::RelativeDelta);

    injector.setInjectionMethod(MacEventInjector::InjectionMethod::AbsolutePosition);
    CFX_TEST_CHECK(injector.injectionMethod() == MacEventInjector::InjectionMethod::AbsolutePosition);

    auto event = makeMouseMoveEvent(src);
    auto result = injector.inject(event);
    CFX_TEST_CHECK(result.ok);

    injector.setInjectionMethod(MacEventInjector::InjectionMethod::RelativeDelta);
    result = injector.inject(event);
    CFX_TEST_CHECK(result.ok);

    CFX_TEST_CHECK(injector.totalInjected() == 2);

    printf("  [PASS] test_injector_injection_method\n");
}

static void test_injector_all_event_types() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);

    CFX_TEST_CHECK(injector.inject(makeMouseMoveEvent(src)).ok);
    CFX_TEST_CHECK(injector.inject(makeMouseButtonEvent(src, EventType::MouseButtonPress, MouseButton::Left)).ok);
    CFX_TEST_CHECK(injector.inject(makeMouseButtonEvent(src, EventType::MouseButtonRelease, MouseButton::Left)).ok);
    CFX_TEST_CHECK(injector.inject(makeMouseButtonEvent(src, EventType::MouseButtonPress, MouseButton::Right)).ok);
    CFX_TEST_CHECK(injector.inject(makeWheelEvent(src)).ok);
    CFX_TEST_CHECK(injector.inject(makeKeyEvent(src, EventType::KeyPress, 42)).ok);
    CFX_TEST_CHECK(injector.inject(makeKeyEvent(src, EventType::KeyRelease, 42)).ok);

    CFX_TEST_CHECK(injector.totalInjected() == 7);
    CFX_TEST_CHECK(injector.totalRejected() == 0);

    printf("  [PASS] test_injector_all_event_types\n");
}

static void test_injector_all_button_types() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);

    CFX_TEST_CHECK(injector.inject(makeMouseButtonEvent(src, EventType::MouseButtonPress, MouseButton::Left)).ok);
    CFX_TEST_CHECK(injector.inject(makeMouseButtonEvent(src, EventType::MouseButtonRelease, MouseButton::Left)).ok);
    CFX_TEST_CHECK(injector.inject(makeMouseButtonEvent(src, EventType::MouseButtonPress, MouseButton::Right)).ok);
    CFX_TEST_CHECK(injector.inject(makeMouseButtonEvent(src, EventType::MouseButtonRelease, MouseButton::Right)).ok);
    CFX_TEST_CHECK(injector.inject(makeMouseButtonEvent(src, EventType::MouseButtonPress, MouseButton::Middle)).ok);
    CFX_TEST_CHECK(injector.inject(makeMouseButtonEvent(src, EventType::MouseButtonRelease, MouseButton::Middle)).ok);

    CFX_TEST_CHECK(injector.totalInjected() == 6);
    CFX_TEST_CHECK(injector.totalRejected() == 0);

    printf("  [PASS] test_injector_all_button_types\n");
}

static void test_injector_injection_method_location_compute() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);

    injector.setInjectionMethod(MacEventInjector::InjectionMethod::LocationCompute);
    CFX_TEST_CHECK(injector.injectionMethod() == MacEventInjector::InjectionMethod::LocationCompute);

    auto event = makeMouseMoveEvent(src);
    auto result = injector.inject(event);
    CFX_TEST_CHECK(result.ok);
    CFX_TEST_CHECK(injector.totalInjected() == 1);

    printf("  [PASS] test_injector_injection_method_location_compute\n");
}

static void test_injector_batch() {
    NodeId src = makeNodeId(1, 100);
    MacEventInjector injector(src);

    std::vector<CanonicalInputEvent> events;
    for (u64 i = 1; i <= 5; ++i) {
        events.push_back(makeMouseMoveEvent(src, i));
    }
    auto result = injector.injectBatch(events);
    CFX_TEST_CHECK(result.ok);
    CFX_TEST_CHECK(result.failedCount == 0);
    CFX_TEST_CHECK(injector.totalInjected() == 5);

    events.clear();
    events.push_back(makeMouseMoveEvent(src, 1));
    events.push_back(makeMouseMoveEvent(makeNodeId(9, 9), 2));
    events.push_back(makeMouseMoveEvent(src, 3));
    result = injector.injectBatch(events);
    CFX_TEST_CHECK(!result.ok);
    CFX_TEST_CHECK(result.failedCount == 1);
    CFX_TEST_CHECK(injector.totalInjected() == 7);
    CFX_TEST_CHECK(injector.totalRejected() == 1);

    printf("  [PASS] test_injector_batch\n");
}

static void test_injector_release_all_pressed_empty() {
    MacEventInjector injector(makeNodeId(1, 100));
    PressedStateSnapshot snapshot{};

    auto result = injector.releaseAllPressed(snapshot);
    CFX_TEST_CHECK(result.releasedCount == 0);

    printf("  [PASS] test_injector_release_all_pressed_empty\n");
}

static void test_injector_release_all_pressed_buttons() {
    MacEventInjector injector(makeNodeId(1, 100));
    PressedStateSnapshot snapshot{};

    snapshot.pressedMouseButtons.setPressed(MouseButton::Left);
    snapshot.pressedMouseButtons.setPressed(MouseButton::Right);
    snapshot.pressedMouseButtons.setPressed(MouseButton::Middle);

    auto result = injector.releaseAllPressed(snapshot);
    CFX_TEST_CHECK(result.releasedCount == 3);

    printf("  [PASS] test_injector_release_all_pressed_buttons\n");
}

static void test_injector_release_all_pressed_keys() {
    MacEventInjector injector(makeNodeId(1, 100));
    PressedStateSnapshot snapshot{};

    snapshot.pressedKeys.setPressed(42);
    snapshot.pressedKeys.setPressed(100);
    snapshot.pressedKeys.setPressed(255);

    auto result = injector.releaseAllPressed(snapshot);
    CFX_TEST_CHECK(result.releasedCount == 3);

    printf("  [PASS] test_injector_release_all_pressed_keys\n");
}

static void test_injector_release_all_pressed_mixed() {
    MacEventInjector injector(makeNodeId(1, 100));
    PressedStateSnapshot snapshot{};

    snapshot.pressedMouseButtons.setPressed(MouseButton::Left);
    snapshot.pressedMouseButtons.setPressed(MouseButton::Right);
    snapshot.pressedKeys.setPressed(10);
    snapshot.pressedKeys.setPressed(20);
    snapshot.pressedKeys.setPressed(30);

    auto result = injector.releaseAllPressed(snapshot);
    CFX_TEST_CHECK(result.releasedCount == 5);

    printf("  [PASS] test_injector_release_all_pressed_mixed\n");
}

static void test_injector_sync_modifiers() {
    MacEventInjector injector(makeNodeId(1, 100));

    ModifierState none{false, false, false, false, false};
    CFX_TEST_CHECK(injector.syncModifiers(none));
    CFX_TEST_CHECK(injector.localModifierState() == none);

    ModifierState all{true, true, true, true, true};
    CFX_TEST_CHECK(injector.syncModifiers(all));
    CFX_TEST_CHECK(injector.localModifierState() == all);

    ModifierState partial{true, false, true, false, true};
    CFX_TEST_CHECK(injector.syncModifiers(partial));
    CFX_TEST_CHECK(injector.localModifierState() == partial);

    printf("  [PASS] test_injector_sync_modifiers\n");
}

static void test_injector_sync_modifiers_explicit_alignment() {
    MacEventInjector injector(makeNodeId(1, 100));

    CFX_TEST_CHECK(!injector.localModifierState().shift);

    ModifierState sourceShift{true, false, false, false, false};
    CFX_TEST_CHECK(injector.syncModifiers(sourceShift));
    CFX_TEST_CHECK(injector.localModifierState().shift);
    CFX_TEST_CHECK(injector.localModifierState() == sourceShift);

    ModifierState releaseShift{false, false, false, false, false};
    CFX_TEST_CHECK(injector.syncModifiers(releaseShift));
    CFX_TEST_CHECK(!injector.localModifierState().shift);
    CFX_TEST_CHECK(injector.localModifierState() == releaseShift);

    printf("  [PASS] test_injector_sync_modifiers_explicit_alignment\n");
}

static void test_injector_sync_modifiers_partial_alignment() {
    MacEventInjector injector(makeNodeId(1, 100));

    ModifierState initial{true, true, false, false, false};
    CFX_TEST_CHECK(injector.syncModifiers(initial));
    CFX_TEST_CHECK(injector.localModifierState() == initial);

    ModifierState target{false, true, true, false, true};
    CFX_TEST_CHECK(injector.syncModifiers(target));
    CFX_TEST_CHECK(injector.localModifierState() == target);
    CFX_TEST_CHECK(!injector.localModifierState().shift);
    CFX_TEST_CHECK(injector.localModifierState().ctrl);
    CFX_TEST_CHECK(injector.localModifierState().alt);
    CFX_TEST_CHECK(!injector.localModifierState().cmd);
    CFX_TEST_CHECK(injector.localModifierState().fn);

    printf("  [PASS] test_injector_sync_modifiers_partial_alignment\n");
}

static void test_injector_sync_modifiers_idempotent() {
    MacEventInjector injector(makeNodeId(1, 100));

    ModifierState state{true, false, true, false, false};
    CFX_TEST_CHECK(injector.syncModifiers(state));
    CFX_TEST_CHECK(injector.localModifierState() == state);

    CFX_TEST_CHECK(injector.syncModifiers(state));
    CFX_TEST_CHECK(injector.localModifierState() == state);

    printf("  [PASS] test_injector_sync_modifiers_idempotent\n");
}

static void test_injector_set_source_node_id() {
    MacEventInjector injector(makeNodeId(1, 100));

    NodeId newSrc = makeNodeId(5, 500);
    injector.setSourceNodeId(newSrc);
    CFX_TEST_CHECK(injector.sourceNodeId() == newSrc);

    auto event = makeMouseMoveEvent(newSrc);
    CFX_TEST_CHECK(injector.inject(event).ok);

    printf("  [PASS] test_injector_set_source_node_id\n");
}

static void test_executor_deadline_constant() {
    CFX_TEST_CHECK(ReleaseAllPressedExecutor::kTotalDeadlineMs == 100);

    printf("  [PASS] test_executor_deadline_constant (kTotalDeadlineMs=%u)\n",
           ReleaseAllPressedExecutor::kTotalDeadlineMs);
}

static void test_executor_empty_snapshot() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    auto injectFn = [](const CanonicalInputEvent&) -> InjectResult {
        return {true, 0, 0};
    };

    auto result = executor.execute(snapshot, injectFn);
    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::Success);
    CFX_TEST_CHECK(executor.releasedCount() == 0);
    CFX_TEST_CHECK(executor.totalElapsedMs() <= ReleaseAllPressedExecutor::kTotalDeadlineMs);

    printf("  [PASS] test_executor_empty_snapshot\n");
}

static void test_executor_all_released_success() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    snapshot.pressedMouseButtons.setPressed(MouseButton::Left);
    snapshot.pressedMouseButtons.setPressed(MouseButton::Right);
    snapshot.pressedKeys.setPressed(42);
    snapshot.pressedKeys.setPressed(100);

    uint32_t callCount = 0;
    auto injectFn = [&callCount](const CanonicalInputEvent& e) -> InjectResult {
        (void)e;
        ++callCount;
        return {true, 0, 0};
    };

    auto result = executor.execute(snapshot, injectFn);
    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::Success);
    CFX_TEST_CHECK(executor.releasedCount() == 4);
    CFX_TEST_CHECK(callCount == 4);
    CFX_TEST_CHECK(executor.totalElapsedMs() <= ReleaseAllPressedExecutor::kTotalDeadlineMs);

    printf("  [PASS] test_executor_all_released_success\n");
}

static void test_executor_partial_release() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    snapshot.pressedMouseButtons.setPressed(MouseButton::Left);
    snapshot.pressedMouseButtons.setPressed(MouseButton::Right);
    snapshot.pressedKeys.setPressed(42);

    uint32_t callCount = 0;
    auto injectFn = [&callCount](const CanonicalInputEvent& e) -> InjectResult {
        (void)e;
        ++callCount;
        if (callCount == 2) {
            return {false, 1, 0};
        }
        return {true, 0, 0};
    };

    auto result = executor.execute(snapshot, injectFn);
    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::PartialRelease);
    CFX_TEST_CHECK(executor.releasedCount() == 2);
    CFX_TEST_CHECK(callCount == 3);

    printf("  [PASS] test_executor_partial_release\n");
}

static void test_executor_deadline_reached() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    for (uint16_t i = 0; i < 200; ++i) {
        snapshot.pressedKeys.setPressed(i);
    }

    std::vector<std::chrono::high_resolution_clock::time_point> callTimestamps;
    auto injectFn = [&callTimestamps](const CanonicalInputEvent& e) -> InjectResult {
        (void)e;
        callTimestamps.push_back(std::chrono::high_resolution_clock::now());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return {true, 0, 0};
    };

    auto tStart = std::chrono::high_resolution_clock::now();
    auto result = executor.execute(snapshot, injectFn);
    auto tEnd = std::chrono::high_resolution_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();

    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::DeadlineReached);
    CFX_TEST_CHECK(executor.releasedCount() < 200);
    CFX_TEST_CHECK(executor.releasedCount() >= 3);

    auto tDeadline = tStart + std::chrono::milliseconds(ReleaseAllPressedExecutor::kTotalDeadlineMs);
    auto epsilon = std::chrono::milliseconds(5);
    for (size_t i = 0; i < callTimestamps.size(); ++i) {
        CFX_TEST_CHECK(callTimestamps[i] < tDeadline + epsilon);
    }

    CFX_TEST_CHECK(static_cast<uint64_t>(elapsedMs) <= ReleaseAllPressedExecutor::kTotalDeadlineMs + 20);

    printf("  [PASS] test_executor_deadline_reached (released=%u, elapsed=%lldms, calls=%zu)\n",
           executor.releasedCount(), (long long)elapsedMs, callTimestamps.size());
}

static void test_executor_deadline_immediate_stop() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    for (uint16_t i = 0; i < 10; ++i) {
        snapshot.pressedKeys.setPressed(i);
    }

    uint32_t callCount = 0;
    auto injectFn = [&callCount](const CanonicalInputEvent& e) -> InjectResult {
        (void)e;
        ++callCount;
        if (callCount <= 3) {
            return {true, 0, 0};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(101));
        return {true, 0, 0};
    };

    auto result = executor.execute(snapshot, injectFn);

    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::DeadlineReached);
    CFX_TEST_CHECK(callCount == 4);
    CFX_TEST_CHECK(executor.releasedCount() == 4);

    printf("  [PASS] test_executor_deadline_immediate_stop (calls=%u, released=%u)\n",
           callCount, executor.releasedCount());
}

static void test_executor_deadline_deterministic_clock() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    for (uint16_t i = 0; i < 15; ++i) {
        snapshot.pressedKeys.setPressed(i);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    auto mockTime = t0;
    auto clockFn = [&mockTime]() { return mockTime; };

    uint32_t callCount = 0;
    auto injectFn = [&mockTime, &callCount](const CanonicalInputEvent& e) -> InjectResult {
        (void)e;
        ++callCount;
        mockTime += std::chrono::milliseconds(10);
        return {true, 0, 0};
    };

    auto result = executor.execute(snapshot, injectFn, clockFn);

    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::DeadlineReached);
    CFX_TEST_CHECK(callCount == 10);
    CFX_TEST_CHECK(executor.releasedCount() == 10);
    CFX_TEST_CHECK(executor.totalElapsedMs() == 100);

    printf("  [PASS] test_executor_deadline_deterministic_clock (calls=%u, released=%u, elapsed=%ums)\n",
           callCount, executor.releasedCount(), executor.totalElapsedMs());
}

static void test_executor_deadline_deterministic_under_limit() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    for (uint16_t i = 0; i < 5; ++i) {
        snapshot.pressedKeys.setPressed(i);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    auto mockTime = t0;
    auto clockFn = [&mockTime]() { return mockTime; };

    uint32_t callCount = 0;
    auto injectFn = [&mockTime, &callCount](const CanonicalInputEvent& e) -> InjectResult {
        (void)e;
        ++callCount;
        mockTime += std::chrono::milliseconds(10);
        return {true, 0, 0};
    };

    auto result = executor.execute(snapshot, injectFn, clockFn);

    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::Success);
    CFX_TEST_CHECK(callCount == 5);
    CFX_TEST_CHECK(executor.releasedCount() == 5);
    CFX_TEST_CHECK(executor.totalElapsedMs() == 50);
    CFX_TEST_CHECK(executor.totalElapsedMs() <= ReleaseAllPressedExecutor::kTotalDeadlineMs);

    printf("  [PASS] test_executor_deadline_deterministic_under_limit (calls=%u, released=%u, elapsed=%ums)\n",
           callCount, executor.releasedCount(), executor.totalElapsedMs());
}

static void test_executor_deadline_deterministic_no_calls_after() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    for (uint16_t i = 0; i < 20; ++i) {
        snapshot.pressedKeys.setPressed(i);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    auto mockTime = t0;
    auto clockFn = [&mockTime]() { return mockTime; };

    uint32_t callCount = 0;
    bool calledAfterDeadline = false;
    auto injectFn = [&mockTime, &callCount, &calledAfterDeadline, t0](const CanonicalInputEvent& e) -> InjectResult {
        (void)e;
        if (mockTime >= t0 + std::chrono::milliseconds(ReleaseAllPressedExecutor::kTotalDeadlineMs)) {
            calledAfterDeadline = true;
        }
        ++callCount;
        mockTime += std::chrono::milliseconds(8);
        return {true, 0, 0};
    };

    auto result = executor.execute(snapshot, injectFn, clockFn);

    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::DeadlineReached);
    CFX_TEST_CHECK(!calledAfterDeadline);
    CFX_TEST_CHECK(callCount == 13);
    CFX_TEST_CHECK(executor.releasedCount() == 13);
    CFX_TEST_CHECK(executor.totalElapsedMs() == 104);

    printf("  [PASS] test_executor_deadline_deterministic_no_calls_after (calls=%u, calledAfterDeadline=%s)\n",
           callCount, calledAfterDeadline ? "true" : "false");
}

static void test_executor_degraded() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    snapshot.pressedMouseButtons.setPressed(MouseButton::Left);

    auto injectFn = [](const CanonicalInputEvent& e) -> InjectResult {
        (void)e;
        return {false, 1, 0};
    };

    auto result = executor.execute(snapshot, injectFn);
    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::Degraded);
    CFX_TEST_CHECK(executor.releasedCount() == 0);
    CFX_TEST_CHECK(executor.degraded());

    printf("  [PASS] test_executor_degraded\n");
}

static void test_executor_execution_count() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    auto injectFn = [](const CanonicalInputEvent&) -> InjectResult {
        return {true, 0, 0};
    };

    CFX_TEST_CHECK(executor.executionCount() == 0);
    executor.execute(snapshot, injectFn);
    CFX_TEST_CHECK(executor.executionCount() == 1);
    executor.execute(snapshot, injectFn);
    CFX_TEST_CHECK(executor.executionCount() == 2);

    printf("  [PASS] test_executor_execution_count\n");
}

static void test_executor_result_string() {
    CFX_TEST_CHECK(std::string(ReleaseAllPressedExecutor::resultString(
        ReleaseAllPressedExecutor::Result::Success)) == "Success");
    CFX_TEST_CHECK(std::string(ReleaseAllPressedExecutor::resultString(
        ReleaseAllPressedExecutor::Result::PartialRelease)) == "PartialRelease");
    CFX_TEST_CHECK(std::string(ReleaseAllPressedExecutor::resultString(
        ReleaseAllPressedExecutor::Result::DeadlineReached)) == "DeadlineReached");
    CFX_TEST_CHECK(std::string(ReleaseAllPressedExecutor::resultString(
        ReleaseAllPressedExecutor::Result::Degraded)) == "Degraded");

    printf("  [PASS] test_executor_result_string\n");
}

static void test_executor_bounded_within_100ms() {
    ReleaseAllPressedExecutor executor;
    PressedStateSnapshot snapshot{};

    snapshot.pressedMouseButtons.setPressed(MouseButton::Left);
    snapshot.pressedMouseButtons.setPressed(MouseButton::Right);
    snapshot.pressedMouseButtons.setPressed(MouseButton::Middle);
    for (uint16_t i = 0; i < 5; ++i) {
        snapshot.pressedKeys.setPressed(i);
    }

    auto injectFn = [](const CanonicalInputEvent& e) -> InjectResult {
        (void)e;
        return {true, 0, 0};
    };

    auto tStart = std::chrono::high_resolution_clock::now();
    auto result = executor.execute(snapshot, injectFn);
    auto tEnd = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();

    CFX_TEST_CHECK(result == ReleaseAllPressedExecutor::Result::Success);
    CFX_TEST_CHECK(static_cast<uint64_t>(elapsed) <= ReleaseAllPressedExecutor::kTotalDeadlineMs);
    CFX_TEST_CHECK(executor.totalElapsedMs() <= ReleaseAllPressedExecutor::kTotalDeadlineMs);
    CFX_TEST_CHECK(executor.releasedCount() == 8);

    printf("  [PASS] test_executor_bounded_within_100ms (elapsed=%lldms)\n",
           (long long)elapsed);
}

int main() {
    printf("=== TASK-042: Group 4 Unit Test Constraints ===\n\n");

    printf("[MacEventInjector]\n");
    test_injector_source_validation();
    test_injector_controller_mode_rejects_all();
    test_injector_param_validation();
    test_injector_keycode_range_validation();
    test_injector_screen_boundary_validation();
    test_injector_injection_method();
    test_injector_all_event_types();
    test_injector_all_button_types();
    test_injector_injection_method_location_compute();
    test_injector_batch();
    test_injector_release_all_pressed_empty();
    test_injector_release_all_pressed_buttons();
    test_injector_release_all_pressed_keys();
    test_injector_release_all_pressed_mixed();
    test_injector_sync_modifiers();
    test_injector_sync_modifiers_explicit_alignment();
    test_injector_sync_modifiers_partial_alignment();
    test_injector_sync_modifiers_idempotent();
    test_injector_set_source_node_id();

    printf("\n[ReleaseAllPressedExecutor]\n");
    test_executor_deadline_constant();
    test_executor_empty_snapshot();
    test_executor_all_released_success();
    test_executor_partial_release();
    test_executor_deadline_reached();
    test_executor_deadline_immediate_stop();
    test_executor_deadline_deterministic_clock();
    test_executor_deadline_deterministic_under_limit();
    test_executor_deadline_deterministic_no_calls_after();
    test_executor_degraded();
    test_executor_execution_count();
    test_executor_result_string();
    test_executor_bounded_within_100ms();

    printf("\n=== All TASK-042 tests passed (32/32) ===\n");
    return 0;
}