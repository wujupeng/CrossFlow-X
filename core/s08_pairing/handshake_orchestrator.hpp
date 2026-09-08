#pragma once

#include <optional>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s08_pairing/i_pairing_manager.hpp"
#include "s08_pairing/i_registration_manager.hpp"
#include "s08_pairing/i_trusted_node_list.hpp"
#include "s06_identity/i_node_identity_manager.hpp"

namespace cfx {

class HandshakeOrchestrator {
public:
    HandshakeOrchestrator(IPairingManager& pairingMgr,
                          IRegistrationManager& regMgr,
                          ITrustedNodeList& trustedList,
                          INodeIdentityManager& identityMgr);

    std::optional<ErrorCode> initiateHandshake(const NodeId& peerNodeId,
                                                 const std::string& pairingCode) noexcept;

    std::optional<ErrorCode> handlePairingRequest(const PairingRequest& request,
                                                  PairingResponse& outResponse) noexcept;

    std::optional<ErrorCode> handlePairingResponse(const PairingResponse& response) noexcept;

    std::optional<ErrorCode> handleRegistrationRequest(const RegistrationRequest& request,
                                                        RegistrationResponse& outResponse) noexcept;

    std::optional<ErrorCode> handleRegistrationResponse(const RegistrationResponse& response) noexcept;

    std::optional<ErrorCode> completeHandshake(const NodeId& peerNodeId) noexcept;

    std::optional<ErrorCode> rollbackHandshake(const NodeId& peerNodeId) noexcept;

    bool isHandshakeComplete(const NodeId& peerNodeId) const noexcept;

private:
    IPairingManager& pairingMgr_;
    IRegistrationManager& regMgr_;
    ITrustedNodeList& trustedList_;
    INodeIdentityManager& identityMgr_;

    struct HandshakeState {
        NodeId peerNodeId;
        bool pairingDone{false};
        bool registrationDone{false};
    };
    std::vector<HandshakeState> activeHandshakes_;

    HandshakeState* findHandshake(const NodeId& nid) noexcept;
};

}  // namespace cfx