#pragma once

#include <optional>

#include "common/domain.hpp"
#include "common/messages.hpp"

namespace cfx {

class ITopologyManager {
public:
    virtual ~ITopologyManager() = default;
    virtual EndpointIdentity loadIdentity() = 0;
    virtual TopologyView loadTopology(const TopologyConfig& config) = 0;
    virtual TopologyView currentView() = 0;
    virtual MergeResult mergeRemoteDeclaration(const TopologyDeclaration& decl) = 0;
    virtual std::optional<EndpointIdentity> neighbor(EdgeDirection direction) = 0;
    virtual void setNeighborState(const NodeId& nodeId, NeighborState state) = 0;
    virtual void updateScreenBoundary(const ScreenBoundary& boundary) = 0;
};

}  // namespace cfx