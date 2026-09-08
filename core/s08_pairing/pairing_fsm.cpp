#include "s08_pairing/pairing_fsm.hpp"

namespace cfx {

PairingFsm::PeerState* PairingFsm::findPeer(const NodeId& nid) noexcept {
    for (auto& p : peers_) if (p.nodeId == nid) return &p;
    return nullptr;
}

const PairingFsm::PeerState* PairingFsm::findPeer(const NodeId& nid) const noexcept {
    for (auto& p : peers_) if (p.nodeId == nid) return &p;
    return nullptr;
}

PairingState PairingFsm::currentState(const NodeId& peerNodeId) const noexcept {
    auto* p = findPeer(peerNodeId);
    return p ? p->state : PairingState::Discovered;
}

bool PairingFsm::isTransitionValid(PairingState from, PairingState to) const noexcept {
    if (from == to) return true;
    switch (from) {
        case PairingState::Discovered:
            return to == PairingState::Untrusted || to == PairingState::Rejected;
        case PairingState::Untrusted:
            return to == PairingState::Pairing || to == PairingState::Rejected;
        case PairingState::Pairing:
            return to == PairingState::Trusted || to == PairingState::Rejected || to == PairingState::Rollback;
        case PairingState::Trusted:
            return to == PairingState::Registering || to == PairingState::Rollback;
        case PairingState::Registering:
            return to == PairingState::Registered || to == PairingState::Rollback;
        case PairingState::Registered:
            return to == PairingState::Member || to == PairingState::Rollback;
        case PairingState::Member:
            return to == PairingState::Rollback;
        case PairingState::Rejected:
            return to == PairingState::Discovered;
        case PairingState::Rollback:
            return to == PairingState::Discovered || to == PairingState::Untrusted;
    }
    return false;
}

std::optional<ErrorCode> PairingFsm::initiatePairing(const NodeId& peerNodeId,
                                                      const std::string& pairingCode) noexcept {
    if (pairingCode.empty()) return ErrorCode::PairCodeMismatch;
    auto* peer = findPeer(peerNodeId);
    if (!peer) {
        peers_.push_back({peerNodeId, PairingState::Discovered, ""});
        peer = &peers_.back();
    }
    if (peer->state == PairingState::Discovered) {
        peer->state = PairingState::Untrusted;
    }
    if (!isTransitionValid(peer->state, PairingState::Pairing)) return ErrorCode::PairIllegalTrans;
    peer->state = PairingState::Pairing;
    peer->pairingCode = pairingCode;
    return std::nullopt;
}

std::optional<ErrorCode> PairingFsm::handlePairingRequest(const PairingRequest& request,
                                                          PairingResponse& outResponse) noexcept {
    if (request.pairingCode.empty()) return ErrorCode::PairCodeMismatch;
    auto* peer = findPeer(request.requesterNodeId);
    if (!peer) {
        peers_.push_back({request.requesterNodeId, PairingState::Untrusted, ""});
        peer = &peers_.back();
    }
    peer->state = PairingState::Trusted;
    outResponse.responderNodeId = request.requesterNodeId;
    outResponse.accepted = true;
    outResponse.identity = std::nullopt;
    outResponse.fence = request.fence;
    return std::nullopt;
}

std::optional<ErrorCode> PairingFsm::handlePairingResponse(const PairingResponse& response) noexcept {
    auto* peer = findPeer(response.responderNodeId);
    if (!peer) return ErrorCode::PairUnpaired;
    if (!response.accepted) {
        peer->state = PairingState::Rejected;
        return std::nullopt;
    }
    if (!isTransitionValid(peer->state, PairingState::Trusted)) return ErrorCode::PairIllegalTrans;
    peer->state = PairingState::Trusted;
    return std::nullopt;
}

std::optional<ErrorCode> PairingFsm::forceTransition(const NodeId& peerNodeId, PairingState newState) noexcept {
    auto* peer = findPeer(peerNodeId);
    if (!peer) return ErrorCode::PairUnpaired;
    if (!isTransitionValid(peer->state, newState)) return ErrorCode::PairIllegalTrans;
    peer->state = newState;
    return std::nullopt;
}

}  // namespace cfx