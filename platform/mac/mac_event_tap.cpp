#include "mac/mac_event_tap.hpp"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <chrono>

namespace cfx {

MacEventTap::MacEventTap() noexcept
    : permissionGuard_{},
      fieldExtractor_{},
      spscQueue_{},
      eventTap_{nullptr},
      runLoopSource_{nullptr},
      onEvent_{} {}

MacEventTap::~MacEventTap() noexcept {
    CaptureHandle dummy{};
    dummy.id = 0;
    dummy.active = false;
    stop(dummy);
}

#ifdef __APPLE__

static const CGEventType kListenEvents[] = {
    kCGEventMouseMoved,
    kCGEventLeftMouseDown,
    kCGEventLeftMouseUp,
    kCGEventRightMouseDown,
    kCGEventRightMouseUp,
    kCGEventOtherMouseDown,
    kCGEventOtherMouseUp,
    kCGEventScrollWheel,
    kCGEventKeyDown,
    kCGEventKeyUp,
};

#endif

uint32_t MacEventTap::listenEventType(size_t index) noexcept {
#ifdef __APPLE__
    if (index < kListenEventCount) {
        return static_cast<uint32_t>(kListenEvents[index]);
    }
#else
    static const uint32_t types[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    if (index < kListenEventCount) {
        return types[index];
    }
#endif
    return 0xFFFFFFFF;
}

uint64_t MacEventTap::buildListenEventMask() noexcept {
    uint64_t mask = 0;
    for (size_t i = 0; i < kListenEventCount; ++i) {
        mask |= (1ULL << listenEventType(i));
    }
    return mask;
}

bool MacEventTap::isEventMaskComplete() noexcept {
    const uint64_t mask = buildListenEventMask();
    for (size_t i = 0; i < kListenEventCount; ++i) {
        uint64_t bit;
        bit = (1ULL << listenEventType(i));
        if ((mask & bit) == 0) {
            return false;
        }
    }
    return true;
}

#ifdef __APPLE__

void* MacEventTap::cgEventCallback(void* /*proxy*/, uint32_t /*type*/, void* event, void* userInfo) noexcept {
    MacEventTap* self = static_cast<MacEventTap*>(userInfo);
    if (!self || !event) {
        return event;
    }

    // Step 1: MacEventFieldExtractor — field extraction (≤100us, no heap alloc)
    const RawInputEventFlat flat = self->fieldExtractor_.extract(event);
    if (!flat.valid) {
        return event;
    }

    // Step 2: Modifier atomic update (std::atomic store, ≤200ns)
    self->lastModifierFlags_.store(flat.modifierFlags, std::memory_order_release);

    // Step 3: RawInputEvent enqueue (lock-free SPSC, no heap alloc)
    (void)self->spscQueue_.tryPushDropOldest(flat);
    self->totalCaptured_.fetch_add(1, std::memory_order_relaxed);

    // Callback MUST NOT: call onEvent / call FSM / perform edge detection /
    //                     perform handoff / perform downstream dispatch / block
    return event;
}

bool MacEventTap::installEventTap() noexcept {
    const CGEventMask eventMask = static_cast<CGEventMask>(buildListenEventMask());

    CFMachPortRef tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionListenOnly,
        eventMask,
        [](CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* userInfo) -> CGEventRef {
            return static_cast<CGEventRef>(MacEventTap::cgEventCallback(proxy, type, event, userInfo));
        },
        this);

    if (!tap) {
        return false;
    }

    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    if (!src) {
        CFRelease(tap);
        return false;
    }

    CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopDefaultMode);
    CGEventTapEnable(tap, true);

    eventTap_ = tap;
    runLoopSource_ = src;
    return true;
}

void MacEventTap::uninstallEventTap() noexcept {
    if (runLoopSource_) {
        CFRunLoopSourceRef src = static_cast<CFRunLoopSourceRef>(runLoopSource_);
        CFRunLoopRemoveSource(CFRunLoopGetMain(), src, kCFRunLoopDefaultMode);
        CFRelease(src);
        runLoopSource_ = nullptr;
    }
    if (eventTap_) {
        CFRelease(static_cast<CFMachPortRef>(eventTap_));
        eventTap_ = nullptr;
    }
}

#else

void* MacEventTap::cgEventCallback(void*, uint32_t, void* event, void*) noexcept {
    return event;
}

bool MacEventTap::installEventTap() noexcept {
    return false;
}

void MacEventTap::uninstallEventTap() noexcept {}

#endif

void MacEventTap::enterDegradedState() noexcept {
    state_.store(static_cast<uint8_t>(TapState::Degraded), std::memory_order_release);
}

void MacEventTap::captureThreadLoop() noexcept {
    while (captureThreadRunning_.load(std::memory_order_acquire)) {
        RawInputEventFlat flat{};
        if (spscQueue_.tryPop(flat)) {
            processEvent(flat);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    RawInputEventFlat flat{};
    while (spscQueue_.tryPop(flat)) {
        processEvent(flat);
    }
}

void MacEventTap::processEvent(const RawInputEventFlat& flat) noexcept {
    if (!flat.valid) {
        return;
    }
    // Capture thread → CGEventNormalizer::normalize() → edge detection → onEvent
    // (TASK-015: normalizer runs in Capture thread, NOT in callback)
    normalizer_.normalize(flat);
}

CaptureHandle MacEventTap::start(std::function<void(const RawInputEvent&)> onEvent) {
    if (state_.load(std::memory_order_acquire) != static_cast<uint8_t>(TapState::Inactive)) {
        CaptureHandle h{};
        h.id = 0;
        h.active = false;
        return h;
    }

    if (!permissionGuard_.ensurePermissions()) {
        enterDegradedState();
        CaptureHandle h{};
        h.id = 0;
        h.active = false;
        return h;
    }

    onEvent_ = std::move(onEvent);
    normalizer_.setOnEvent(onEvent_);
    normalizer_.setScreenBoundary(queryScreenBoundary());

    if (!installEventTap()) {
        enterDegradedState();
        CaptureHandle h{};
        h.id = 0;
        h.active = false;
        return h;
    }

    captureThreadRunning_.store(true, std::memory_order_release);
    captureThread_ = std::thread([this]() { captureThreadLoop(); });

    state_.store(static_cast<uint8_t>(TapState::Active), std::memory_order_release);

    return handleManager_.acquire();
}

void MacEventTap::stop(CaptureHandle handle) {
    if (handle.id != 0 && handle.active) {
        handleManager_.release(handle);
    }

    if (state_.load(std::memory_order_acquire) == static_cast<uint8_t>(TapState::Inactive)) {
        return;
    }

    captureThreadRunning_.store(false, std::memory_order_release);
    if (captureThread_.joinable()) {
        captureThread_.join();
    }

    uninstallEventTap();
    onEvent_ = {};
    state_.store(static_cast<uint8_t>(TapState::Inactive), std::memory_order_release);
}

ScreenBoundary MacEventTap::queryScreenBoundary() {
    ScreenBoundary boundary{};

#ifdef __APPLE__
    const CGDirectDisplayID displayId = CGMainDisplayID();
    boundary.width = static_cast<u32>(CGDisplayPixelsWide(displayId));
    boundary.height = static_cast<u32>(CGDisplayPixelsHigh(displayId));
    const CGRect bounds = CGDisplayBounds(displayId);
    boundary.originX = static_cast<u32>(bounds.origin.x);
    boundary.originY = static_cast<u32>(bounds.origin.y);
#endif

    return boundary;
}

}  // namespace cfx