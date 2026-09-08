#pragma once

#include <optional>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

class ITopologyVersionManager {
public:
    virtual ~ITopologyVersionManager() = default;

    virtual TopologyVersion currentVersion() const noexcept = 0;
    virtual TopologyCommitState commitState() const noexcept = 0;

    virtual std::optional<ErrorCode> proposeChange(const TopologyChangeProposal& proposal) noexcept = 0;
    virtual std::optional<ErrorCode> validateProposal() noexcept = 0;
    virtual std::optional<ErrorCode> prepareCommit() noexcept = 0;
    virtual std::optional<ErrorCode> activateVersion() noexcept = 0;
    virtual std::optional<ErrorCode> abortCommit() noexcept = 0;

    virtual bool isCoordinator() const noexcept = 0;
    virtual std::optional<NodeId> coordinatorId() const noexcept = 0;
    virtual std::optional<ErrorCode> setCoordinator(const NodeId& authorityNodeId) noexcept = 0;
    virtual bool isCommitLocked() const noexcept = 0;
};

}  // namespace cfx