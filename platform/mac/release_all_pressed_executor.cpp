#include "mac/release_all_pressed_executor.hpp"

#include <chrono>

namespace cfx {

const char* ReleaseAllPressedExecutor::resultString(Result r) noexcept {
    switch (r) {
        case Result::Success:        return "Success";
        case Result::PartialRelease: return "PartialRelease";
        case Result::DeadlineReached: return "DeadlineReached";
        case Result::Degraded:       return "Degraded";
    }
    return "Unknown";
}

ReleaseAllPressedExecutor::Result ReleaseAllPressedExecutor::execute(
    const PressedStateSnapshot& snapshot,
    std::function<InjectResult(const CanonicalInputEvent&)> injectFn,
    ClockFn clockFn) noexcept {

    const auto tStart = clockFn();
    const auto tDeadline = tStart + std::chrono::milliseconds(kTotalDeadlineMs);

    executionCount_.fetch_add(1, std::memory_order_relaxed);

    uint32_t released = 0;
    NodeId sourceId{};

    for (uint8_t btn = 0; btn < 8; ++btn) {
        if (snapshot.pressedMouseButtons.isPressed(static_cast<MouseButton>(btn))) {
            if (clockFn() >= tDeadline) {
                lastResult_.store(static_cast<uint8_t>(Result::DeadlineReached), std::memory_order_release);
                goto done;
            }

            CanonicalInputEvent event{};
            event.eventType = EventType::MouseButtonRelease;
            event.payload = MouseButtonPayload{static_cast<MouseButton>(btn)};
            event.sourceNodeId = sourceId;

            InjectResult r = injectFn(event);
            if (r.ok) {
                ++released;
            }
            if (clockFn() >= tDeadline) {
                lastResult_.store(static_cast<uint8_t>(Result::DeadlineReached), std::memory_order_release);
                goto done;
            }
        }
    }

    for (uint16_t code = 0; code < 256; ++code) {
        if (snapshot.pressedKeys.isPressed(static_cast<KeyCode>(code))) {
            if (clockFn() >= tDeadline) {
                lastResult_.store(static_cast<uint8_t>(Result::DeadlineReached), std::memory_order_release);
                goto done;
            }

            CanonicalInputEvent event{};
            event.eventType = EventType::KeyRelease;
            event.payload = KeyPayload{static_cast<KeyCode>(code)};
            event.sourceNodeId = sourceId;

            InjectResult r = injectFn(event);
            if (r.ok) {
                ++released;
            }
            if (clockFn() >= tDeadline) {
                lastResult_.store(static_cast<uint8_t>(Result::DeadlineReached), std::memory_order_release);
                goto done;
            }
        }
    }

done:
    {
        auto tEnd = clockFn();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count();
        totalElapsedMs_.store(static_cast<uint32_t>(elapsed), std::memory_order_relaxed);
        releasedCount_.store(released, std::memory_order_relaxed);

        if (static_cast<Result>(lastResult_.load(std::memory_order_acquire)) != Result::DeadlineReached) {
            uint32_t totalPressed = snapshot.pressedMouseButtons.count() + static_cast<uint32_t>(snapshot.pressedKeys.count());
            if (released == totalPressed && totalPressed > 0) {
                lastResult_.store(static_cast<uint8_t>(Result::Success), std::memory_order_release);
            } else if (released > 0) {
                lastResult_.store(static_cast<uint8_t>(Result::PartialRelease), std::memory_order_release);
            } else if (totalPressed == 0) {
                lastResult_.store(static_cast<uint8_t>(Result::Success), std::memory_order_release);
            } else {
                lastResult_.store(static_cast<uint8_t>(Result::Degraded), std::memory_order_release);
            }
        }
    }

    return static_cast<Result>(lastResult_.load(std::memory_order_acquire));
}

}  // namespace cfx
