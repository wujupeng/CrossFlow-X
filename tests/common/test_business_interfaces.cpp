#include "common/domain.hpp"
#include "common/messages.hpp"
#include "s01_event/i_event_normalizer.hpp"
#include "s02_topology/i_topology_manager.hpp"
#include "s03_handoff/i_handoff_orchestrator.hpp"
#include "s04_coord/i_coord_mapper.hpp"
#include "s05_transport/i_control_plane_channel.hpp"
#include "s05_transport/i_input_plane_channel.hpp"

namespace {

int testControlMessageVariant() {
    using namespace cfx;
    ControlMessage m1 = HandoffRequestMessage{HandoffRequest{}};
    ControlMessage m2 = HandoffResponseMessage{HandoffResponse{}};
    ControlMessage m3 = HeartbeatMessage{1000};
    ControlMessage m4 = TopologyDeclarationMessage{TopologyDeclaration{}};
    ControlMessage m5 = TopologySyncMessage{TopologyView{}};
    ControlMessage m6 = PairingMessage{"ABC123"};

    if (!std::holds_alternative<HandoffRequestMessage>(m1)) return 1;
    if (!std::holds_alternative<HandoffResponseMessage>(m2)) return 1;
    if (!std::holds_alternative<HeartbeatMessage>(m3)) return 1;
    if (!std::holds_alternative<TopologyDeclarationMessage>(m4)) return 1;
    if (!std::holds_alternative<TopologySyncMessage>(m5)) return 1;
    if (!std::holds_alternative<PairingMessage>(m6)) return 1;
    return 0;
}

int testHandoffOutcomeVariant() {
    using namespace cfx;
    HandoffOutcome transferred = HandoffTransferred{NodeId::generate()};
    HandoffOutcome rolledBack = HandoffRolledBack{RollbackReason::Timeout};

    if (!std::holds_alternative<HandoffTransferred>(transferred)) return 1;
    if (!std::holds_alternative<HandoffRolledBack>(rolledBack)) return 1;
    if (std::get<HandoffRolledBack>(rolledBack).reason != RollbackReason::Timeout) return 1;
    return 0;
}

int testMockInterfaces() {
    using namespace cfx;

    struct MockNormalizer : IEventNormalizer {
        std::optional<CanonicalInputEvent> normalize(const RawInputEvent& raw) override {
            (void)raw;
            return std::nullopt;
        }
        ModifierState snapshotModifiers() override {
            return ModifierState{false, false, false, false, false};
        }
        void alignModifiers(const ModifierState& target) override { (void)target; }
    };

    struct MockTopology : ITopologyManager {
        EndpointIdentity loadIdentity() override { return EndpointIdentity{}; }
        TopologyView loadTopology(const TopologyConfig& config) override {
            (void)config;
            return TopologyView{};
        }
        TopologyView currentView() override { return TopologyView{}; }
        MergeResult mergeRemoteDeclaration(const TopologyDeclaration& decl) override {
            (void)decl;
            return MergeResult{true, std::nullopt};
        }
        std::optional<EndpointIdentity> neighbor(EdgeDirection direction) override {
            (void)direction;
            return std::nullopt;
        }
        void setNeighborState(const NodeId& nodeId, NeighborState state) override {
            (void)nodeId; (void)state;
        }
        void updateScreenBoundary(const ScreenBoundary& boundary) override { (void)boundary; }
    };

    struct MockHandoff : IHandoffOrchestrator {
        HandoffOutcome initiate(const HandoffRequest& req) override {
            (void)req;
            return HandoffTransferred{NodeId{}};
        }
        HandoffResponse handleIncoming(const HandoffRequest& req) override {
            (void)req;
            return HandoffResponse{TraceId{}, true, std::nullopt};
        }
        void handleResponse(const HandoffResponse& resp) override { (void)resp; }
        void onLinkDown(const NodeId& remoteNodeId) override { (void)remoteNodeId; }
    };

    struct MockCoordMapper : ICoordMapper {
        EntryCoord mapOverflow(const OverflowMapInput& input) override {
            (void)input;
            return EntryCoord{0, 0};
        }
        EntryCoord clamp(const EntryCoord& coord, const ScreenBoundary& targetBoundary) override {
            (void)targetBoundary;
            return coord;
        }
    };

    struct MockControlPlane : IControlPlaneChannel {
        SendResult send(const ControlMessage& msg) override {
            (void)msg;
            return SendResult{true, 1};
        }
        void onMessage(std::function<void(const ControlMessage&)> handler) override { (void)handler; }
        void onLinkState(std::function<void(const LinkStateEvent&)> handler) override { (void)handler; }
    };

    struct MockInputPlane : IInputPlaneChannel {
        void send(const CanonicalInputEvent& event) override { (void)event; }
        void onEvent(std::function<void(const CanonicalInputEvent&)> handler) override { (void)handler; }
        u32 backlog() override { return 0; }
    };

    MockNormalizer norm;
    MockTopology topo;
    MockHandoff handoff;
    MockCoordMapper mapper;
    MockControlPlane cp;
    MockInputPlane ip;

    auto mods = norm.snapshotModifiers();
    (void)mods;

    auto view = topo.currentView();
    (void)view;

    auto outcome = handoff.initiate(HandoffRequest{});
    if (!std::holds_alternative<HandoffTransferred>(outcome)) return 1;

    auto entry = mapper.mapOverflow(OverflowMapInput{});
    (void)entry;

    auto sr = cp.send(HeartbeatMessage{100});
    if (!sr.ok) return 1;

    if (ip.backlog() != 0) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testControlMessageVariant()) return 1;
    if (testHandoffOutcomeVariant()) return 1;
    if (testMockInterfaces()) return 1;
    return 0;
}