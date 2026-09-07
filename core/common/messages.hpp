#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "common/domain.hpp"

namespace cfx {

struct HandoffRequest {
    TraceId traceId;
    NodeId sourceNodeId;
    NodeId targetNodeId;
    EdgeDirection edgeDirection;
    u32 overflow;
    ModifierState modifierSnapshot;
};

struct HandoffResponse {
    TraceId traceId;
    bool accepted;
    std::optional<RejectReason> reason;
};

struct HandoffTransferred {
    NodeId target;
};

struct HandoffRolledBack {
    RollbackReason reason;
};

using HandoffOutcome = std::variant<HandoffTransferred, HandoffRolledBack>;

struct OverflowMapInput {
    ScreenBoundary sourceBoundary;
    ScreenBoundary targetBoundary;
    EdgeDirection edgeDirection;
    u32 overflow;
    u32 sourceVertical;
};

struct TopologyConfig {
    std::vector<EndpointIdentity> endpoints;
    std::vector<NeighborRelation> neighborRelations;
};

struct TopologyDeclaration {
    NodeId declarerNodeId;
    Platform platform;
    ScreenBoundary screenBoundary;
    std::optional<NodeId> leftNeighbor;
    std::optional<NodeId> rightNeighbor;
    u64 topologyVersion;
};

struct NodeIdConflict {
    NodeId conflictingNodeId;
    NodeId existingNodeId;
};

struct MergeResult {
    bool ok;
    std::optional<NodeIdConflict> conflict;
};

struct LinkStateEvent {
    NodeId remoteNodeId;
    LinkState state;
};

struct HandoffRequestMessage {
    HandoffRequest request;
};

struct HandoffResponseMessage {
    HandoffResponse response;
};

struct HeartbeatMessage {
    u64 timestamp;
};

struct TopologyDeclarationMessage {
    TopologyDeclaration declaration;
};

struct TopologySyncMessage {
    TopologyView view;
};

struct PairingMessage {
    std::string pairingCode;
};

using ControlMessage = std::variant<
    HandoffRequestMessage,
    HandoffResponseMessage,
    HeartbeatMessage,
    TopologyDeclarationMessage,
    TopologySyncMessage,
    PairingMessage
>;

struct SendResult {
    bool ok;
    u32 seq;
};

}  // namespace cfx