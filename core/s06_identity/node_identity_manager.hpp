#pragma once

#include <optional>
#include <string>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "s06_identity/i_node_identity_manager.hpp"

namespace cfx {

class NodeIdentityManager : public INodeIdentityManager {
public:
    NodeIdentityManager() = default;
    explicit NodeIdentityManager(std::string persistPath);

    std::optional<NodeIdentity> getIdentity() const noexcept override;
    NodeId getNodeId() const noexcept override;
    SessionEpoch currentEpoch() const noexcept override;

    std::optional<ErrorCode> initialize(NodeIdentity identity) noexcept override;
    std::optional<ErrorCode> updatePublicKey(const PublicKey& newKey) noexcept override;
    std::optional<ErrorCode> updateCapabilities(const Capabilities& caps) noexcept override;
    std::optional<ErrorCode> addEndpointAddress(const EndpointAddress& addr) noexcept override;
    std::optional<ErrorCode> removeEndpointAddress(const EndpointAddress& addr) noexcept override;
    std::optional<ErrorCode> joinTopology(const TopologyMembership& membership) noexcept override;
    std::optional<ErrorCode> leaveTopology() noexcept override;

    SessionEpoch incrementEpoch() noexcept override;

    std::optional<ErrorCode> persist() const noexcept override;
    std::optional<ErrorCode> load() noexcept override;
    std::optional<ErrorCode> recoverNodeId() noexcept override;

private:
    std::optional<NodeIdentity> identity_;
    std::string persistPath_;
};

}  // namespace cfx