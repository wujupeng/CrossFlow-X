#pragma once

#include <optional>
#include <string>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"

namespace cfx {

class IPairingManager {
public:
    virtual ~IPairingManager() = default;

    virtual PairingState currentState(const NodeId& peerNodeId) const noexcept = 0;
    virtual std::optional<ErrorCode> initiatePairing(const NodeId& peerNodeId,
                                                      const std::string& pairingCode) noexcept = 0;
    virtual std::optional<ErrorCode> handlePairingRequest(const PairingRequest& request,
                                                          PairingResponse& outResponse) noexcept = 0;
    virtual std::optional<ErrorCode> handlePairingResponse(const PairingResponse& response) noexcept = 0;

    virtual bool isTransitionValid(PairingState from, PairingState to) const noexcept = 0;
    virtual std::optional<ErrorCode> forceTransition(const NodeId& peerNodeId, PairingState newState) noexcept = 0;
};

}  // namespace cfx