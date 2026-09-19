#include "mac/mac_event_injector.hpp"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <atomic>
#include <cstdio>

namespace cfx {

MacEventInjector::MacEventInjector(NodeId sourceNodeId) noexcept
    : sourceNodeId_(sourceNodeId) {}

bool MacEventInjector::validateSource(const CanonicalInputEvent& event) const noexcept {
    if (isController_) {
        fprintf(stderr, "[CFX-E-INJ-UNAUTHORIZED-SOURCE] controller mode rejects injection\n");
        return false;
    }
    if (event.sourceNodeId != sourceNodeId_) {
        fprintf(stderr, "[CFX-E-INJ-UNAUTHORIZED-SOURCE] source node mismatch\n");
        return false;
    }
    return true;
}

bool MacEventInjector::validateParams(const CanonicalInputEvent& event) const noexcept {
    if (!event.validate()) {
        fprintf(stderr, "[CFX-W-INJ-INVALID-PARAM] event.validate() failed\n");
        return false;
    }

    if (event.eventType == EventType::KeyPress || event.eventType == EventType::KeyRelease) {
        const auto& payload = std::get<KeyPayload>(event.payload);
        if (payload.keyCode >= 256) {
            fprintf(stderr, "[CFX-W-INJ-INVALID-PARAM] keyCode %u out of range [0,255]\n", payload.keyCode);
            return false;
        }
    }

    if (event.eventType == EventType::MouseButtonPress || event.eventType == EventType::MouseButtonRelease) {
        const auto& payload = std::get<MouseButtonPayload>(event.payload);
        if (static_cast<uint8_t>(payload.button) > static_cast<uint8_t>(MouseButton::Middle)) {
            fprintf(stderr, "[CFX-W-INJ-INVALID-PARAM] button %u out of enum range\n", static_cast<uint8_t>(payload.button));
            return false;
        }
    }

    if (event.eventType == EventType::MouseMove && injectionMethod_ == InjectionMethod::AbsolutePosition) {
        const auto& payload = std::get<MouseMovePayload>(event.payload);
        if (screenBoundary_.isValid()) {
            if (payload.deltaX < 0 || payload.deltaY < 0) {
                fprintf(stderr, "[CFX-W-INJ-INVALID-PARAM] absolute position (%d,%d) negative\n", payload.deltaX, payload.deltaY);
                return false;
            }
            if (static_cast<uint32_t>(payload.deltaX) > screenBoundary_.originX + screenBoundary_.width ||
                static_cast<uint32_t>(payload.deltaY) > screenBoundary_.originY + screenBoundary_.height) {
                fprintf(stderr, "[CFX-W-INJ-INVALID-PARAM] absolute position (%d,%d) exceeds screen (%u,%u)\n",
                        payload.deltaX, payload.deltaY,
                        screenBoundary_.originX + screenBoundary_.width,
                        screenBoundary_.originY + screenBoundary_.height);
                return false;
            }
        }
    }

    return true;
}

#ifdef __APPLE__

InjectResult MacEventInjector::injectMouseEvent(const CanonicalInputEvent& event) noexcept {
    const auto& payload = std::get<MouseMovePayload>(event.payload);

    if (injectionMethod_ == InjectionMethod::AbsolutePosition) {
        CGEventRef moveEvent = CGEventCreateMouseEvent(
            nullptr, kCGEventMouseMoved,
            CGPointMake(static_cast<CGFloat>(payload.deltaX), static_cast<CGFloat>(payload.deltaY)),
            kCGMouseButtonLeft);
        if (!moveEvent) return {false, 1, 0};
        CGEventPost(kCGHIDEventTap, moveEvent);
        CFRelease(moveEvent);
    } else if (injectionMethod_ == InjectionMethod::LocationCompute) {
        CGEventRef locEvent = CGEventCreate(nullptr);
        if (!locEvent) return {false, 1, 0};
        CGPoint cursor = CGEventGetLocation(locEvent);
        CFRelease(locEvent);
        CGFloat targetX = cursor.x + static_cast<CGFloat>(payload.deltaX);
        CGFloat targetY = cursor.y + static_cast<CGFloat>(payload.deltaY);
        CGEventRef moveEvent = CGEventCreateMouseEvent(
            nullptr, kCGEventMouseMoved, CGPointMake(targetX, targetY), kCGMouseButtonLeft);
        if (!moveEvent) return {false, 1, 0};
        CGEventPost(kCGHIDEventTap, moveEvent);
        CFRelease(moveEvent);
    } else {
        CGEventRef moveEvent = CGEventCreateMouseEvent(
            nullptr, kCGEventMouseMoved, CGPointMake(0, 0), kCGMouseButtonLeft);
        if (!moveEvent) return {false, 1, 0};
        CGEventSetDoubleValueField(moveEvent, kCGMouseEventDeltaX, static_cast<double>(payload.deltaX));
        CGEventSetDoubleValueField(moveEvent, kCGMouseEventDeltaY, static_cast<double>(payload.deltaY));
        CGEventPost(kCGHIDEventTap, moveEvent);
        CFRelease(moveEvent);
    }
    return {true, 0, 0};
}

InjectResult MacEventInjector::injectMouseButtonEvent(const CanonicalInputEvent& event) noexcept {
    const auto& payload = std::get<MouseButtonPayload>(event.payload);
    const bool isPress = (event.eventType == EventType::MouseButtonPress);

    CGEventType type;
    CGMouseButton button;
    switch (payload.button) {
        case MouseButton::Left:
            type = isPress ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
            button = kCGMouseButtonLeft;
            break;
        case MouseButton::Right:
            type = isPress ? kCGEventRightMouseDown : kCGEventRightMouseUp;
            button = kCGMouseButtonRight;
            break;
        case MouseButton::Middle:
            type = isPress ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
            button = kCGMouseButtonCenter;
            break;
        default:
            return {false, 1, 0};
    }

    CGEventRef btnEvent = CGEventCreateMouseEvent(nullptr, type, CGPointMake(0, 0), button);
    if (!btnEvent) return {false, 1, 0};
    CGEventPost(kCGHIDEventTap, btnEvent);
    CFRelease(btnEvent);
    return {true, 0, 0};
}

InjectResult MacEventInjector::injectWheelEvent(const CanonicalInputEvent& event) noexcept {
    const auto& payload = std::get<WheelPayload>(event.payload);
    CGEventRef scrollEvent = CGEventCreateScrollWheelEvent(
        nullptr, kCGScrollEventUnitLine, 1, static_cast<int32_t>(payload.delta));
    if (!scrollEvent) return {false, 1, 0};
    CGEventPost(kCGHIDEventTap, scrollEvent);
    CFRelease(scrollEvent);
    return {true, 0, 0};
}

InjectResult MacEventInjector::injectKeyEvent(const CanonicalInputEvent& event) noexcept {
    const auto& payload = std::get<KeyPayload>(event.payload);
    const bool keyDown = (event.eventType == EventType::KeyPress);
    CGEventRef keyEvent = CGEventCreateKeyboardEvent(nullptr, static_cast<CGKeyCode>(payload.keyCode), keyDown);
    if (!keyEvent) return {false, 1, 0};
    CGEventPost(kCGHIDEventTap, keyEvent);
    CFRelease(keyEvent);
    return {true, 0, 0};
}

#endif

InjectResult MacEventInjector::inject(const CanonicalInputEvent& event) noexcept {
    if (!validateSource(event) || !validateParams(event)) {
        totalRejected_.fetch_add(1, std::memory_order_relaxed);
        return {false, 1, 0};
    }

    InjectResult result{false, 1, 0};

    switch (event.eventType) {
        case EventType::MouseMove:
            result = injectMouseEvent(event);
            break;
        case EventType::MouseButtonPress:
        case EventType::MouseButtonRelease:
            result = injectMouseButtonEvent(event);
            break;
        case EventType::Wheel:
            result = injectWheelEvent(event);
            break;
        case EventType::KeyPress:
        case EventType::KeyRelease:
            result = injectKeyEvent(event);
            break;
    }

    if (result.ok) {
        totalInjected_.fetch_add(1, std::memory_order_relaxed);
    } else {
        totalRejected_.fetch_add(1, std::memory_order_relaxed);
    }
    return result;
}

InjectResult MacEventInjector::injectBatch(const std::vector<CanonicalInputEvent>& events) noexcept {
    u32 failedCount = 0;
    for (const auto& event : events) {
        InjectResult r = inject(event);
        if (!r.ok) ++failedCount;
    }
    return {failedCount == 0, failedCount, 0};
}

ReleaseResult MacEventInjector::releaseAllPressed(const PressedStateSnapshot& pressed) noexcept {
    u32 releasedCount = 0;

    for (uint8_t btn = 0; btn < 8; ++btn) {
        if (pressed.pressedMouseButtons.isPressed(static_cast<MouseButton>(btn))) {
            ++releasedCount;
        }
    }

    for (uint16_t code = 0; code < 256; ++code) {
        if (pressed.pressedKeys.isPressed(static_cast<KeyCode>(code))) {
            ++releasedCount;
        }
    }

    return {releasedCount, 0};
}

bool MacEventInjector::syncModifiers(const ModifierState& sourceState) noexcept {
#ifdef __APPLE__
    CGEventFlags flags = 0;
    if (sourceState.shift) flags |= kCGEventFlagMaskShift;
    if (sourceState.ctrl)  flags |= kCGEventFlagMaskControl;
    if (sourceState.alt)   flags |= kCGEventFlagMaskAlternate;
    if (sourceState.cmd)   flags |= kCGEventFlagMaskCommand;
    if (sourceState.fn)    flags |= kCGEventFlagMaskSecondaryFn;

    CGEventRef event = CGEventCreate(nullptr);
    if (!event) return false;
    CGEventSetFlags(event, flags);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
    return true;
#else
    (void)sourceState;
    return true;
#endif
}

}  // namespace cfx