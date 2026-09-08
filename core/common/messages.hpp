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

struct DiscoveryAnnouncement {
    DiscoveryDigest digest;
    SessionFence fence;
};

struct PairingRequest {
    NodeId requesterNodeId;
    std::string pairingCode;
    SessionFence fence;
};

struct PairingResponse {
    NodeId responderNodeId;
    bool accepted;
    std::optional<NodeIdentity> identity;
    SessionFence fence;
};

struct RegistrationRequest {
    NodeIdentity identity;
    SessionFence fence;
};

struct RegistrationResponse {
    NodeId responderNodeId;
    bool accepted;
    std::optional<TopologyMembership> membership;
    SessionFence fence;
};

enum class MembershipChangeType : u8 {
    Joined,
    Left,
    Updated,
};

struct MembershipChangeNotification {
    MembershipChangeType changeType;
    NodeId memberId;
    TopologyMembership membership;
    TopologyVersion topologyVersion;
    SessionFence fence;
};

struct IdentityRecoveryRequest {
    NodeId nodeId;
    SessionEpoch lastKnownEpoch;
    SessionInstanceId lastKnownInstance;
    SessionFence fence;
};

struct IdentityRecoveryResponse {
    NodeId responderNodeId;
    bool recovered;
    std::optional<NodeIdentity> identity;
    SessionFence fence;
};

struct GoodbyeAnnouncement {
    NodeId nodeId;
    SessionFence fence;
};

using ControlMessage = std::variant<
    HandoffRequestMessage,
    HandoffResponseMessage,
    HeartbeatMessage,
    TopologyDeclarationMessage,
    TopologySyncMessage,
    PairingMessage,
    DiscoveryAnnouncement,
    PairingRequest,
    PairingResponse,
    RegistrationRequest,
    RegistrationResponse,
    MembershipChangeNotification,
    IdentityRecoveryRequest,
    IdentityRecoveryResponse,
    GoodbyeAnnouncement
>;

struct SendResult {
    bool ok;
    u32 seq;
};

}  // namespace cfx