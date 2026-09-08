#include "s08_pairing/registration_manager.hpp"

namespace cfx {

RegistrationRecord* RegistrationManager::find(const NodeId& nid) noexcept {
    for (auto& r : records_) {
        if (r.peerNodeId == nid) return &r;
    }
    return nullptr;
}

const RegistrationRecord* RegistrationManager::find(const NodeId& nid) const noexcept {
    for (auto& r : records_) {
        if (r.peerNodeId == nid) return &r;
    }
    return nullptr;
}

std::optional<ErrorCode> RegistrationManager::registerPeer(const NodeIdentity& peerIdentity) noexcept {
    if (!peerIdentity.isValid()) return ErrorCode::SessNodeidForge;
    auto* existing = find(peerIdentity.nodeId);
    if (existing) {
        existing->peerIdentity = peerIdentity;
        existing->previousState = existing->currentState;
        existing->currentState = PairingState::Registered;
        existing->enteredAt = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        existing->failureReason.reset();
        return std::nullopt;
    }
    RegistrationRecord record;
    record.peerNodeId = peerIdentity.nodeId;
    record.peerIdentity = peerIdentity;
    record.currentState = PairingState::Registered;
    record.previousState = PairingState::Trusted;
    record.enteredAt = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    records_.push_back(record);
    return std::nullopt;
}

std::optional<ErrorCode> RegistrationManager::unregisterPeer(const NodeId& peerNodeId) noexcept {
    records_.erase(std::remove_if(records_.begin(), records_.end(),
        [&](const RegistrationRecord& r) { return r.peerNodeId == peerNodeId; }),
        records_.end());
    return std::nullopt;
}

std::optional<RegistrationRecord> RegistrationManager::getRegistration(const NodeId& peerNodeId) const noexcept {
    auto* r = find(peerNodeId);
    if (!r) return std::nullopt;
    return *r;
}

bool RegistrationManager::isRegistered(const NodeId& peerNodeId) const noexcept {
    auto* r = find(peerNodeId);
    return r && r->currentState == PairingState::Registered;
}

std::optional<ErrorCode> RegistrationManager::handleRegistrationRequest(const RegistrationRequest& request,
                                                                        RegistrationResponse& outResponse) noexcept {
    if (!request.identity.isValid()) return ErrorCode::SessNodeidForge;
    auto err = registerPeer(request.identity);
    if (err) return err;
    outResponse.responderNodeId = request.identity.nodeId;
    outResponse.accepted = true;
    outResponse.membership = std::nullopt;
    outResponse.fence = request.fence;
    return std::nullopt;
}

}  // namespace cfx