#pragma once

#include <optional>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"

namespace cfx {

class IRegistrationManager {
public:
    virtual ~IRegistrationManager() = default;

    virtual std::optional<ErrorCode> registerPeer(const NodeIdentity& peerIdentity) noexcept = 0;
    virtual std::optional<ErrorCode> unregisterPeer(const NodeId& peerNodeId) noexcept = 0;
    virtual std::optional<RegistrationRecord> getRegistration(const NodeId& peerNodeId) const noexcept = 0;
    virtual bool isRegistered(const NodeId& peerNodeId) const noexcept = 0;

    virtual std::optional<ErrorCode> handleRegistrationRequest(const RegistrationRequest& request,
                                                                RegistrationResponse& outResponse) noexcept = 0;
};

}  // namespace cfx