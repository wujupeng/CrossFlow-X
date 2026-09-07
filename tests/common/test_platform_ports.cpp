#include "common/domain.hpp"
#include "common/platform_ports.hpp"

namespace {

int testRawInputEventFields() {
    using namespace cfx;
    RawInputEvent ev{};
    ev.platformTime = 12345;
    ev.kind = RawEventKind::MouseMove;
    ev.payload = RawMouseMovePayload{10, 20};

    if (ev.platformTime != 12345) return 1;
    if (ev.kind != RawEventKind::MouseMove) return 1;
    if (!std::holds_alternative<RawMouseMovePayload>(ev.payload)) return 1;
    if (std::get<RawMouseMovePayload>(ev.payload).deltaX != 10) return 1;
    if (std::get<RawMouseMovePayload>(ev.payload).deltaY != 20) return 1;
    return 0;
}

int testCaptureHandle() {
    using namespace cfx;
    CaptureHandle h{1, true};
    if (h.id != 1) return 1;
    if (!h.active) return 1;
    return 0;
}

int testInjectResult() {
    using namespace cfx;
    InjectResult r{true, 0, 100};
    if (!r.ok) return 1;
    if (r.failedCount != 0) return 1;
    if (r.latencyUs != 100) return 1;
    return 0;
}

int testPressedStateSnapshot() {
    using namespace cfx;
    PressedStateSnapshot snap{};
    snap.modifiers = ModifierState{true, false, false, false, false};
    snap.pressedMouseButtons = {MouseButton::Left};
    snap.pressedKeys = {65, 66};

    if (!snap.modifiers.shift) return 1;
    if (snap.pressedMouseButtons.size() != 1) return 1;
    if (snap.pressedKeys.size() != 2) return 1;
    return 0;
}

int testMockInterfaces() {
    using namespace cfx;

    struct MockCapture : IInputCapture {
        CaptureHandle start(std::function<void(const RawInputEvent&)> onEvent) override {
            (void)onEvent;
            return CaptureHandle{1, true};
        }
        void stop(CaptureHandle handle) override { (void)handle; }
        ScreenBoundary queryScreenBoundary() override {
            return ScreenBoundary{1920, 1080, 0, 0};
        }
    };

    struct MockInjector : IInputInjector {
        InjectResult inject(const CanonicalInputEvent& event) override {
            (void)event;
            return InjectResult{true, 0, 50};
        }
        InjectResult injectBatch(const std::vector<CanonicalInputEvent>& events) override {
            return InjectResult{true, 0, static_cast<u64>(events.size()) * 50};
        }
        ReleaseResult releaseAllPressed(const PressedStateSnapshot& pressed) override {
            return ReleaseResult{static_cast<u32>(pressed.pressedKeys.size()), 1};
        }
    };

    struct MockClock : IMonotonicClock {
        u64 nowUs() override { return 1000; }
    };

    struct MockScreenQuery : IScreenQuery {
        ScreenBoundary primaryBoundary() override {
            return ScreenBoundary{1920, 1080, 0, 0};
        }
    };

    MockCapture cap;
    MockInjector inj;
    MockClock clock;
    MockScreenQuery query;

    auto handle = cap.start([](const RawInputEvent&) {});
    if (!handle.active) return 1;
    cap.stop(handle);

    auto sb = cap.queryScreenBoundary();
    if (sb.width != 1920) return 1;

    CanonicalInputEvent ev{};
    ev.eventId = 1;
    ev.sourceNodeId = NodeId::generate();
    ev.timestamp = 100;
    ev.eventType = EventType::MouseMove;
    ev.payload = MouseMovePayload{1, 1};
    ev.modifierState = ModifierState{false, false, false, false, false};
    auto ir = inj.inject(ev);
    if (!ir.ok) return 1;

    if (clock.nowUs() != 1000) return 1;
    if (query.primaryBoundary().height != 1080) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testRawInputEventFields()) return 1;
    if (testCaptureHandle()) return 1;
    if (testInjectResult()) return 1;
    if (testPressedStateSnapshot()) return 1;
    if (testMockInterfaces()) return 1;
    return 0;
}