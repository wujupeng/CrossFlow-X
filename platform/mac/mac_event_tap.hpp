#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "common/spsc_ring_buffer.hpp"
#include "mac/a11y_permission_guard.hpp"
#include "mac/authoritative_resync.hpp"
#include "mac/capture_handle.hpp"
#include "mac/dual_channel_spsc.hpp"
#include "mac/mac_event_field_extractor.hpp"
#include "mac/cg_event_normalizer.hpp"

namespace cfx {

enum class TapState : uint8_t {
    Inactive,
    Active,
    Degraded,
};

class MacEventTap : public IInputCapture {
public:
    MacEventTap() noexcept;
    ~MacEventTap() noexcept override;

    MacEventTap(const MacEventTap&) = delete;
    MacEventTap& operator=(const MacEventTap&) = delete;

    CaptureHandle start(std::function<void(const RawInputEvent&)> onEvent) override;
    void stop(CaptureHandle handle) override;
    ScreenBoundary queryScreenBoundary() override;

    TapState state() const noexcept {
        return static_cast<TapState>(state_.load(std::memory_order_acquire));
    }

    bool isDegraded() const noexcept {
        return state_.load(std::memory_order_acquire) == static_cast<uint8_t>(TapState::Degraded);
    }

    uint64_t totalCaptured() const noexcept {
        return totalCaptured_.load(std::memory_order_relaxed);
    }

    uint64_t totalDropped() const noexcept {
        return dualChannel_.droppedOldestCount();
    }

    bool isStateChannelSaturated() const noexcept {
        return dualChannel_.isStateChannelSaturated();
    }

    uint64_t stateChannelSaturatedCount() const noexcept {
        return dualChannel_.stateChannelSaturatedCount();
    }

    DualChannelSpsc& dualChannel() noexcept { return dualChannel_; }

    A11yPermissionGuard& permissionGuard() noexcept { return permissionGuard_; }

    uint64_t currentModifierFlags() const noexcept {
        return lastModifierFlags_.load(std::memory_order_acquire);
    }

    static constexpr size_t kListenEventCount = 10;

    enum class ListenEventId : uint8_t {
        MouseMoved = 0,
        LeftMouseDown = 1,
        LeftMouseUp = 2,
        RightMouseDown = 3,
        RightMouseUp = 4,
        OtherMouseDown = 5,
        OtherMouseUp = 6,
        ScrollWheel = 7,
        KeyDown = 8,
        KeyUp = 9,
    };

    static uint32_t listenEventType(size_t index) noexcept;

    static uint64_t buildListenEventMask() noexcept;

    static bool isEventMaskComplete() noexcept;

    CGEventNormalizer& normalizer() noexcept { return normalizer_; }

    AuthoritativeResync& resync() noexcept { return resync_; }

    void processEvent(const RawInputEventFlat& flat) noexcept;

private:
    static void* cgEventCallback(void* proxy, uint32_t type, void* event, void* userInfo) noexcept;

    bool installEventTap() noexcept;
    void uninstallEventTap() noexcept;
    void enterDegradedState() noexcept;

    void captureThreadLoop() noexcept;


    A11yPermissionGuard permissionGuard_;
    MacEventFieldExtractor fieldExtractor_;
    CGEventNormalizer normalizer_;
    DualChannelSpsc dualChannel_;
    AuthoritativeResync resync_;

    void* eventTap_{nullptr};
    void* runLoopSource_{nullptr};
    std::thread captureThread_;
    std::function<void(const RawInputEvent&)> onEvent_;

    std::atomic<uint8_t> state_{static_cast<uint8_t>(TapState::Inactive)};
    std::atomic<bool> captureThreadRunning_{false};
    std::atomic<uint64_t> totalCaptured_{0};
    std::atomic<uint64_t> lastModifierFlags_{0};

    CaptureHandleManager handleManager_;
};

}  // namespace cfx