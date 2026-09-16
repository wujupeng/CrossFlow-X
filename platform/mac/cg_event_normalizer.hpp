#pragma once

#include <cstdint>
#include <functional>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "mac/mac_event_field_extractor.hpp"

namespace cfx {

class CGEventNormalizer {
public:
    CGEventNormalizer() noexcept;
    ~CGEventNormalizer() noexcept = default;

    CGEventNormalizer(const CGEventNormalizer&) = delete;
    CGEventNormalizer& operator=(const CGEventNormalizer&) = delete;

    void setOnEvent(std::function<void(const RawInputEvent&)> onEvent) noexcept {
        onEvent_ = std::move(onEvent);
    }

    void setScreenBoundary(const ScreenBoundary& boundary) noexcept {
        boundary_ = boundary;
    }

    void normalize(const RawInputEventFlat& flat) noexcept;

    bool detectEdgeOverflow(const RawInputEventFlat& flat) const noexcept;

    uint64_t normalizedCount() const noexcept {
        return normalizedCount_.load(std::memory_order_relaxed);
    }

    uint64_t edgeOverflowCount() const noexcept {
        return edgeOverflowCount_.load(std::memory_order_relaxed);
    }

    uint64_t unknownEventCount() const noexcept {
        return unknownEventCount_.load(std::memory_order_relaxed);
    }

    static ModifierState extractModifierState(uint64_t cgEventFlags) noexcept;

    static RawInputEvent toRawInputEvent(const RawInputEventFlat& flat) noexcept;

private:
    std::function<void(const RawInputEvent&)> onEvent_;
    ScreenBoundary boundary_{};

    std::atomic<uint64_t> normalizedCount_{0};
    std::atomic<uint64_t> edgeOverflowCount_{0};
    std::atomic<uint64_t> unknownEventCount_{0};
};

}  // namespace cfx