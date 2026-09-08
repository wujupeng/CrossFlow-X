#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s06_identity/i_identity_recovery_manager.hpp"
#include "s06_identity/i_node_identity_manager.hpp"
#include "s08_pairing/i_trusted_node_list.hpp"

namespace cfx {

class IdentityRecoveryManager : public IIdentityRecoveryManager {
public:
    IdentityRecoveryManager(INodeIdentityManager& identityMgr,
                            ITrustedNodeList& trustedList);

    std::optional<ErrorCode> initiateRecovery(const NodeId& peerNodeId) noexcept override;
    std::optional<NodeIdentity> handleRecoveryResponse(const IdentityRecoveryResponse& response) noexcept override;
    std::optional<ErrorCode> handleRecoveryRequest(const IdentityRecoveryRequest& request,
                                                    IdentityRecoveryResponse& outResponse) noexcept override;

    bool isRecoveryInProgress(const NodeId& peerNodeId) const noexcept override;
    void cancelRecovery(const NodeId& peerNodeId) noexcept override;

private:
    INodeIdentityManager& identityMgr_;
    ITrustedNodeList& trustedList_;

    struct RecoveryState {
        NodeId peerNodeId;
        u64 initiatedAt{0};
    };
    std::vector<RecoveryState> activeRecoveries_;

    RecoveryState* findRecovery(const NodeId& nid) noexcept;
};

}  // namespace cfx