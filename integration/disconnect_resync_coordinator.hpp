#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

#include "common/domain.hpp"
#include "common/messages.hpp"
#include "common/platform_ports.hpp"
#include "s05_transport/i_control_plane_channel.hpp"

namespace cfx {

enum class DisconnectResyncState : u8 {
    Connected = 0,
    Disconnected = 1,
    Reconnecting = 2,
    Resync = 3,
    Recovery = 4,
};

struct DisconnectResyncConfig {
    u32 releaseAllPressedDeadlineMs{100};
    u32 resyncVerificationTimeoutMs{5000};
};

using ReleaseAllPressedFn = std::function<ReleaseResult(const PressedStateSnapshot&)>;
using GetPressedStateFn = std::function<PressedStateSnapshot()>;
using TriggerResyncFn = std::function<void()>;
using ConfirmRecoveryFn = std::function<bool()>;
using OnStateChangeFn = std::function<void(DisconnectResyncState)>;

class DisconnectResyncCoordinator {
public:
    DisconnectResyncCoordinator(IControlPlaneChannel& controlPlane,
                                const DisconnectResyncConfig& config,
                                ReleaseAllPressedFn releaseAllPressedFn,
                                GetPressedStateFn getPressedStateFn,
                                TriggerResyncFn triggerResyncFn,
                                ConfirmRecoveryFn confirmRecoveryFn) noexcept;
    ~DisconnectResyncCoordinator() noexcept;

    DisconnectResyncCoordinator(const DisconnectResyncCoordinator&) = delete;
    DisconnectResyncCoordinator& operator=(const DisconnectResyncCoordinator&) = delete;

    bool start() noexcept;
    void stop() noexcept;

    DisconnectResyncState state() const noexcept {
        return static_cast<DisconnectResyncState>(state_.load(std::memory_order_relaxed));
    }
    bool isInputAllowed() const noexcept {
        return state_.load(std::memory_order_relaxed) == static_cast<u8>(DisconnectResyncState::Connected);
    }
    u32 disconnectCount() const noexcept { return disconnectCount_.load(std::memory_order_relaxed); }
    u32 resyncCount() const noexcept { return resyncCount_.load(std::memory_order_relaxed); }
    u32 recoveryCount() const noexcept { return recoveryCount_.load(std::memory_order_relaxed); }

    void setOnStateChange(OnStateChangeFn handler) noexcept { onStateChange_ = std::move(handler); }

private:
    void handleLinkState(const LinkStateEvent& event) noexcept;
    void transitionTo(DisconnectResyncState newState) noexcept;
    void onDisconnected() noexcept;
    void onReconnecting() noexcept;
    void onConnected() noexcept;

    IControlPlaneChannel& controlPlane_;
    DisconnectResyncConfig config_;

    ReleaseAllPressedFn releaseAllPressedFn_;
    GetPressedStateFn getPressedStateFn_;
    TriggerResyncFn triggerResyncFn_;
    ConfirmRecoveryFn confirmRecoveryFn_;
    OnStateChangeFn onStateChange_;

    std::atomic<u8> state_{static_cast<u8>(DisconnectResyncState::Connected)};
    std::atomic<bool> started_{false};
    std::atomic<u32> disconnectCount_{0};
    std::atomic<u32> resyncCount_{0};
    std::atomic<u32> recoveryCount_{0};
};

}  // namespace cfx