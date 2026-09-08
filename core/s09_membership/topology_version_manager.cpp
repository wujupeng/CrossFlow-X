#include "s09_membership/topology_version_manager.hpp"

namespace cfx {

TopologyVersion TopologyVersionManager::currentVersion() const noexcept { return current_; }
TopologyCommitState TopologyVersionManager::commitState() const noexcept { return state_; }

std::optional<ErrorCode> TopologyVersionManager::setLocalNodeId(const NodeId& nodeId) noexcept {
    localNodeId_ = nodeId;
    return std::nullopt;
}

std::optional<ErrorCode> TopologyVersionManager::setTopologyAuthority(const NodeId& authorityNodeId) noexcept {
    if (authorityNodeId.isNull()) return ErrorCode::SessNodeidForge;
    if (authorityNodeId_ && *authorityNodeId_ != authorityNodeId)
        return ErrorCode::TopoCoordDuplicateClaim;
    authorityNodeId_ = authorityNodeId;
    coordinatorId_ = authorityNodeId;
    return std::nullopt;
}

std::optional<ErrorCode> TopologyVersionManager::setCoordinator(const NodeId& authorityNodeId) noexcept {
    if (!authorityNodeId_) return ErrorCode::TopoCoordDuplicateClaim;
    if (*authorityNodeId_ != authorityNodeId) return ErrorCode::TopoCoordDuplicateClaim;
    coordinatorId_ = authorityNodeId;
    return std::nullopt;
}

std::optional<ErrorCode> TopologyVersionManager::proposeChange(const TopologyChangeProposal& proposal) noexcept {
    if (isCommitLocked()) return ErrorCode::TopoCommitFail;
    if (!isCoordinator()) return ErrorCode::TopoCommitFail;
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

std::optional<ErrorCode> TopologyVersionManager::authorizeActivation() noexcept {
    if (state_ != TopologyCommitState::Prepared) return ErrorCode::TopoCommitFail;
    if (!isCoordinator()) return ErrorCode::TopoCommitFail;
    state_ = TopologyCommitState::ActivationAuthorized;
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
    state_ = TopologyCommitState::Active;
    return std::nullopt;
}

std::optional<ErrorCode> TopologyVersionManager::markCoordinatorOffline() noexcept {
    coordinatorOnline_ = false;
    if (state_ != TopologyCommitState::Active) {
        pendingProposal_.reset();
        state_ = TopologyCommitState::Locked;
    }
    return std::nullopt;
}

std::optional<ErrorCode> TopologyVersionManager::markCoordinatorOnline() noexcept {
    coordinatorOnline_ = true;
    if (state_ == TopologyCommitState::Locked) {
        state_ = TopologyCommitState::Active;
    }
    return std::nullopt;
}

bool TopologyVersionManager::isCoordinator() const noexcept {
    if (!coordinatorOnline_) return false;
    return authorityNodeId_ && localNodeId_ && *authorityNodeId_ == *localNodeId_;
}

std::optional<NodeId> TopologyVersionManager::coordinatorId() const noexcept {
    if (!coordinatorOnline_) return std::nullopt;
    return coordinatorId_;
}

bool TopologyVersionManager::isCommitLocked() const noexcept {
    if (!authorityNodeId_) return true;
    if (!coordinatorOnline_) return true;
    if (state_ == TopologyCommitState::Locked) return true;
    return false;
}

bool TopologyVersionManager::isAuthorityConfigured() const noexcept {
    return authorityNodeId_.has_value();
}

std::optional<NodeId> TopologyVersionManager::authorityNodeId() const noexcept {
    return authorityNodeId_;
}

}  // namespace cfx
