#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

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

}  // namespace cfx