#include "s09_membership/topology_version_manager.hpp"

namespace cfx {

TopologyVersion TopologyVersionManager::currentVersion() const noexcept { return current_; }
TopologyCommitState TopologyVersionManager::commitState() const noexcept { return state_; }

std::optional<ErrorCode> TopologyVersionManager::proposeChange(const TopologyChangeProposal& proposal) noexcept {
    if (isCommitLocked()) return ErrorCode::TopoCommitFail;
    if (!coordinatorId_ || !localNodeId_ || *coordinatorId_ != *localNodeId_)
        return ErrorCode::TopoCommitFail;
    if (state_ != TopologyCommitState::Active) return ErrorCode::TopoValidationFail;
    pendingProposal_ = proposal;
    state_ = TopologyCommitState::Proposed;
    return std::nullopt;
}

std::optional<ErrorCode> TopologyVersionManager::validateProposal() noexcept {
    if (state_ != TopologyCommitState::Proposed || !pendingProposal_) return ErrorCode::TopoValidationFail;
    state_ = TopologyCommitState::Validated;
    return std::nullopt;
}

std::optional<ErrorCode> TopologyVersionManager::prepareCommit() noexcept {
    if (state_ != TopologyCommitState::Validated) return ErrorCode::TopoValidationFail;
    state_ = TopologyCommitState::Prepared;
    return std::nullopt;
}

std::optional<ErrorCode> TopologyVersionManager::activateVersion() noexcept {
    if (state_ != TopologyCommitState::ActivationAuthorized) return ErrorCode::TopoCommitFail;
    if (pendingProposal_) {
        current_ = pendingProposal_->proposedVersion;
        current_.commitState = TopologyCommitState::Active;
        pendingProposal_.reset();
    }
    state_ = TopologyCommitState::Active;
    return std::nullopt;
}

std::optional<ErrorCode> TopologyVersionManager::abortCommit() noexcept {
    if (state_ == TopologyCommitState::Active) return std::nullopt;
    pendingProposal_.reset();
    state_ = TopologyCommitState::RolledBack;
    state_ = TopologyCommitState::Active;
    return std::nullopt;
}

bool TopologyVersionManager::isCoordinator() const noexcept {
    return coordinatorId_ && localNodeId_ && *coordinatorId_ == *localNodeId_;
}

std::optional<NodeId> TopologyVersionManager::coordinatorId() const noexcept {
    return coordinatorId_;
}

std::optional<ErrorCode> TopologyVersionManager::setCoordinator(const NodeId& authorityNodeId) noexcept {
    if (coordinatorId_ && *coordinatorId_ != authorityNodeId)
        return ErrorCode::TopoCoordDuplicateClaim;
    coordinatorId_ = authorityNodeId;
    return std::nullopt;
}

bool TopologyVersionManager::isCommitLocked() const noexcept {
    return !coordinatorId_ || (localNodeId_ && *coordinatorId_ != *localNodeId_ && state_ == TopologyCommitState::Locked);
}

}  // namespace cfx