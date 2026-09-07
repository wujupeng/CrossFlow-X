#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "common/domain.hpp"

namespace cfx {

enum class RawEventKind : u8 {
    MouseMove,
    MouseButtonPress,
    MouseButtonRelease,
    Wheel,
    KeyPress,
    KeyRelease,
};

struct RawMouseMovePayload {
    i32 deltaX;
    i32 deltaY;
};

struct RawMouseButtonPayload {
    MouseButton button;
};

struct RawWheelPayload {
    i32 delta;
    WheelAxis axis;
};

struct RawKeyPayload {
    KeyCode keyCode;
};

using RawPayload = std::variant<RawMouseMovePayload, RawMouseButtonPayload, RawWheelPayload, RawKeyPayload>;

struct RawInputEvent {
    u64 platformTime;
    RawEventKind kind;
    RawPayload payload;
};

struct CaptureHandle {
    u32 id;
    bool active;
};

struct InjectResult {
    bool ok;
    u32 failedCount;
    u64 latencyUs;
};

struct PressedStateSnapshot {
    ModifierState modifiers;
    std::vector<MouseButton> pressedMouseButtons;
    std::vector<KeyCode> pressedKeys;
};

struct ReleaseResult {
    u32 releasedCount;
    u32 latencyMs;
};

class IInputCapture {
public:
    virtual ~IInputCapture() = default;
    virtual CaptureHandle start(std::function<void(const RawInputEvent&)> onEvent) = 0;
    virtual void stop(CaptureHandle handle) = 0;
    virtual ScreenBoundary queryScreenBoundary() = 0;
};

class IInputInjector {
public:
    virtual ~IInputInjector() = default;
    virtual InjectResult inject(const CanonicalInputEvent& event) = 0;
    virtual InjectResult injectBatch(const std::vector<CanonicalInputEvent>& events) = 0;
    virtual ReleaseResult releaseAllPressed(const PressedStateSnapshot& pressed) = 0;
};

class IMonotonicClock {
public:
    virtual ~IMonotonicClock() = default;
    virtual u64 nowUs() = 0;
};

class IScreenQuery {
public:
    virtual ~IScreenQuery() = default;
    virtual ScreenBoundary primaryBoundary() = 0;
};

}  // namespace cfx