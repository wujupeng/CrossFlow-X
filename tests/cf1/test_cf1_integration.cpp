#include <cstdio>
#include <string>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s06_identity/node_identity_manager.hpp"
#include "s06_identity/session_fence.hpp"
#include "s06_identity/identity_recovery_manager.hpp"
#include "s07_discovery/discovery_service.hpp"
#include "s08_pairing/pairing_fsm.hpp"
#include "s08_pairing/trusted_node_list.hpp"
#include "s08_pairing/registration_manager.hpp"
#include "s08_pairing/handshake_orchestrator.hpp"
#include "s09_membership/membership_manager.hpp"
#include "s09_membership/topology_version_manager.hpp"

namespace {

using namespace cfx;

struct TestContext {
    NodeIdentityManager identityA;
    NodeIdentityManager identityB;
    DiscoveryService discovery;
    PairingFsm pairing;
    RegistrationManager registration;
    TrustedNodeList trusted;
    MembershipManager membership;
    TopologyVersionManager topology;
    SessionFenceImpl fence;

    TestContext() : fence(NodeId{0,0}) {}
};

TestContext setupContext() {
    TestContext ctx;
    NodeIdentity idA{};
    idA.nodeId = NodeId::generate();
    idA.platform = Platform::Mac;
    idA.capabilities.protocolVersion = 1;
    idA.capabilities.screenBoundary = {1920, 1080, 0, 0};
    ctx.identityA.initialize(idA);

    NodeIdentity idB{};
    idB.nodeId = NodeId::generate();
    idB.platform = Platform::Win;
    idB.capabilities.protocolVersion = 1;
    idB.capabilities.screenBoundary = {1920, 1080, 0, 0};
    ctx.identityB.initialize(idB);

    ctx.fence = SessionFenceImpl(idA.nodeId);
    return ctx;
}

int testEndToEndIdentityEstablishment() {
    auto ctx = setupContext();
    auto idA = ctx.identityA.getIdentity();
    if (!idA) return 1;

    DiscoveryDigest digest;
    digest.nodeId = idA->nodeId;
    digest.platform = idA->platform;
    digest.sessionEpoch = idA->sessionEpoch;
    digest.protocolVersion = 1;
    digest.topologyId = "topo1";
    auto err = ctx.discovery.startAnnouncing(digest);
    if (err) return 1;
    ctx.discovery.startListening();

    DiscoveryAnnouncement ann;
    ann.digest = digest;
    ann.fence = SessionFence::newSession(idA->nodeId);
    err = ctx.discovery.handleDiscoveryAnnouncement(ann);
    if (err) return 1;
    auto found = ctx.discovery.findNode(idA->nodeId);
    if (!found) return 1;

    HandshakeOrchestrator hs(ctx.pairing, ctx.registration, ctx.trusted, ctx.identityA);
    auto idB = ctx.identityB.getIdentity();
    err = hs.initiateHandshake(idB->nodeId, "123456");
    if (err) return 1;
    err = hs.rollbackHandshake(idB->nodeId);
    if (err) return 1;
    return 0;
}

int testTopologyAtomicCommit() {
    auto ctx = setupContext();
    auto idA = ctx.identityA.getIdentity();
    ctx.topology.setLocalNodeId(idA->nodeId);
    auto err = ctx.topology.setTopologyAuthority(idA->nodeId);
    if (err) return 1;
    if (!ctx.topology.isCoordinator()) return 1;

    TopologyChangeProposal proposal;
    proposal.proposedBy = idA->nodeId;
    proposal.proposedVersion.value = 2;
    proposal.proposedVersion.topologyId = "topo1";

    err = ctx.topology.proposeChange(proposal);
    if (err) return 1;
    err = ctx.topology.validateProposal();
    if (err) return 1;
    err = ctx.topology.prepareCommit();
    if (err) return 1;
    err = ctx.topology.activateVersion();
    if (!err) return 1;
    err = ctx.topology.authorizeActivation();
    if (err) return 1;
    err = ctx.topology.activateVersion();
    if (err) return 1;
    if (ctx.topology.currentVersion().value != 2) return 1;
    return 0;
}

int testSessionFencing() {
    auto ctx = setupContext();
    auto idA = ctx.identityA.getIdentity();
    auto verdict = ctx.fence.checkIncoming(idA->nodeId, SessionEpoch{1}, ctx.fence.currentInstance());
    if (verdict != SessionFenceVerdict::Accept) return 1;
    NodeId unknown = NodeId::generate();
    verdict = ctx.fence.checkIncoming(unknown, SessionEpoch{1}, ctx.fence.currentInstance());
    if (verdict != SessionFenceVerdict::UnknownNode) return 1;
    verdict = ctx.fence.checkIncoming(idA->nodeId, SessionEpoch{0}, ctx.fence.currentInstance());
    if (verdict != SessionFenceVerdict::StaleEpoch) return 1;
    return 0;
}

int testIdentityRecoveryFlow() {
    auto ctx = setupContext();
    auto idA = ctx.identityA.getIdentity();
    auto idB = ctx.identityB.getIdentity();

    TrustedNodeEntry entry;
    entry.nodeId = idB->nodeId;
    entry.pairedAt = 1000;
    entry.lastSeenEpoch = SessionEpoch{1};
    ctx.trusted.add(entry);

    IdentityRecoveryManager recovery(ctx.identityA, ctx.trusted);
    auto err = recovery.initiateRecovery(idB->nodeId);
    if (err) return 1;

    IdentityRecoveryRequest request;
    request.nodeId = idB->nodeId;
    request.lastKnownEpoch = SessionEpoch{1};
    request.lastKnownInstance = SessionInstanceId{idB->nodeId, 0, true};
    request.fence = SessionFence::newSession(idB->nodeId);

    IdentityRecoveryResponse response;
    err = recovery.handleRecoveryRequest(request, response);
    if (err) return 1;
    if (!response.recovered) return 1;
    if (!response.identity) return 1;
    return 0;
}

int testCoordinatorOfflineLocks() {
    auto ctx = setupContext();
    auto idA = ctx.identityA.getIdentity();
    ctx.topology.setLocalNodeId(idA->nodeId);
    ctx.topology.setTopologyAuthority(idA->nodeId);

    TopologyChangeProposal proposal;
    proposal.proposedBy = idA->nodeId;
    proposal.proposedVersion.value = 2;

    ctx.topology.markCoordinatorOffline();
    if (!ctx.topology.isCommitLocked()) return 1;
    auto err = ctx.topology.proposeChange(proposal);
    if (!err) return 1;
    if (ctx.topology.currentVersion().value != 0) return 1;

    ctx.topology.markCoordinatorOnline();
    err = ctx.topology.proposeChange(proposal);
    if (err) return 1;
    return 0;
}

int testNoSplitBrain() {
    auto ctx = setupContext();
    auto idA = ctx.identityA.getIdentity();
    auto idB = ctx.identityB.getIdentity();

    ctx.topology.setLocalNodeId(idA->nodeId);
    ctx.topology.setTopologyAuthority(idA->nodeId);
    if (!ctx.topology.isCoordinator()) return 1;

    TopologyVersionManager mgrB;
    mgrB.setLocalNodeId(idB->nodeId);
    auto err = mgrB.setTopologyAuthority(idA->nodeId);
    if (err) return 1;
    if (mgrB.isCoordinator()) return 1;
    TopologyChangeProposal proposal;
    proposal.proposedBy = idB->nodeId;
    err = mgrB.proposeChange(proposal);
    if (!err) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testEndToEndIdentityEstablishment()) { std::puts("FAIL: testEndToEndIdentityEstablishment"); return 1; }
    if (testTopologyAtomicCommit()) { std::puts("FAIL: testTopologyAtomicCommit"); return 1; }
    if (testSessionFencing()) { std::puts("FAIL: testSessionFencing"); return 1; }
    if (testIdentityRecoveryFlow()) { std::puts("FAIL: testIdentityRecoveryFlow"); return 1; }
    if (testCoordinatorOfflineLocks()) { std::puts("FAIL: testCoordinatorOfflineLocks"); return 1; }
    if (testNoSplitBrain()) { std::puts("FAIL: testNoSplitBrain"); return 1; }
    std::puts("ALL INTEGRATION TESTS PASS");
    return 0;
}