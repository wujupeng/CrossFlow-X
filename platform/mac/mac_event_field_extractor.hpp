#pragma once

#include <cstdint>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"

namespace cfx {

struct RawInputEventFlat {
    uint64_t platformTime;
    uint64_t modifierFlags;
    uint8_t  kind;
    int32_t  deltaX;
    int32_t  deltaY;
    uint8_t  button;
    int32_t  wheelDelta;
    uint8_t  wheelAxis;
    uint16_t keyCode;
    uint8_t  valid;
};

static_assert(std::is_trivially_copyable_v<RawInputEventFlat>,
              "RawInputEventFlat must be trivially copyable for SPSC");

class MacEventFieldExtractor {
public:
    MacEventFieldExtractor() noexcept = default;
    ~MacEventFieldExtractor() noexcept = default;

    MacEventFieldExtractor(const MacEventFieldExtractor&) = delete;
    MacEventFieldExtractor& operator=(const MacEventFieldExtractor&) = delete;

    RawInputEventFlat extract(const void* cgEvent) const noexcept;

    RawInputEvent toRawInputEvent(const RawInputEventFlat& flat) const noexcept;

    static uint64_t extractPlatformTime(const void* cgEvent) noexcept;

    static uint64_t extractModifierFlags(const void* cgEvent) noexcept;

    static uint8_t mapEventType(uint32_t cgEventType) noexcept;

    static uint16_t mapKeyCode(uint16_t virtualKeyCode) noexcept;

    static uint8_t mapMouseButton(uint8_t cgMouseButton) noexcept;

private:
    static constexpr uint16_t UNKNOWN_KEYCODE = 0xFFFF;
};

}  // namespace cfx