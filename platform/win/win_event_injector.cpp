#include "win/win_event_injector.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <cstdio>

namespace cfx {

WinEventInjector::WinEventInjector(NodeId sourceNodeId) noexcept
    : sourceNodeId_(sourceNodeId) {}

bool WinEventInjector::validateSource(const CanonicalInputEvent& event) const noexcept {
    if (event.sourceNodeId != sourceNodeId_) {
        fprintf(stderr, "[CFX-E-INJ-UNAUTHORIZED-SOURCE] rejected event from non-controller source\n");
        return false;
    }
    return true;
}

InjectResult WinEventInjector::inject(const CanonicalInputEvent& event) noexcept {
    if (!validateSource(event)) {
        totalRejected_.fetch_add(1, std::memory_order_relaxed);
        return {false, 1, 0};
    }

    auto start = std::chrono::steady_clock::now();

    InjectResult result;
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
        default:
            result = {false, 1, 0};
            break;
    }

    auto end = std::chrono::steady_clock::now();
    result.latencyUs = static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

    if (result.ok) {
        totalInjected_.fetch_add(1, std::memory_order_relaxed);
    } else {
        totalRejected_.fetch_add(1, std::memory_order_relaxed);
    }

    return result;
}

InjectResult WinEventInjector::injectBatch(const std::vector<CanonicalInputEvent>& events) noexcept {
    u32 failedCount = 0;
    u64 totalLatency = 0;

    for (const auto& event : events) {
        auto result = inject(event);
        if (!result.ok) {
            ++failedCount;
        }
        totalLatency += result.latencyUs;
    }

    return {failedCount == 0, failedCount, totalLatency};
}

InjectResult WinEventInjector::injectMouseEvent(const CanonicalInputEvent& event) noexcept {
#ifdef _WIN32
    const auto& payload = std::get<MouseMovePayload>(event.payload);

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = payload.deltaX;
    input.mi.dy = payload.deltaY;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;

    UINT sent = SendInput(1, &input, sizeof(INPUT));
    return {sent == 1, static_cast<u32>(sent == 1 ? 0 : 1), 0};
#else
    (void)event;
    return {true, 0, 0};
#endif
}

InjectResult WinEventInjector::injectMouseButtonEvent(const CanonicalInputEvent& event) noexcept {
#ifdef _WIN32
    const auto& payload = std::get<MouseButtonPayload>(event.payload);

    INPUT input{};
    input.type = INPUT_MOUSE;

    if (payload.button == MouseButton::Left) {
        input.mi.dwFlags = (event.eventType == EventType::MouseButtonPress)
            ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    } else if (payload.button == MouseButton::Right) {
        input.mi.dwFlags = (event.eventType == EventType::MouseButtonPress)
            ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    } else if (payload.button == MouseButton::Middle) {
        input.mi.dwFlags = (event.eventType == EventType::MouseButtonPress)
            ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    } else {
        return {false, 1, 0};
    }

    UINT sent = SendInput(1, &input, sizeof(INPUT));
    return {sent == 1, static_cast<u32>(sent == 1 ? 0 : 1), 0};
#else
    (void)event;
    return {true, 0, 0};
#endif
}

InjectResult WinEventInjector::injectWheelEvent(const CanonicalInputEvent& event) noexcept {
#ifdef _WIN32
    const auto& payload = std::get<WheelPayload>(event.payload);

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = (payload.axis == WheelAxis::Vertical)
        ? MOUSEEVENTF_WHEEL : MOUSEEVENTF_HWHEEL;
    input.mi.mouseData = static_cast<DWORD>(payload.delta);

    UINT sent = SendInput(1, &input, sizeof(INPUT));
    return {sent == 1, static_cast<u32>(sent == 1 ? 0 : 1), 0};
#else
    (void)event;
    return {true, 0, 0};
#endif
}

InjectResult WinEventInjector::injectKeyEvent(const CanonicalInputEvent& event) noexcept {
#ifdef _WIN32
    const auto& payload = std::get<KeyPayload>(event.payload);

    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(payload.keyCode);
    input.ki.dwFlags = (event.eventType == EventType::KeyRelease)
        ? KEYEVENTF_KEYUP : 0;

    UINT sent = SendInput(1, &input, sizeof(INPUT));
    return {sent == 1, static_cast<u32>(sent == 1 ? 0 : 1), 0};
#else
    (void)event;
    return {true, 0, 0};
#endif
}

bool WinEventInjector::syncModifiers(const ModifierState& sourceState) noexcept {
#ifdef _WIN32
    struct ModMapping {
        WORD vk;
        bool ModifierState::* field;
    };
    static const ModMapping mods[] = {
        {VK_SHIFT,   &ModifierState::shift},
        {VK_CONTROL, &ModifierState::ctrl},
        {VK_MENU,    &ModifierState::alt},
        {VK_LWIN,    &ModifierState::cmd},
    };

    for (const auto& m : mods) {
        bool localPressed = (GetAsyncKeyState(m.vk) & 0x8000) != 0;
        bool sourcePressed = sourceState.*(m.field);

        if (sourcePressed && !localPressed) {
            INPUT down{};
            down.type = INPUT_KEYBOARD;
            down.ki.wVk = m.vk;
            SendInput(1, &down, sizeof(INPUT));
        } else if (!sourcePressed && localPressed) {
            INPUT up{};
            up.type = INPUT_KEYBOARD;
            up.ki.wVk = m.vk;
            up.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &up, sizeof(INPUT));
        }
    }

    localModifierState_ = sourceState;
    return true;
#else
    localModifierState_ = sourceState;
    return true;
#endif
}

ReleaseResult WinEventInjector::releaseAllPressed(const PressedStateSnapshot& pressed) noexcept {
#ifdef _WIN32
    auto start = std::chrono::steady_clock::now();
    u32 releasedCount = 0;

    if (pressed.pressedMouseButtons.isPressed(MouseButton::Left)) {
        INPUT up{};
        up.type = INPUT_MOUSE;
        up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &up, sizeof(INPUT));
        ++releasedCount;
    }
    if (pressed.pressedMouseButtons.isPressed(MouseButton::Right)) {
        INPUT up{};
        up.type = INPUT_MOUSE;
        up.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        SendInput(1, &up, sizeof(INPUT));
        ++releasedCount;
    }
    if (pressed.pressedMouseButtons.isPressed(MouseButton::Middle)) {
        INPUT up{};
        up.type = INPUT_MOUSE;
        up.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        SendInput(1, &up, sizeof(INPUT));
        ++releasedCount;
    }

    for (KeyCode code = 0; code < 256; ++code) {
        if (pressed.pressedKeys.isPressed(code)) {
            INPUT up{};
            up.type = INPUT_KEYBOARD;
            up.ki.wVk = static_cast<WORD>(code);
            up.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &up, sizeof(INPUT));
            ++releasedCount;
        }
    }

    localModifierState_ = ModifierState{false, false, false, false, false};

    auto end = std::chrono::steady_clock::now();
    auto latencyMs = static_cast<u32>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    return {releasedCount, latencyMs};
#else
    return {0, 0};
#endif
}

}  // namespace cfx