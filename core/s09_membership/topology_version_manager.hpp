#pragma once

#include <optional>
#include <string>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "s09_membership/i_topology_version_manager.hpp"

namespace cfx {

class TopologyVersionManager : public ITopologyVersionManager {
public:
    TopologyVersionManager() = default;

    TopologyVersion currentVersion() const noexcept override;
    TopologyCommitState commitState() const noexcept override;

    std::optional<ErrorCode> proposeChange(const TopologyChangeProposal& proposal) noexcept override;
    std::optional<ErrorCode> validateProposal() noexcept override;
    std::optional<ErrorCode> prepareCommit() noexcept override;
    std::optional<ErrorCode> activateVersion() noexcept override;
    std::optional<ErrorCode> abortCommit() noexcept override;

    bool isCoordinator() const noexcept override;
    std::optional<NodeId> coordinatorId() const noexcept override;
    std::optional<ErrorCode> setCoordinator(const NodeId& authorityNodeId) noexcept override;
    bool isCommitLocked() const noexcept override;

    std::optional<ErrorCode> setLocalNodeId(const NodeId& nodeId) noexcept;
    std::optional<ErrorCode> setTopologyAuthority(const NodeId& authorityNodeId) noexcept;
    std::optional<ErrorCode> authorizeActivation() noexcept;
    std::optional<ErrorCode> markCoordinatorOffline() noexcept;
    std::optional<ErrorCode> markCoordinatorOnline() noexcept;
    bool isAuthorityConfigured() const noexcept;
    std::optional<NodeId> authorityNodeId() const noexcept;

private:
    TopologyVersion current_{};
    TopologyCommitState state_{TopologyCommitState::Active};
    std::optional<TopologyChangeProposal> pendingProposal_;
    std::optional<NodeId> coordinatorId_;
    std::optional<NodeId> localNodeId_;
    std::optional<NodeId> authorityNodeId_;
    bool coordinatorOnline_{true};
};

}  // namespace cfx
