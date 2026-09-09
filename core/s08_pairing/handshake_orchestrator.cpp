#include "s08_pairing/handshake_orchestrator.hpp"

namespace cfx {

HandshakeOrchestrator::HandshakeOrchestrator(IPairingManager& pairingMgr,
                                              IRegistrationManager& regMgr,
                                              ITrustedNodeList& trustedList,
                                              INodeIdentityManager& identityMgr)
    : pairingMgr_(pairingMgr)
    , regMgr_(regMgr)
    , trustedList_(trustedList)
    , identityMgr_(identityMgr) {}

HandshakeOrchestrator::HandshakeState* HandshakeOrchestrator::findHandshake(const NodeId& nid) noexcept {
    for (auto& h : activeHandshakes_) {
        if (h.peerNodeId == nid) return &h;
    }
    return nullptr;
}

std::optional<ErrorCode> HandshakeOrchestrator::initiateHandshake(const NodeId& peerNodeId,
                                                                   const std::string& pairingCode) noexcept {
    auto err = pairingMgr_.initiatePairing(peerNodeId, pairingCode);
    if (err) return err;
    if (!findHandshake(peerNodeId)) {
        activeHandshakes_.push_back({peerNodeId, false, false});
    }
    return std::nullopt;
}

std::optional<ErrorCode> HandshakeOrchestrator::handlePairingRequest(const PairingRequest& request,
                                                                     PairingResponse& outResponse) noexcept {
    return pairingMgr_.handlePairingRequest(request, outResponse);
}

std::optional<ErrorCode> HandshakeOrchestrator::handlePairingResponse(const PairingResponse& response) noexcept {
    auto err = pairingMgr_.handlePairingResponse(response);
    if (err) return err;
    auto* hs = findHandshake(response.responderNodeId);
    if (hs) hs->pairingDone = true;
    if (pairingMgr_.currentState(response.responderNodeId) == PairingState::Trusted) {
        TrustedNodeEntry entry;
        entry.nodeId = response.responderNodeId;
        entry.pairedAt = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        entry.lastSeenEpoch = identityMgr_.currentEpoch();
        trustedList_.add(entry);
    }
    return std::nullopt;
}

std::optional<ErrorCode> HandshakeOrchestrator::handleRegistrationRequest(const RegistrationRequest& request,
                                                                           RegistrationResponse& outResponse) noexcept {
    return regMgr_.handleRegistrationRequest(request, outResponse);
}

std::optional<ErrorCode> HandshakeOrchestrator::handleRegistrationResponse(const RegistrationResponse& response) noexcept {
    if (!response.accepted) return rollbackHandshake(response.responderNodeId);
    auto* hs = findHandshake(response.responderNodeId);
    if (hs) hs->registrationDone = true;
    return std::nullopt;
}

std::optional<ErrorCode> HandshakeOrchestrator::completeHandshake(const NodeId& peerNodeId) noexcept {
    auto* hs = findHandshake(peerNodeId);
    if (!hs) return ErrorCode::PairUnpaired;
    if (!hs->pairingDone) return ErrorCode::PairUnpaired;
    if (!hs->registrationDone) return ErrorCode::RegUndiscovered;
    auto err = pairingMgr_.forceTransition(peerNodeId, PairingState::Registering);
    if (err) return err;
    err = pairingMgr_.forceTransition(peerNodeId, PairingState::Registered);
    if (err) return err;
    err = pairingMgr_.forceTransition(peerNodeId, PairingState::Member);
    if (err) return err;
    return std::nullopt;
}

std::optional<ErrorCode> HandshakeOrchestrator::rollbackHandshake(const NodeId& peerNodeId) noexcept {
    auto* hs = findHandshake(peerNodeId);
    if (hs) {
        hs->pairingDone = false;
        hs->registrationDone = false;
    }
    auto state = pairingMgr_.currentState(peerNodeId);
    if (state != PairingState::Discovered && state != PairingState::Rejected) {
        pairingMgr_.forceTransition(peerNodeId, PairingState::Rollback);
        pairingMgr_.forceTransition(peerNodeId, PairingState::Discovered);
    }
    trustedList_.remove(peerNodeId);
    return std::nullopt;
}

bool HandshakeOrchestrator::isHandshakeComplete(const NodeId& peerNodeId) const noexcept {
    for (const auto& h : activeHandshakes_) {
        if (h.peerNodeId == peerNodeId) {
            return h.pairingDone && h.registrationDone;
        }
    }
    return false;
}

}  // namespace cfx