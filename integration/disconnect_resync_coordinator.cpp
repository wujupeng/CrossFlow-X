#include "integration/disconnect_resync_coordinator.hpp"

#include <cstdio>

namespace cfx {

DisconnectResyncCoordinator::DisconnectResyncCoordinator(
    IControlPlaneChannel& controlPlane,
    const DisconnectResyncConfig& config,
    ReleaseAllPressedFn releaseAllPressedFn,
    GetPressedStateFn getPressedStateFn,
    TriggerResyncFn triggerResyncFn,
    ConfirmRecoveryFn confirmRecoveryFn) noexcept
    : controlPlane_(controlPlane)
    , config_(config)
    , releaseAllPressedFn_(std::move(releaseAllPressedFn))
    , getPressedStateFn_(std::move(getPressedStateFn))
    , triggerResyncFn_(std::move(triggerResyncFn))
    , confirmRecoveryFn_(std::move(confirmRecoveryFn)) {}

DisconnectResyncCoordinator::~DisconnectResyncCoordinator() noexcept {
    stop();
}

bool DisconnectResyncCoordinator::start() noexcept {
    if (started_.load(std::memory_order_relaxed)) {
        return true;
    }

    controlPlane_.onLinkState([this](const LinkStateEvent& event) {
        handleLinkState(event);
    });

    started_.store(true, std::memory_order_relaxed);
    return true;
}

void DisconnectResyncCoordinator::stop() noexcept {
    started_.store(false, std::memory_order_relaxed);
}

void DisconnectResyncCoordinator::handleLinkState(const LinkStateEvent& event) noexcept {
    if (!started_.load(std::memory_order_relaxed)) {
        return;
    }

    switch (event.state) {
        case LinkState::Connected:
            onConnected();
            break;
        case LinkState::Disconnected:
        case LinkState::Lost:
            onDisconnected();
            break;
        case LinkState::Connecting:
            onReconnecting();
            break;
        default:
            break;
    }
}

void DisconnectResyncCoordinator::transitionTo(DisconnectResyncState newState) noexcept {
    auto oldState = static_cast<DisconnectResyncState>(state_.exchange(static_cast<u8>(newState), std::memory_order_relaxed));
    if (oldState != newState && onStateChange_) {
        onStateChange_(newState);
    }
}

void DisconnectResyncCoordinator::onDisconnected() noexcept {
    transitionTo(DisconnectResyncState::Disconnected);
    disconnectCount_.fetch_add(1, std::memory_order_relaxed);

    if (getPressedStateFn_ && releaseAllPressedFn_) {
        auto snapshot = getPressedStateFn_();
        releaseAllPressedFn_(snapshot);
    }

    if (triggerResyncFn_) {
        triggerResyncFn_();
    }

    transitionTo(DisconnectResyncState::Reconnecting);
}

void DisconnectResyncCoordinator::onReconnecting() noexcept {
    if (state_.load(std::memory_order_relaxed) == static_cast<u8>(DisconnectResyncState::Disconnected)) {
        transitionTo(DisconnectResyncState::Reconnecting);
    }
}

void DisconnectResyncCoordinator::onConnected() noexcept {
    auto currentState = state_.load(std::memory_order_relaxed);

    if (currentState == static_cast<u8>(DisconnectResyncState::Connected)) {
        return;
    }

    transitionTo(DisconnectResyncState::Resync);
    resyncCount_.fetch_add(1, std::memory_order_relaxed);

    if (triggerResyncFn_) {
        triggerResyncFn_();
    }

    bool recoveryConfirmed = true;
    if (confirmRecoveryFn_) {
        recoveryConfirmed = confirmRecoveryFn_();
    }

    if (recoveryConfirmed) {
        transitionTo(DisconnectResyncState::Connected);
    } else {
        transitionTo(DisconnectResyncState::Recovery);
        recoveryCount_.fetch_add(1, std::memory_order_relaxed);
        fprintf(stderr, "[CFX-W-RESYNC-RECOVERY] bitmap stale, waiting for user release\n");
    }
}

}  // namespace cfx