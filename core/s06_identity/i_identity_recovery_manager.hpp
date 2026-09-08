#pragma once

#include <optional>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

class IIdentityRecoveryManager {
public:
    virtual ~IIdentityRecoveryManager() = default;

    virtual std::optional<ErrorCode> initiateRecovery(const NodeId& peerNodeId) noexcept = 0;
    virtual std::optional<NodeIdentity> handleRecoveryResponse(const IdentityRecoveryResponse& response) noexcept = 0;
    virtual std::optional<ErrorCode> handleRecoveryRequest(const IdentityRecoveryRequest& request,
                                                            IdentityRecoveryResponse& outResponse) noexcept = 0;

    virtual bool isRecoveryInProgress(const NodeId& peerNodeId) const noexcept = 0;
    virtual void cancelRecovery(const NodeId& peerNodeId) noexcept = 0;
};

}  // namespace cfx