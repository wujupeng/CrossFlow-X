#include "mac/cg_event_normalizer.hpp"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace cfx {

CGEventNormalizer::CGEventNormalizer() noexcept
    : onEvent_{}, boundary_{} {}

#ifdef __APPLE__

ModifierState CGEventNormalizer::extractModifierState(uint64_t cgEventFlags) noexcept {
    ModifierState mods{};
    mods.shift = (cgEventFlags & kCGEventFlagMaskShift) != 0;
    mods.ctrl  = (cgEventFlags & kCGEventFlagMaskControl) != 0;
    mods.alt   = (cgEventFlags & kCGEventFlagMaskAlternate) != 0;
    mods.cmd   = (cgEventFlags & kCGEventFlagMaskCommand) != 0;
    mods.fn    = (cgEventFlags & kCGEventFlagMaskSecondaryFn) != 0;
    return mods;
}

#else

ModifierState CGEventNormalizer::extractModifierState(uint64_t) noexcept {
    ModifierState mods{};
    return mods;
}

#endif

RawInputEvent CGEventNormalizer::toRawInputEvent(const RawInputEventFlat& flat) noexcept {
    RawInputEvent event{};
    event.platformTime = flat.platformTime;
    event.kind = static_cast<RawEventKind>(flat.kind);

    switch (event.kind) {
        case RawEventKind::MouseMove:
            event.payload = RawMouseMovePayload{flat.deltaX, flat.deltaY};
            break;
        case RawEventKind::MouseButtonPress:
        case RawEventKind::MouseButtonRelease:
            event.payload = RawMouseButtonPayload{static_cast<MouseButton>(flat.button)};
            break;
        case RawEventKind::Wheel:
            event.payload = RawWheelPayload{flat.wheelDelta, static_cast<WheelAxis>(flat.wheelAxis)};
            break;
        case RawEventKind::KeyPress:
        case RawEventKind::KeyRelease:
            event.payload = RawKeyPayload{flat.keyCode};
            break;
    }

    return event;
}

void CGEventNormalizer::normalize(const RawInputEventFlat& flat) noexcept {
    if (!flat.valid) {
        unknownEventCount_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    if (detectEdgeOverflow(flat)) {
        edgeOverflowCount_.fetch_add(1, std::memory_order_relaxed);
    }

    if (onEvent_) {
        const RawInputEvent event = toRawInputEvent(flat);
        onEvent_(event);
    }

    normalizedCount_.fetch_add(1, std::memory_order_relaxed);
}

bool CGEventNormalizer::detectEdgeOverflow(const RawInputEventFlat& flat) const noexcept {
    if (static_cast<RawEventKind>(flat.kind) != RawEventKind::MouseMove) {
        return false;
    }
    if (!boundary_.isValid()) {
        return false;
    }

    const int64_t newX = static_cast<int64_t>(boundary_.originX) + flat.deltaX;
    const int64_t newY = static_cast<int64_t>(boundary_.originY) + flat.deltaY;

    if (newX < 0 || newX >= static_cast<int64_t>(boundary_.originX + boundary_.width)) {
        return true;
    }
    if (newY < 0 || newY >= static_cast<int64_t>(boundary_.originY + boundary_.height)) {
        return true;
    }
    return false;
}

}  // namespace cfx