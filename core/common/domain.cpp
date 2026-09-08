#include "common/domain.hpp"

#include <algorithm>
#include <random>

namespace cfx {

namespace {

void setUuidV4Bits(u64& high, u64& low) noexcept {
    high = (high & 0xFFF0FFFFFFFFFFFFULL) | 0x0004000000000000ULL;
    low  = (low  & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
}

u64 randomU64() {
    std::random_device rd;
    return (static_cast<u64>(rd()) << 32) | static_cast<u64>(rd());
}

}  // namespace

NodeId NodeId::generate() {
    NodeId id;
    id.high = randomU64();
    id.low  = randomU64();
    setUuidV4Bits(id.high, id.low);
    return id;
}

TraceId TraceId::generate() {
    TraceId id;
    id.high = randomU64();
    id.low  = randomU64();
    setUuidV4Bits(id.high, id.low);
    return id;
}

bool CanonicalInputEvent::validate() const noexcept {
    if (eventId == 0) return false;
    if (timestamp == 0) return false;
    if (sourceNodeId.isNull()) return false;

    switch (eventType) {
        case EventType::MouseMove:
            if (!std::holds_alternative<MouseMovePayload>(payload)) return false;
            break;
        case EventType::MouseButtonPress:
        case EventType::MouseButtonRelease:
            if (!std::holds_alternative<MouseButtonPayload>(payload)) return false;
            break;
        case EventType::Wheel:
            if (!std::holds_alternative<WheelPayload>(payload)) return false;
            break;
        case EventType::KeyPress:
        case EventType::KeyRelease:
            if (!std::holds_alternative<KeyPayload>(payload)) return false;
            break;
    }
    return true;
}

bool TopologyView::hasDuplicateNodeIds() const noexcept {
    for (std::size_t i = 0; i < endpoints.size(); ++i) {
        for (std::size_t j = i + 1; j < endpoints.size(); ++j) {
            if (endpoints[i].nodeId == endpoints[j].nodeId) return true;
        }
    }
    return false;
}

bool TopologyView::isLinear() const noexcept {
    if (endpoints.empty()) return false;
    if (hasDuplicateNodeIds()) return false;

    for (const auto& rel : neighborRelations) {
        if (rel.neighborCount() > 2) return false;
    }

    for (const auto& rel : neighborRelations) {
        bool found = false;
        for (const auto& ep : endpoints) {
            if (ep.nodeId == rel.nodeId) {
                found = true;
                break;
            }
        }
        if (!found) return false;

        auto checkNeighborExists = [&](const std::optional<NodeId>& neighbor) {
            if (!neighbor) return true;
            for (const auto& ep : endpoints) {
                if (ep.nodeId == *neighbor) return true;
            }
            return false;
        };
        if (!checkNeighborExists(rel.leftNeighbor)) return false;
        if (!checkNeighborExists(rel.rightNeighbor)) return false;
    }

    return true;
}

std::optional<EndpointIdentity> TopologyView::neighborOf(const NodeId& nodeId, EdgeDirection direction) const {
    for (const auto& rel : neighborRelations) {
        if (rel.nodeId != nodeId) continue;

        std::optional<NodeId> neighborId;
        NeighborState neighborState;

        if (direction == EdgeDirection::Left) {
            neighborId = rel.leftNeighbor;
            neighborState = rel.leftState;
        } else {
            neighborId = rel.rightNeighbor;
            neighborState = rel.rightState;
        }

        if (!neighborId) return std::nullopt;
        if (neighborState == NeighborState::Offline) return std::nullopt;

        for (const auto& ep : endpoints) {
            if (ep.nodeId == *neighborId) return ep;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> DiscoveryDigest::toTxtRecord() const {
    std::vector<std::pair<std::string, std::string>> record;
    record.reserve(8);
    record.emplace_back("nid", std::to_string(nodeId.high) + std::to_string(nodeId.low));
    record.emplace_back("plat", platform == Platform::Mac ? "mac" : "win");
    record.emplace_back("epoch", std::to_string(sessionEpoch.value));
    record.emplace_back("pver", std::to_string(protocolVersion));
    record.emplace_back("tid", topologyId);
    std::string caps;
    for (auto cap : capabilitiesFingerprint) {
        if (!caps.empty()) caps += ",";
        caps += (cap == InputType::Mouse) ? "mouse" : "keyboard";
    }
    record.emplace_back("caps", caps);
    return record;
}

}  // namespace cfx