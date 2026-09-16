#pragma once

#include <array>
#include <cstdint>
#include <functional>

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

struct MouseButtonBitmap {
    u8 bits{0};

    bool isPressed(MouseButton btn) const noexcept {
        return (bits >> static_cast<u8>(btn)) & 1u;
    }
    void setPressed(MouseButton btn) noexcept {
        bits |= (1u << static_cast<u8>(btn));
    }
    void clearPressed(MouseButton btn) noexcept {
        bits &= ~(1u << static_cast<u8>(btn));
    }
    void clear() noexcept { bits = 0; }
    bool anyPressed() const noexcept { return bits != 0; }
    u8 count() const noexcept {
        u8 c = 0;
        u8 v = bits;
        while (v) { c += v & 1u; v >>= 1; }
        return c;
    }
};

struct KeyCodeBitmap {
    std::array<u64, 4> words{};

    bool isPressed(KeyCode code) const noexcept {
        if (code >= 256) return false;
        const auto wordIdx = code / 64;
        const auto bitIdx = code % 64;
        return (words[wordIdx] >> bitIdx) & 1ull;
    }
    void setPressed(KeyCode code) noexcept {
        if (code >= 256) return;
        const auto wordIdx = code / 64;
        const auto bitIdx = code % 64;
        words[wordIdx] |= (1ull << bitIdx);
    }
    void clearPressed(KeyCode code) noexcept {
        if (code >= 256) return;
        const auto wordIdx = code / 64;
        const auto bitIdx = code % 64;
        words[wordIdx] &= ~(1ull << bitIdx);
    }
    void clear() noexcept { words.fill(0); }
    bool anyPressed() const noexcept {
        for (const auto& w : words) { if (w) return true; }
        return false;
    }
    u16 count() const noexcept {
        u16 c = 0;
        for (const auto& w : words) {
            u64 v = w;
            while (v) { c += static_cast<u16>(v & 1ull); v >>= 1; }
        }
        return c;
    }
};

struct PressedStateSnapshot {
    ModifierState modifiers;
    MouseButtonBitmap pressedMouseButtons;
    KeyCodeBitmap pressedKeys;
    bool stale{false};
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