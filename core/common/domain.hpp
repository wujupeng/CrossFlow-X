#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "common/error_code.hpp"

namespace cfx {

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i32 = int32_t;

enum class EventType : u8 {
    MouseMove,
    MouseButtonPress,
    MouseButtonRelease,
    Wheel,
    KeyPress,
    KeyRelease,
};

enum class EdgeDirection : u8 {
    Left,
    Right,
};

enum class EndpointState : u8 {
    Master,
    Slave,
    Idle,
};

enum class Platform : u8 {
    Mac,
    Win,
};

enum class FrameType : u8 {
    InputEvent,
    HandoffRequest,
    HandoffResponse,
    Heartbeat,
    HeartbeatAck,
    TopologyDeclaration,
    VersionNegotiation,
    PairingRequest,
    PairingResponse,
};

enum class LinkState : u8 {
    Disconnected,
    Connecting,
    Connected,
    Pairing,
    Paired,
    Lost,
};

enum class NeighborState : u8 {
    Online,
    Offline,
};

enum class MouseButton : u8 {
    Left,
    Right,
    Middle,
};

enum class WheelAxis : u8 {
    Vertical,
    Horizontal,
};

using KeyCode = u16;

enum class RejectReason : u8 {
    TopologyConflict,
    BusyHandoff,
    NotNeighbor,
    Unpaired,
};

enum class RollbackReason : u8 {
    Timeout,
    LinkDown,
    TargetReject,
};

struct NodeId {
    u64 high;
    u64 low;

    bool operator==(const NodeId& other) const noexcept {
        return high == other.high && low == other.low;
    }
    bool operator!=(const NodeId& other) const noexcept {
        return !(*this == other);
    }
    bool operator<(const NodeId& other) const noexcept {
        if (high != other.high) return high < other.high;
        return low < other.low;
    }

    static NodeId generate();
    bool isNull() const noexcept { return high == 0 && low == 0; }
};

struct TraceId {
    u64 high;
    u64 low;

    bool operator==(const TraceId& other) const noexcept {
        return high == other.high && low == other.low;
    }
    bool operator!=(const TraceId& other) const noexcept {
        return !(*this == other);
    }
    bool operator<(const TraceId& other) const noexcept {
        if (high != other.high) return high < other.high;
        return low < other.low;
    }

    static TraceId generate();
    bool isNull() const noexcept { return high == 0 && low == 0; }
};

struct ScreenBoundary {
    u32 width;
    u32 height;
    u32 originX;
    u32 originY;

    bool isValid() const noexcept {
        return width > 0 && height > 0;
    }
};

struct EntryCoord {
    u32 x;
    u32 y;
};

struct ModifierState {
    bool shift;
    bool ctrl;
    bool alt;
    bool cmd;
    bool fn;

    bool operator==(const ModifierState& other) const noexcept {
        return shift == other.shift && ctrl == other.ctrl && alt == other.alt
            && cmd == other.cmd && fn == other.fn;
    }
};

struct MouseMovePayload {
    i32 deltaX;
    i32 deltaY;
};

struct MouseButtonPayload {
    MouseButton button;
};

struct WheelPayload {
    i32 delta;
    WheelAxis axis;
};

struct KeyPayload {
    KeyCode keyCode;
};

using EventPayload = std::variant<MouseMovePayload, MouseButtonPayload, WheelPayload, KeyPayload>;

struct CanonicalInputEvent {
    u64 eventId;
    NodeId sourceNodeId;
    u64 timestamp;
    EventType eventType;
    EventPayload payload;
    ModifierState modifierState;

    bool validate() const noexcept;
};

struct EndpointIdentity {
    NodeId nodeId;
    Platform platform;
    ScreenBoundary screenBoundary;

    bool isValid() const noexcept {
        return !nodeId.isNull() && screenBoundary.isValid();
    }
};

struct NeighborRelation {
    NodeId nodeId;
    std::optional<NodeId> leftNeighbor;
    std::optional<NodeId> rightNeighbor;
    NeighborState leftState;
    NeighborState rightState;

    u8 neighborCount() const noexcept {
        return static_cast<u8>(leftNeighbor.has_value() + rightNeighbor.has_value());
    }
};

struct TopologyView {
    std::vector<EndpointIdentity> endpoints;
    std::vector<NeighborRelation> neighborRelations;
    u64 version;

    bool isLinear() const noexcept;
    std::optional<EndpointIdentity> neighborOf(const NodeId& nodeId, EdgeDirection direction) const;
    bool hasDuplicateNodeIds() const noexcept;
};

struct HandoffContext {
    TraceId traceId;
    NodeId sourceNodeId;
    NodeId targetNodeId;
    EdgeDirection edgeDirection;
    u32 overflow;
    EntryCoord entryCoord;
    ModifierState modifierSnapshot;
    u64 createdAt;
};

struct Link {
    NodeId localNodeId;
    NodeId remoteNodeId;
    u16 protocolVersion;
    LinkState state;
    bool paired;
    u64 lastHeartbeatAt;
};

enum class InputType : u8 {
    Mouse,
    Keyboard,
};

enum class AddressType : u8 {
    IPv4,
    IPv6,
    Hostname,
};

struct PublicKey {
    std::vector<u8> bytes;
    bool isPresent{false};

    static PublicKey empty() noexcept { return PublicKey{}; }
    bool isEmpty() const noexcept { return !isPresent && bytes.empty(); }
};

struct Capabilities {
    std::vector<InputType> supportedInputTypes;
    ScreenBoundary screenBoundary;
    bool supportsCircular{false};
    u16 protocolVersion{0};

    bool isValid() const noexcept {
        return protocolVersion > 0 && screenBoundary.isValid();
    }
};

struct EndpointAddress {
    AddressType addressType;
    std::string value;
    u16 port;
    u8 priority;
};

struct TopologyMembership {
    std::string topologyId;
    std::optional<NodeId> leftNeighbor;
    std::optional<NodeId> rightNeighbor;
    u32 segmentIndex;
};

struct SessionEpoch {
    u64 value{0};

    void increment() noexcept { ++value; }
    bool isMonotonicAfter(const SessionEpoch& prev) const noexcept {
        return value > prev.value;
    }
};

struct SessionInstanceId {
    NodeId nodeId;
    u64 createdAt{0};
    bool isActive{true};
};

struct SessionFence {
    NodeId nodeId;
    SessionEpoch epoch;
    SessionInstanceId instanceId;

    static SessionFence newSession(const NodeId& nid) noexcept {
        SessionFence f;
        f.nodeId = nid;
        f.epoch.value = 1;
        f.instanceId.nodeId = nid;
        f.instanceId.createdAt = 0;
        f.instanceId.isActive = true;
        return f;
    }
};

struct NodeIdentity {
    NodeId nodeId;
    PublicKey publicKey;
    Platform platform;
    Capabilities capabilities;
    std::vector<EndpointAddress> endpointAddresses;
    std::optional<TopologyMembership> topologyMembership;
    SessionEpoch sessionEpoch;

    bool isValid() const noexcept {
        return !nodeId.isNull() && capabilities.isValid();
    }
};

struct DiscoveryRecord {
    NodeId nodeId;
    Platform platform;
    std::vector<InputType> capabilitiesFingerprint;
    SessionEpoch sessionEpoch;
    u16 protocolVersion;
    std::string topologyId;
    std::vector<EndpointAddress> endpointAddresses;
    u64 lastSeenAt;
};

struct DiscoveryDigest {
    NodeId nodeId;
    Platform platform;
    std::vector<InputType> capabilitiesFingerprint;
    SessionEpoch sessionEpoch;
    u16 protocolVersion;
    std::string topologyId;

    std::vector<std::pair<std::string, std::string>> toTxtRecord() const;
};

enum class PairingState : u8 {
    Discovered,
    Untrusted,
    Pairing,
    Trusted,
    Registering,
    Registered,
    Member,
    Rejected,
    Rollback,
};

struct TrustedNodeEntry {
    NodeId nodeId;
    u64 pairedAt;
    SessionEpoch lastSeenEpoch;
};

struct RegistrationRecord {
    NodeId peerNodeId;
    NodeIdentity peerIdentity;
    PairingState currentState;
    PairingState previousState;
    u64 enteredAt;
    std::optional<ErrorCode> failureReason;
};

struct MembershipEntry {
    NodeId nodeId;
    TopologyMembership topologyMembership;
    u64 joinedAt;
    bool isOnline;
};

enum class TopologyCommitState : u8 {
    Active,
    Proposed,
    Validated,
    Prepared,
    ActivationAuthorized,
    Committing,
    RolledBack,
    Locked,
};

struct TopologyVersion {
    u64 value{0};
    std::string topologyId;
    u64 lastCommitAt{0};
    TopologyCommitState commitState{TopologyCommitState::Active};
};

struct TopologyChangeProposal {
    TopologyVersion proposedVersion;
    std::vector<MembershipEntry> membershipChange;
    NodeId proposedBy;
    u64 proposedAt;
};

enum class SessionFenceVerdict : u8 {
    Accept,
    Reject,
    StaleEpoch,
    InstanceConflict,
    UnknownNode,
};

}  // namespace cfx