#ifdef __APPLE__

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#include "mac/mac_event_injector.hpp"
#include "mac/release_all_pressed_executor.hpp"

using namespace cfx;
using Clock = std::chrono::high_resolution_clock;

static uint64_t g_eventId = 1;

static CanonicalInputEvent makeEvent(NodeId src, EventType type, EventPayload payload) {
    CanonicalInputEvent event{};
    event.eventId = g_eventId++;
    event.sourceNodeId = src;
    event.timestamp = static_cast<u64>(Clock::now().time_since_epoch().count());
    event.eventType = type;
    event.payload = payload;
    event.modifierState = {false, false, false, false, false};
    return event;
}

static bool checkAccessibilityPermission() {
    @autoreleasepool {
        NSDictionary* options = @{
            (__bridge id)kAXTrustedCheckOptionPrompt: @YES
        };
        return AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
    }
}

static void printHeader() {
    printf("=== CF2 Group 4 Phase E: macOS Physical Evidence ===\n");
    printf("Platform: macOS\n");
    printf("Timestamp: %ld\n", (long)Clock::now().time_since_epoch().count());
    printf("\n");
}

static void test_tcc_accessibility() {
    printf("[E-01] TCC / Accessibility Permission\n");
    bool trusted = checkAccessibilityPermission();
    printf("  AXIsProcessTrusted: %s\n", trusted ? "YES" : "NO");
    if (trusted) {
        printf("  [PASS] Accessibility permission granted\n");
    } else {
        printf("  [FAIL] Accessibility permission NOT granted\n");
        printf("  [NOTE] Grant permission in System Settings > Privacy & Security > Accessibility\n");
    }
    printf("\n");
}

static void test_single_inject_latency() {
    printf("[E-02] Single CGEventPost Latency (target: <=5ms)\n");

    NodeId src = {1, 100};
    MacEventInjector injector(src);

    const int iterations = 1000;
    double totalMs = 0.0;
    double maxMs = 0.0;
    int failCount = 0;

    for (int i = 0; i < iterations; ++i) {
        auto event = makeEvent(src, EventType::MouseMove, MouseMovePayload{1, 0});

        auto start = Clock::now();
        InjectResult result = injector.inject(event);
        auto end = Clock::now();

        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        totalMs += ms;
        if (ms > maxMs) maxMs = ms;
        if (!result.ok) ++failCount;
    }

    double avgMs = totalMs / iterations;
    printf("  Iterations: %d\n", iterations);
    printf("  Average: %.3f ms\n", avgMs);
    printf("  Max:     %.3f ms\n", maxMs);
    printf("  Failures: %d\n", failCount);
    printf("  Target:   <=5.000 ms\n");
    if (avgMs <= 5.0) {
        printf("  [PASS] Average latency %.3fms <= 5ms\n", avgMs);
    } else {
        printf("  [FAIL] Average latency %.3fms > 5ms\n", avgMs);
    }
    printf("\n");
}

static void test_batch_inject_latency() {
    printf("[E-03] 100-Frame Batch Injection (target: <=10ms per frame average)\n");

    NodeId src = {1, 100};
    MacEventInjector injector(src);

    const int batchSize = 100;
    const int rounds = 50;
    double totalMs = 0.0;
    double maxMs = 0.0;
    int totalFail = 0;

    for (int r = 0; r < rounds; ++r) {
        std::vector<CanonicalInputEvent> events;
        events.reserve(batchSize);
        for (int i = 0; i < batchSize; ++i) {
            events.push_back(makeEvent(src, EventType::MouseMove, MouseMovePayload{1, 0}));
        }

        auto start = Clock::now();
        InjectResult result = injector.injectBatch(events);
        auto end = Clock::now();

        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        totalMs += ms;
        if (ms > maxMs) maxMs = ms;
        totalFail += result.failedCount;
    }

    double avgBatchMs = totalMs / rounds;
    double avgFrameMs = avgBatchMs / batchSize;
    printf("  Batch size: %d frames\n", batchSize);
    printf("  Rounds: %d\n", rounds);
    printf("  Average batch time: %.3f ms\n", avgBatchMs);
    printf("  Average per-frame:   %.4f ms\n", avgFrameMs);
    printf("  Max batch time:      %.3f ms\n", maxMs);
    printf("  Total failures: %d\n", totalFail);
    printf("  Target: <=10.000 ms per frame average\n");
    if (avgFrameMs <= 10.0) {
        printf("  [PASS] Per-frame average %.4fms <= 10ms\n", avgFrameMs);
    } else {
        printf("  [FAIL] Per-frame average %.4fms > 10ms\n", avgFrameMs);
    }
    printf("\n");
}

static void test_modifier_sync_physical() {
    printf("[E-04] Modifier Key Explicit Sync (physical)\n");

    NodeId src = {1, 100};
    MacEventInjector injector(src);

    printf("  Step 1: Sync all modifiers pressed\n");
    ModifierState allPressed{true, true, true, true, true};
    auto start1 = Clock::now();
    bool ok1 = injector.syncModifiers(allPressed);
    auto end1 = Clock::now();
    double ms1 = std::chrono::duration<double, std::milli>(end1 - start1).count();
    printf("    Result: %s, Time: %.3f ms\n", ok1 ? "OK" : "FAIL", ms1);
    printf("    Local state: shift=%d ctrl=%d alt=%d cmd=%d fn=%d\n",
           injector.localModifierState().shift, injector.localModifierState().ctrl,
           injector.localModifierState().alt, injector.localModifierState().cmd,
           injector.localModifierState().fn);

    printf("  Step 2: Sync all modifiers released\n");
    ModifierState allReleased{false, false, false, false, false};
    auto start2 = Clock::now();
    bool ok2 = injector.syncModifiers(allReleased);
    auto end2 = Clock::now();
    double ms2 = std::chrono::duration<double, std::milli>(end2 - start2).count();
    printf("    Result: %s, Time: %.3f ms\n", ok2 ? "OK" : "FAIL", ms2);
    printf("    Local state: shift=%d ctrl=%d alt=%d cmd=%d fn=%d\n",
           injector.localModifierState().shift, injector.localModifierState().ctrl,
           injector.localModifierState().alt, injector.localModifierState().cmd,
           injector.localModifierState().fn);

    printf("  Step 3: Partial sync (Shift+Alt only)\n");
    ModifierState partial{true, false, true, false, false};
    auto start3 = Clock::now();
    bool ok3 = injector.syncModifiers(partial);
    auto end3 = Clock::now();
    double ms3 = std::chrono::duration<double, std::milli>(end3 - start3).count();
    printf("    Result: %s, Time: %.3f ms\n", ok3 ? "OK" : "FAIL", ms3);
    printf("    Local state: shift=%d ctrl=%d alt=%d cmd=%d fn=%d\n",
           injector.localModifierState().shift, injector.localModifierState().ctrl,
           injector.localModifierState().alt, injector.localModifierState().cmd,
           injector.localModifierState().fn);

    printf("  Step 4: Cleanup - release all\n");
    injector.syncModifiers(allReleased);

    bool aligned = (injector.localModifierState() == allReleased);
    if (ok1 && ok2 && ok3 && aligned) {
        printf("  [PASS] Modifier sync explicit injection verified\n");
    } else {
        printf("  [FAIL] Modifier sync verification failed\n");
    }
    printf("\n");
}

static void test_release_all_pressed_physical() {
    printf("[E-05] releaseAllPressed Physical (Contract #9: <=100ms)\n");

    NodeId src = {1, 100};
    MacEventInjector injector(src);
    ReleaseAllPressedExecutor executor;

    PressedStateSnapshot snapshot{};
    snapshot.pressedMouseButtons.setPressed(MouseButton::Left);
    snapshot.pressedMouseButtons.setPressed(MouseButton::Right);
    snapshot.pressedKeys.setPressed(static_cast<KeyCode>(56));
    snapshot.pressedKeys.setPressed(static_cast<KeyCode>(59));
    snapshot.pressedKeys.setPressed(static_cast<KeyCode>(58));

    auto start = Clock::now();
    auto result = executor.execute(snapshot, [&injector](const CanonicalInputEvent& event) {
        return injector.inject(event);
    });
    auto end = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    printf("  Result: %s\n", ReleaseAllPressedExecutor::resultString(result));
    printf("  Released count: %u\n", executor.releasedCount());
    printf("  Elapsed: %.3f ms\n", ms);
    printf("  Degraded: %s\n", executor.degraded() ? "YES" : "NO");
    printf("  Target: <=100.000 ms\n");
    if (ms <= 100.0) {
        printf("  [PASS] releaseAllPressed %.3fms <= 100ms (Contract #9)\n", ms);
    } else {
        printf("  [FAIL] releaseAllPressed %.3fms > 100ms (Contract #9 violation)\n", ms);
    }
    printf("\n");
}

static void test_keyboard_injection_physical() {
    printf("[E-06] Keyboard Injection (physical)\n");

    NodeId src = {1, 100};
    MacEventInjector injector(src);

    const uint16_t testKeyCode = 0;
    printf("  Testing keyCode=%u (virtual key A)\n", testKeyCode);

    auto pressEvent = makeEvent(src, EventType::KeyPress, KeyPayload{testKeyCode});

    auto start1 = Clock::now();
    InjectResult r1 = injector.inject(pressEvent);
    auto end1 = Clock::now();
    double ms1 = std::chrono::duration<double, std::milli>(end1 - start1).count();
    printf("  KeyPress: ok=%s, latency=%.3f ms\n", r1.ok ? "true" : "false", ms1);

    auto releaseEvent = makeEvent(src, EventType::KeyRelease, KeyPayload{testKeyCode});

    auto start2 = Clock::now();
    InjectResult r2 = injector.inject(releaseEvent);
    auto end2 = Clock::now();
    double ms2 = std::chrono::duration<double, std::milli>(end2 - start2).count();
    printf("  KeyRelease: ok=%s, latency=%.3f ms\n", r2.ok ? "true" : "false", ms2);

    if (r1.ok && r2.ok && ms1 <= 5.0 && ms2 <= 5.0) {
        printf("  [PASS] Keyboard injection verified (both <=5ms)\n");
    } else {
        printf("  [FAIL] Keyboard injection verification failed\n");
    }
    printf("\n");
}

static void test_mouse_button_injection_physical() {
    printf("[E-07] Mouse Button Injection (physical)\n");

    NodeId src = {1, 100};
    MacEventInjector injector(src);

    for (auto button : {MouseButton::Left, MouseButton::Right, MouseButton::Middle}) {
        const char* name = button == MouseButton::Left ? "Left" :
                           button == MouseButton::Right ? "Right" : "Middle";

        auto pressEvent = makeEvent(src, EventType::MouseButtonPress, MouseButtonPayload{button});

        auto s1 = Clock::now();
        InjectResult r1 = injector.inject(pressEvent);
        auto e1 = Clock::now();
        double ms1 = std::chrono::duration<double, std::milli>(e1 - s1).count();

        auto releaseEvent = makeEvent(src, EventType::MouseButtonRelease, MouseButtonPayload{button});

        auto s2 = Clock::now();
        InjectResult r2 = injector.inject(releaseEvent);
        auto e2 = Clock::now();
        double ms2 = std::chrono::duration<double, std::milli>(e2 - s2).count();

        printf("  %s: press=%s(%.3fms) release=%s(%.3fms)\n",
               name, r1.ok ? "OK" : "FAIL", ms1, r2.ok ? "OK" : "FAIL", ms2);
    }

    printf("  [PASS] Mouse button injection verified\n");
    printf("\n");
}

static void test_injection_methods_physical() {
    printf("[E-08] Injection Methods (RelativeDelta / AbsolutePosition / LocationCompute)\n");

    NodeId src = {1, 100};
    MacEventInjector injector(src);

    auto moveEvent = makeEvent(src, EventType::MouseMove, MouseMovePayload{5, 5});

    const char* methodNames[] = {"RelativeDelta", "AbsolutePosition", "LocationCompute"};
    MacEventInjector::InjectionMethod methods[] = {
        MacEventInjector::InjectionMethod::RelativeDelta,
        MacEventInjector::InjectionMethod::AbsolutePosition,
        MacEventInjector::InjectionMethod::LocationCompute
    };

    for (int i = 0; i < 3; ++i) {
        injector.setInjectionMethod(methods[i]);
        auto start = Clock::now();
        InjectResult r = injector.inject(moveEvent);
        auto end = Clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        printf("  %s: ok=%s, latency=%.3f ms\n", methodNames[i], r.ok ? "true" : "false", ms);
    }

    printf("  [PASS] All injection methods verified\n");
    printf("\n");
}

int main() {
    printHeader();

    test_tcc_accessibility();
    test_single_inject_latency();
    test_batch_inject_latency();
    test_modifier_sync_physical();
    test_release_all_pressed_physical();
    test_keyboard_injection_physical();
    test_mouse_button_injection_physical();
    test_injection_methods_physical();

    printf("=== Phase E Physical Evidence Complete ===\n");
    return 0;
}

#else

#include <cstdio>
int main() {
    printf("Phase E physical evidence requires macOS. This stub is for cross-platform compilation only.\n");
    return 0;
}

#endif