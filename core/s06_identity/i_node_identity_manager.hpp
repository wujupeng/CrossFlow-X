#pragma once

#include <optional>
#include <string>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

class INodeIdentityManager {
public:
    virtual ~INodeIdentityManager() = default;

    virtual std::optional<NodeIdentity> getIdentity() const noexcept = 0;
    virtual NodeId getNodeId() const noexcept = 0;
    virtual SessionEpoch currentEpoch() const noexcept = 0;

    virtual std::optional<ErrorCode> initialize(NodeIdentity identity) noexcept = 0;
    virtual std::optional<ErrorCode> updatePublicKey(const PublicKey& newKey) noexcept = 0;
    virtual std::optional<ErrorCode> updateCapabilities(const Capabilities& caps) noexcept = 0;
    virtual std::optional<ErrorCode> addEndpointAddress(const EndpointAddress& addr) noexcept = 0;
    virtual std::optional<ErrorCode> removeEndpointAddress(const EndpointAddress& addr) noexcept = 0;
    virtual std::optional<ErrorCode> joinTopology(const TopologyMembership& membership) noexcept = 0;
    virtual std::optional<ErrorCode> leaveTopology() noexcept = 0;

    virtual SessionEpoch incrementEpoch() noexcept = 0;

    virtual std::optional<ErrorCode> persist() const noexcept = 0;
    virtual std::optional<ErrorCode> load() noexcept = 0;
    virtual std::optional<ErrorCode> recoverNodeId() noexcept = 0;
};

}  // namespace cfx