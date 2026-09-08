#include "s06_identity/identity_recovery_manager.hpp"

namespace cfx {

IdentityRecoveryManager::IdentityRecoveryManager(INodeIdentityManager& identityMgr,
                                                  ITrustedNodeList& trustedList)
    : identityMgr_(identityMgr)
    , trustedList_(trustedList) {}

IdentityRecoveryManager::RecoveryState* IdentityRecoveryManager::findRecovery(const NodeId& nid) noexcept {
    for (auto& r : activeRecoveries_) {
        if (r.peerNodeId == nid) return &r;
    }
    return nullptr;
}

std::optional<ErrorCode> IdentityRecoveryManager::initiateRecovery(const NodeId& peerNodeId) noexcept {
    if (!trustedList_.isTrusted(peerNodeId)) return ErrorCode::RecoveryUntrusted;
    if (findRecovery(peerNodeId)) return std::nullopt;
    RecoveryState state;
    state.peerNodeId = peerNodeId;
    state.initiatedAt = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    activeRecoveries_.push_back(state);
    return std::nullopt;
}

std::optional<NodeIdentity> IdentityRecoveryManager::handleRecoveryResponse(const IdentityRecoveryResponse& response) noexcept {
    if (!response.recovered) {
        cancelRecovery(response.responderNodeId);
        return std::nullopt;
    }
    auto* recovery = findRecovery(response.responderNodeId);
    if (!recovery) return std::nullopt;
    if (response.identity) {
        activeRecoveries_.erase(std::remove_if(activeRecoveries_.begin(), activeRecoveries_.end(),
            [&](const RecoveryState& r) { return r.peerNodeId == response.responderNodeId; }),
            activeRecoveries_.end());
        return *response.identity;
    }
    return std::nullopt;
}

std::optional<ErrorCode> IdentityRecoveryManager::handleRecoveryRequest(const IdentityRecoveryRequest& request,
                                                                        IdentityRecoveryResponse& outResponse) noexcept {
    auto identity = identityMgr_.getIdentity();
    if (!identity) return ErrorCode::SessUuidGenFail;
    if (!trustedList_.isTrusted(request.nodeId)) return ErrorCode::RecoveryUntrusted;
    outResponse.responderNodeId = identity->nodeId;
    outResponse.recovered = true;
    outResponse.identity = *identity;
    outResponse.fence = request.fence;
    return std::nullopt;
}

bool IdentityRecoveryManager::isRecoveryInProgress(const NodeId& peerNodeId) const noexcept {
    for (const auto& r : activeRecoveries_) {
        if (r.peerNodeId == peerNodeId) return true;
    }
    return false;
}

void IdentityRecoveryManager::cancelRecovery(const NodeId& peerNodeId) noexcept {
    activeRecoveries_.erase(std::remove_if(activeRecoveries_.begin(), activeRecoveries_.end(),
        [&](const RecoveryState& r) { return r.peerNodeId == peerNodeId; }),
        activeRecoveries_.end());
}

}  // namespace cfx