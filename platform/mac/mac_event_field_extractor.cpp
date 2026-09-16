#include "mac/mac_event_field_extractor.hpp"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#endif

namespace cfx {

#ifdef __APPLE__

static constexpr uint16_t kVkMap[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
};

static constexpr size_t kVkMapSize = sizeof(kVkMap) / sizeof(kVkMap[0]);

RawInputEventFlat MacEventFieldExtractor::extract(const void* cgEvent) const noexcept {
    RawInputEventFlat out{};
    out.valid = 0;

    if (!cgEvent) {
        return out;
    }

    const CGEventRef event = static_cast<CGEventRef>(cgEvent);
    const CGEventType type = CGEventGetType(event);

    out.platformTime = static_cast<uint64_t>(CGEventGetTimestamp(event));
    out.modifierFlags = static_cast<uint64_t>(CGEventGetFlags(event));
    out.kind = mapEventType(static_cast<uint32_t>(type));

    if (out.kind == 0xFF) {
        return out;
    }

    const auto kind = static_cast<RawEventKind>(out.kind);

    switch (kind) {
        case RawEventKind::MouseMove: {
            out.deltaX = static_cast<int32_t>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaX));
            out.deltaY = static_cast<int32_t>(CGEventGetIntegerValueField(event, kCGMouseEventDeltaY));
            break;
        }
        case RawEventKind::MouseButtonPress:
        case RawEventKind::MouseButtonRelease: {
            const CGMouseButton btn = CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
            out.button = mapMouseButton(static_cast<uint8_t>(btn));
            break;
        }
        case RawEventKind::Wheel: {
            const int32_t delta1 = static_cast<int32_t>(CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1));
            const int32_t delta2 = static_cast<int32_t>(CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis2));
            if (delta1 != 0) {
                out.wheelDelta = delta1;
                out.wheelAxis = static_cast<uint8_t>(WheelAxis::Vertical);
            } else {
                out.wheelDelta = delta2;
                out.wheelAxis = static_cast<uint8_t>(WheelAxis::Horizontal);
            }
            break;
        }
        case RawEventKind::KeyPress:
        case RawEventKind::KeyRelease: {
            const uint16_t vk = static_cast<uint16_t>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
            out.keyCode = mapKeyCode(vk);
            if (out.keyCode == UNKNOWN_KEYCODE) {
                return out;
            }
            break;
        }
    }

    out.valid = 1;
    return out;
}

uint64_t MacEventFieldExtractor::extractPlatformTime(const void* cgEvent) noexcept {
    if (!cgEvent) return 0;
    return static_cast<uint64_t>(CGEventGetTimestamp(static_cast<CGEventRef>(cgEvent)));
}

uint64_t MacEventFieldExtractor::extractModifierFlags(const void* cgEvent) noexcept {
    if (!cgEvent) return 0;
    return static_cast<uint64_t>(CGEventGetFlags(static_cast<CGEventRef>(cgEvent)));
}

uint8_t MacEventFieldExtractor::mapEventType(uint32_t cgEventType) noexcept {
    switch (cgEventType) {
        case kCGEventMouseMoved:
            return static_cast<uint8_t>(RawEventKind::MouseMove);
        case kCGEventLeftMouseDown:
        case kCGEventRightMouseDown:
        case kCGEventOtherMouseDown:
            return static_cast<uint8_t>(RawEventKind::MouseButtonPress);
        case kCGEventLeftMouseUp:
        case kCGEventRightMouseUp:
        case kCGEventOtherMouseUp:
            return static_cast<uint8_t>(RawEventKind::MouseButtonRelease);
        case kCGEventScrollWheel:
            return static_cast<uint8_t>(RawEventKind::Wheel);
        case kCGEventKeyDown:
            return static_cast<uint8_t>(RawEventKind::KeyPress);
        case kCGEventKeyUp:
            return static_cast<uint8_t>(RawEventKind::KeyRelease);
        default:
            return 0xFF;
    }
}

uint16_t MacEventFieldExtractor::mapKeyCode(uint16_t virtualKeyCode) noexcept {
    if (virtualKeyCode < kVkMapSize) {
        return kVkMap[virtualKeyCode];
    }
    return UNKNOWN_KEYCODE;
}

uint8_t MacEventFieldExtractor::mapMouseButton(uint8_t cgMouseButton) noexcept {
    switch (cgMouseButton) {
        case 0:
            return static_cast<uint8_t>(MouseButton::Left);
        case 1:
            return static_cast<uint8_t>(MouseButton::Right);
        case 2:
            return static_cast<uint8_t>(MouseButton::Middle);
        default:
            return static_cast<uint8_t>(MouseButton::Left);
    }
}

#else

RawInputEventFlat MacEventFieldExtractor::extract(const void*) const noexcept {
    RawInputEventFlat out{};
    out.valid = 0;
    return out;
}

uint64_t MacEventFieldExtractor::extractPlatformTime(const void*) noexcept {
    return 0;
}

uint64_t MacEventFieldExtractor::extractModifierFlags(const void*) noexcept {
    return 0;
}

uint8_t MacEventFieldExtractor::mapEventType(uint32_t) noexcept {
    return 0xFF;
}

uint16_t MacEventFieldExtractor::mapKeyCode(uint16_t) noexcept {
    return UNKNOWN_KEYCODE;
}

uint8_t MacEventFieldExtractor::mapMouseButton(uint8_t) noexcept {
    return 0;
}

#endif

RawInputEvent MacEventFieldExtractor::toRawInputEvent(const RawInputEventFlat& flat) const noexcept {
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

}  // namespace cfx