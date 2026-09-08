#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s08_pairing/i_registration_manager.hpp"

namespace cfx {

class RegistrationManager : public IRegistrationManager {
public:
    RegistrationManager() = default;

    std::optional<ErrorCode> registerPeer(const NodeIdentity& peerIdentity) noexcept override;
    std::optional<ErrorCode> unregisterPeer(const NodeId& peerNodeId) noexcept override;
    std::optional<RegistrationRecord> getRegistration(const NodeId& peerNodeId) const noexcept override;
    bool isRegistered(const NodeId& peerNodeId) const noexcept override;
    std::optional<ErrorCode> handleRegistrationRequest(const RegistrationRequest& request,
                                                        RegistrationResponse& outResponse) noexcept override;

private:
    std::vector<RegistrationRecord> records_;

    RegistrationRecord* find(const NodeId& nid) noexcept;
    const RegistrationRecord* find(const NodeId& nid) const noexcept;
};

}  // namespace cfx