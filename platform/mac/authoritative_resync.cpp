#include "mac/authoritative_resync.hpp"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace cfx {

#ifdef __APPLE__

ModifierState AuthoritativeResync::queryGroundTruthModifiers() noexcept {
    const CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState);
    ModifierState mods{};
    mods.shift = (flags & kCGEventFlagMaskShift) != 0;
    mods.ctrl  = (flags & kCGEventFlagMaskControl) != 0;
    mods.alt   = (flags & kCGEventFlagMaskAlternate) != 0;
    mods.cmd   = (flags & kCGEventFlagMaskCommand) != 0;
    mods.fn    = (flags & kCGEventFlagMaskSecondaryFn) != 0;
    return mods;
}

#else

ModifierState AuthoritativeResync::queryGroundTruthModifiers() noexcept {
    ModifierState mods{};
    return mods;
}

#endif

void AuthoritativeResync::trigger() noexcept {
    state_.store(static_cast<uint8_t>(ResyncState::Resynchronizing),
                 std::memory_order_release);
}

ModifierState AuthoritativeResync::resynchronize() noexcept {
    state_.store(static_cast<uint8_t>(ResyncState::Resynchronizing),
                 std::memory_order_release);

    const ModifierState groundTruth = queryGroundTruthModifiers();
    groundTruthModifiers_.store(groundTruth, std::memory_order_release);

    bitmapStale_.store(true, std::memory_order_release);

    resyncCount_.fetch_add(1, std::memory_order_relaxed);

    state_.store(static_cast<uint8_t>(ResyncState::RecoveryPending),
                 std::memory_order_release);

    return groundTruth;
}

void AuthoritativeResync::markBitmapStale(PressedStateSnapshot& snapshot) noexcept {
    snapshot.stale = true;
    bitmapStale_.store(true, std::memory_order_release);
}

bool AuthoritativeResync::confirmRecovery() noexcept {
    const uint8_t current = state_.load(std::memory_order_acquire);
    if (current != static_cast<uint8_t>(ResyncState::RecoveryPending)) {
        return false;
    }

    bitmapStale_.store(false, std::memory_order_release);
    state_.store(static_cast<uint8_t>(ResyncState::Recovered),
                 std::memory_order_release);
    return true;
}

void AuthoritativeResync::reset() noexcept {
    state_.store(static_cast<uint8_t>(ResyncState::Idle),
                 std::memory_order_release);
    bitmapStale_.store(false, std::memory_order_release);
}

}  // namespace cfx