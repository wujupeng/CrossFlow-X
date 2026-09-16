#pragma once

#include <atomic>
#include <cstdint>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"

namespace cfx {

enum class ResyncState : uint8_t {
    Idle,
    Resynchronizing,
    RecoveryPending,
    Recovered,
};

class AuthoritativeResync {
public:
    AuthoritativeResync() noexcept = default;
    ~AuthoritativeResync() noexcept = default;

    AuthoritativeResync(const AuthoritativeResync&) = delete;
    AuthoritativeResync& operator=(const AuthoritativeResync&) = delete;

    ResyncState state() const noexcept {
        return static_cast<ResyncState>(state_.load(std::memory_order_acquire));
    }

    bool isResynchronizing() const noexcept {
        return state_.load(std::memory_order_acquire) ==
               static_cast<uint8_t>(ResyncState::Resynchronizing);
    }

    void trigger() noexcept;

    ModifierState resynchronize() noexcept;

    void markBitmapStale(PressedStateSnapshot& snapshot) noexcept;

    bool confirmRecovery() noexcept;

    void reset() noexcept;

    ModifierState groundTruthModifiers() const noexcept {
        return groundTruthModifiers_.load(std::memory_order_acquire);
    }

    bool bitmapStale() const noexcept {
        return bitmapStale_.load(std::memory_order_acquire);
    }

    uint64_t resyncCount() const noexcept {
        return resyncCount_.load(std::memory_order_relaxed);
    }

private:
    static ModifierState queryGroundTruthModifiers() noexcept;

    std::atomic<uint8_t> state_{static_cast<uint8_t>(ResyncState::Idle)};
    std::atomic<ModifierState> groundTruthModifiers_{};
    std::atomic<bool> bitmapStale_{false};
    std::atomic<uint64_t> resyncCount_{0};
};

}  // namespace cfx