#pragma once

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s08_pairing/i_pairing_manager.hpp"

namespace cfx {

class PairingFsm : public IPairingManager {
public:
    PairingFsm() = default;

    PairingState currentState(const NodeId& peerNodeId) const noexcept override;
    std::optional<ErrorCode> initiatePairing(const NodeId& peerNodeId,
                                              const std::string& pairingCode) noexcept override;
    std::optional<ErrorCode> handlePairingRequest(const PairingRequest& request,
                                                  PairingResponse& outResponse) noexcept override;
    std::optional<ErrorCode> handlePairingResponse(const PairingResponse& response) noexcept override;
    bool isTransitionValid(PairingState from, PairingState to) const noexcept override;
    std::optional<ErrorCode> forceTransition(const NodeId& peerNodeId, PairingState newState) noexcept override;

private:
    struct PeerState {
        NodeId nodeId;
        PairingState state{PairingState::Discovered};
        std::string pairingCode;
    };
    std::vector<PeerState> peers_;

    PeerState* findPeer(const NodeId& nid) noexcept;
    const PeerState* findPeer(const NodeId& nid) const noexcept;
};

}  // namespace cfx