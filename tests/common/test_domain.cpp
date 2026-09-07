#include <cstdio>
#include <string>
#include <vector>

#include "common/domain.hpp"

namespace {

int testNodeIdGenerate() {
    using namespace cfx;
    NodeId a = NodeId::generate();
    NodeId b = NodeId::generate();

    if (a.isNull()) return 1;
    if (b.isNull()) return 1;
    if (a == b) return 1;
    if ((a.high & 0xF000000000000ULL) != 0x4000000000000ULL) return 1;
    if ((a.low  & 0xC000000000000000ULL) != 0x8000000000000000ULL) return 1;
    return 0;
}

int testTraceIdGenerate() {
    using namespace cfx;
    TraceId t = TraceId::generate();
    if (t.isNull()) return 1;
    if ((t.high & 0xF000000000000ULL) != 0x4000000000000ULL) return 1;
    if ((t.low  & 0xC000000000000000ULL) != 0x8000000000000000ULL) return 1;
    return 0;
}

int testCanonicalInputEventValidate() {
    using namespace cfx;
    NodeId src = NodeId::generate();

    CanonicalInputEvent ev{};
    ev.eventId = 1;
    ev.sourceNodeId = src;
    ev.timestamp = 1000;
    ev.eventType = EventType::MouseMove;
    ev.payload = MouseMovePayload{10, 20};
    ev.modifierState = ModifierState{false, false, false, false, false};
    if (!ev.validate()) return 1;

    CanonicalInputEvent badEventId = ev;
    badEventId.eventId = 0;
    if (badEventId.validate()) return 1;

    CanonicalInputEvent badTs = ev;
    badTs.timestamp = 0;
    if (badTs.validate()) return 1;

    CanonicalInputEvent badSrc = ev;
    badSrc.sourceNodeId = NodeId{0, 0};
    if (badSrc.validate()) return 1;

    CanonicalInputEvent mismatchedPayload = ev;
    mismatchedPayload.eventType = EventType::KeyPress;
    mismatchedPayload.payload = KeyPayload{65};
    if (!mismatchedPayload.validate()) return 1;

    CanonicalInputEvent wrongPayload = ev;
    wrongPayload.eventType = EventType::KeyPress;
    if (wrongPayload.validate()) return 1;
    return 0;
}

int testScreenBoundary() {
    using namespace cfx;
    ScreenBoundary sb{1920, 1080, 0, 0};
    if (!sb.isValid()) return 1;

    ScreenBoundary zero{0, 1080, 0, 0};
    if (zero.isValid()) return 1;
    return 0;
}

int testTopologyViewLinear() {
    using namespace cfx;
    NodeId a = NodeId::generate();
    NodeId b = NodeId::generate();
    NodeId c = NodeId::generate();

    TopologyView topo;
    topo.version = 1;
    topo.endpoints = {
        {a, Platform::Mac, {1920, 1080, 0, 0}},
        {b, Platform::Win, {1920, 1080, 0, 0}},
        {c, Platform::Mac, {1920, 1080, 0, 0}},
    };
    topo.neighborRelations = {
        {a, std::nullopt, b, NeighborState::Offline, NeighborState::Online},
        {b, a, c, NeighborState::Online, NeighborState::Online},
        {c, b, std::nullopt, NeighborState::Online, NeighborState::Offline},
    };

    if (!topo.isLinear()) return 1;

    auto neighbor = topo.neighborOf(b, EdgeDirection::Right);
    if (!neighbor) return 1;
    if (neighbor->nodeId != c) return 1;

    auto leftOfA = topo.neighborOf(a, EdgeDirection::Left);
    if (leftOfA) return 1;

    auto rightOfC = topo.neighborOf(c, EdgeDirection::Right);
    if (rightOfC) return 1;
    return 0;
}

int testTopologyViewDuplicateNodeIds() {
    using namespace cfx;
    NodeId a = NodeId::generate();

    TopologyView topo;
    topo.version = 1;
    topo.endpoints = {
        {a, Platform::Mac, {1920, 1080, 0, 0}},
        {a, Platform::Win, {1920, 1080, 0, 0}},
    };
    if (topo.isLinear()) return 1;
    if (!topo.hasDuplicateNodeIds()) return 1;
    return 0;
}

int testTopologyViewNeighborOverLimit() {
    using namespace cfx;
    NodeId a = NodeId::generate();
    NodeId b = NodeId::generate();
    NodeId c = NodeId::generate();

    TopologyView topo;
    topo.version = 1;
    topo.endpoints = {
        {a, Platform::Mac, {1920, 1080, 0, 0}},
        {b, Platform::Win, {1920, 1080, 0, 0}},
        {c, Platform::Mac, {1920, 1080, 0, 0}},
    };
    topo.neighborRelations = {
        {a, b, c, NeighborState::Online, NeighborState::Online},
    };
    if (topo.neighborRelations[0].neighborCount() != 2) return 1;
    if (!topo.isLinear()) return 1;
    return 0;
}

int testEventPayloadVariant() {
    using namespace cfx;
    EventPayload p1 = MouseMovePayload{5, -3};
    EventPayload p2 = MouseButtonPayload{MouseButton::Left};
    EventPayload p3 = WheelPayload{-1, WheelAxis::Vertical};
    EventPayload p4 = KeyPayload{65};

    if (!std::holds_alternative<MouseMovePayload>(p1)) return 1;
    if (!std::holds_alternative<MouseButtonPayload>(p2)) return 1;
    if (!std::holds_alternative<WheelPayload>(p3)) return 1;
    if (!std::holds_alternative<KeyPayload>(p4)) return 1;

    if (std::get<MouseMovePayload>(p1).deltaX != 5) return 1;
    if (std::get<MouseButtonPayload>(p2).button != MouseButton::Left) return 1;
    if (std::get<WheelPayload>(p3).delta != -1) return 1;
    if (std::get<KeyPayload>(p4).keyCode != 65) return 1;
    return 0;
}

int testHandoffContext() {
    using namespace cfx;
    HandoffContext ctx{};
    ctx.traceId = TraceId::generate();
    ctx.sourceNodeId = NodeId::generate();
    ctx.targetNodeId = NodeId::generate();
    ctx.edgeDirection = EdgeDirection::Right;
    ctx.overflow = 5;
    ctx.entryCoord = EntryCoord{5, 540};
    ctx.modifierSnapshot = ModifierState{true, false, false, false, false};
    ctx.createdAt = 1000;

    if (ctx.traceId.isNull()) return 1;
    if (ctx.sourceNodeId == ctx.targetNodeId) return 1;
    if (ctx.overflow != 5) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testNodeIdGenerate()) { std::fputs("FAIL: testNodeIdGenerate\n", stderr); return 1; }
    if (testTraceIdGenerate()) { std::fputs("FAIL: testTraceIdGenerate\n", stderr); return 1; }
    if (testCanonicalInputEventValidate()) { std::fputs("FAIL: testCanonicalInputEventValidate\n", stderr); return 1; }
    if (testScreenBoundary()) { std::fputs("FAIL: testScreenBoundary\n", stderr); return 1; }
    if (testTopologyViewLinear()) { std::fputs("FAIL: testTopologyViewLinear\n", stderr); return 1; }
    if (testTopologyViewDuplicateNodeIds()) { std::fputs("FAIL: testTopologyViewDuplicateNodeIds\n", stderr); return 1; }
    if (testTopologyViewNeighborOverLimit()) { std::fputs("FAIL: testTopologyViewNeighborOverLimit\n", stderr); return 1; }
    if (testEventPayloadVariant()) { std::fputs("FAIL: testEventPayloadVariant\n", stderr); return 1; }
    if (testHandoffContext()) { std::fputs("FAIL: testHandoffContext\n", stderr); return 1; }
    return 0;
}