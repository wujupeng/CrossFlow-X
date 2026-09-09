#include <cstdio>
#include <string>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s06_identity/node_identity_manager.hpp"
#include "s06_identity/session_fence.hpp"
#include "s06_identity/identity_recovery_manager.hpp"
#include "s07_discovery/discovery_service.hpp"
#include "s07_discovery/mdns.hpp"
#include "s08_pairing/pairing_fsm.hpp"
#include "s08_pairing/trusted_node_list.hpp"
#include "s08_pairing/registration_manager.hpp"
#include "s08_pairing/handshake_orchestrator.hpp"
#include "s09_membership/membership_manager.hpp"
#include "s09_membership/topology_version_manager.hpp"

namespace {

using namespace cfx;

NodeIdentity makeIdentity(Platform plat) {
    NodeIdentity id{};
    id.nodeId = NodeId::generate();
    id.platform = plat;
    id.capabilities.protocolVersion = 1;
    id.capabilities.screenBoundary = {1920, 1080, 0, 0};
    id.capabilities.supportedInputTypes = {InputType::Mouse, InputType::Keyboard};
    return id;
}

int r2_registrationSuccessPath() {
    PairingFsm pairing;
    RegistrationManager reg;
    TrustedNodeList trusted;
    NodeIdentityManager identity;

    auto idA = makeIdentity(Platform::Mac);
    auto idB = makeIdentity(Platform::Win);
    identity.initialize(idA);

    HandshakeOrchestrator hs(pairing, reg, trusted, identity);

    auto err = hs.initiateHandshake(idB.nodeId, "123456");
    if (err) return 1;

    PairingResponse pairResp;
    pairResp.responderNodeId = idB.nodeId;
    pairResp.accepted = true;
    pairResp.fence = SessionFence::newSession(idB.nodeId);
    err = hs.handlePairingResponse(pairResp);
    if (err) return 1;
    if (!trusted.isTrusted(idB.nodeId)) return 1;

    RegistrationRequest regReq;
    regReq.identity = idB;
    regReq.fence = SessionFence::newSession(idB.nodeId);
    RegistrationResponse regResp;
    err = hs.handleRegistrationRequest(regReq, regResp);
    if (err) return 1;
    if (!regResp.accepted) return 1;

    err = hs.handleRegistrationResponse(regResp);
    if (err) return 1;

    err = hs.completeHandshake(idB.nodeId);
    if (err) return 1;
    if (!hs.isHandshakeComplete(idB.nodeId)) return 1;
    if (pairing.currentState(idB.nodeId) != PairingState::Member) return 1;
    return 0;
}

int r3_registrationAtomicRollback() {
    RegistrationManager reg;
    auto id = makeIdentity(Platform::Win);

    auto err = reg.registerPeer(id);
    if (err) return 1;
    if (!reg.isRegistered(id.nodeId)) return 1;

    err = reg.unregisterPeer(id.nodeId);
    if (err) return 1;
    if (reg.isRegistered(id.nodeId)) return 1;

    auto reg2 = reg.getRegistration(id.nodeId);
    if (reg2) return 1;

    NodeIdentity invalid{};
    invalid.nodeId = NodeId{0, 0};
    err = reg.registerPeer(invalid);
    if (!err) return 1;
    return 0;
}

int r4_sessionAdmission() {
    auto id = makeIdentity(Platform::Mac);
    SessionFenceImpl fence(id.nodeId);

    DiscoveryAnnouncement disc;
    disc.digest.nodeId = id.nodeId;
    disc.fence = SessionFence::newSession(id.nodeId);
    if (!fence.isBootstrapMessage(disc)) return 1;

    PairingRequest pairReq;
    pairReq.requesterNodeId = id.nodeId;
    pairReq.fence = SessionFence::newSession(id.nodeId);
    if (!fence.isBootstrapMessage(pairReq)) return 1;

    RegistrationRequest regReq;
    regReq.identity = id;
    regReq.fence = SessionFence::newSession(id.nodeId);
    if (fence.isBootstrapMessage(regReq)) return 1;
    if (!fence.isEstablishedSessionMessage(regReq)) return 1;

    GoodbyeAnnouncement goodbye;
    goodbye.nodeId = id.nodeId;
    goodbye.fence = SessionFence::newSession(id.nodeId);
    if (fence.isBootstrapMessage(goodbye)) return 1;
    if (!fence.isEstablishedSessionMessage(goodbye)) return 1;

    auto verdict = fence.checkIncoming(id.nodeId, SessionEpoch{1}, fence.currentInstance());
    if (verdict != SessionFenceVerdict::Accept) return 1;

    NodeId unknown = NodeId::generate();
    verdict = fence.checkIncoming(unknown, SessionEpoch{1}, fence.currentInstance());
    if (verdict != SessionFenceVerdict::UnknownNode) return 1;

    verdict = fence.checkIncoming(id.nodeId, SessionEpoch{0}, fence.currentInstance());
    if (verdict != SessionFenceVerdict::StaleEpoch) return 1;
    return 0;
}

int r5_identityRecoveryPersistence() {
    NodeIdentityManager mgr;
    auto id = makeIdentity(Platform::Mac);
    mgr.initialize(id);
    mgr.incrementEpoch();
    mgr.incrementEpoch();

    auto err = mgr.persist();
    if (err) return 1;

    NodeIdentityManager restored;
    restored = NodeIdentityManager("test_identity_recovery.bin");
    restored.initialize(id);
    restored.incrementEpoch();
    restored.incrementEpoch();
    err = restored.persist();
    if (err) return 1;

    NodeIdentityManager loaded("test_identity_recovery.bin");
    err = loaded.recoverNodeId();
    if (err) return 1;
    if (loaded.getNodeId() != id.nodeId) return 1;
    if (loaded.currentEpoch().value != 2) return 1;
    return 0;
}

int r6_topologyMultiNode() {
    auto idA = makeIdentity(Platform::Mac);
    auto idB = makeIdentity(Platform::Win);
    auto idC = makeIdentity(Platform::Win);

    TopologyVersionManager mgrA, mgrB, mgrC;
    mgrA.setLocalNodeId(idA.nodeId);
    mgrB.setLocalNodeId(idB.nodeId);
    mgrC.setLocalNodeId(idC.nodeId);

    mgrA.setTopologyAuthority(idA.nodeId);
    mgrB.setTopologyAuthority(idA.nodeId);
    mgrC.setTopologyAuthority(idA.nodeId);

    if (!mgrA.isCoordinator()) return 1;
    if (mgrB.isCoordinator()) return 1;
    if (mgrC.isCoordinator()) return 1;

    TopologyChangeProposal proposal;
    proposal.proposedBy = idA.nodeId;
    proposal.proposedVersion.value = 2;

    auto err = mgrB.proposeChange(proposal);
    if (!err) return 1;
    err = mgrC.proposeChange(proposal);
    if (!err) return 1;

    err = mgrA.proposeChange(proposal);
    if (err) return 1;
    err = mgrA.validateProposal();
    if (err) return 1;
    err = mgrA.prepareCommit();
    if (err) return 1;
    err = mgrA.authorizeActivation();
    if (err) return 1;
    err = mgrA.activateVersion();
    if (err) return 1;

    mgrA.markCoordinatorOffline();
    if (!mgrA.isCommitLocked()) return 1;
    err = mgrA.proposeChange(proposal);
    if (!err) return 1;
    if (mgrA.currentVersion().value != 2) return 1;

    mgrA.markCoordinatorOnline();
    return 0;
}

int r7_amend01_nodeIdNotPublicKey() {
    NodeIdentityManager mgr;
    auto id = makeIdentity(Platform::Mac);
    auto err = mgr.initialize(id);
    if (err) return 1;

    PublicKey newKey;
    newKey.bytes = {1, 2, 3, 4};
    newKey.isPresent = true;
    err = mgr.updatePublicKey(newKey);
    if (err) return 1;

    auto identity = mgr.getIdentity();
    if (!identity) return 1;
    if (identity->nodeId != id.nodeId) return 1;
    if (!identity->publicKey.isPresent) return 1;
    if (identity->publicKey.bytes.size() != 4) return 1;
    return 0;
}

int r7_amend02_pairingStateSeparation() {
    PairingFsm fsm;
    auto peer = NodeId::generate();

    if (fsm.currentState(peer) != PairingState::Discovered) return 1;
    if (fsm.isTransitionValid(PairingState::Discovered, PairingState::Member)) return 1;
    if (fsm.isTransitionValid(PairingState::Untrusted, PairingState::Registered)) return 1;
    if (fsm.isTransitionValid(PairingState::Trusted, PairingState::Member)) return 1;
    if (!fsm.isTransitionValid(PairingState::Discovered, PairingState::Untrusted)) return 1;
    if (!fsm.isTransitionValid(PairingState::Untrusted, PairingState::Pairing)) return 1;
    if (!fsm.isTransitionValid(PairingState::Pairing, PairingState::Trusted)) return 1;
    if (!fsm.isTransitionValid(PairingState::Trusted, PairingState::Registering)) return 1;
    if (!fsm.isTransitionValid(PairingState::Registering, PairingState::Registered)) return 1;
    if (!fsm.isTransitionValid(PairingState::Registered, PairingState::Member)) return 1;
    return 0;
}

int r7_amend03_topologyAtomicCommit() {
    TopologyVersionManager mgr;
    auto local = NodeId::generate();
    mgr.setLocalNodeId(local);
    mgr.setTopologyAuthority(local);

    TopologyChangeProposal proposal;
    proposal.proposedBy = local;
    proposal.proposedVersion.value = 5;

    if (mgr.commitState() != TopologyCommitState::Active) return 1;
    auto err = mgr.proposeChange(proposal);
    if (err) return 1;
    if (mgr.commitState() != TopologyCommitState::Proposed) return 1;

    err = mgr.activateVersion();
    if (!err) return 1;

    err = mgr.validateProposal();
    if (err) return 1;
    if (mgr.commitState() != TopologyCommitState::Validated) return 1;

    err = mgr.prepareCommit();
    if (err) return 1;
    if (mgr.commitState() != TopologyCommitState::Prepared) return 1;

    err = mgr.activateVersion();
    if (!err) return 1;

    err = mgr.authorizeActivation();
    if (err) return 1;
    if (mgr.commitState() != TopologyCommitState::ActivationAuthorized) return 1;

    err = mgr.activateVersion();
    if (err) return 1;
    if (mgr.commitState() != TopologyCommitState::Active) return 1;
    if (mgr.currentVersion().value != 5) return 1;
    return 0;
}

int r7_amend04_sessionFencing() {
    auto id = makeIdentity(Platform::Mac);
    SessionFenceImpl fence(id.nodeId);

    auto f1 = fence.currentFence();
    auto err = fence.startNewSession();
    if (err) return 1;
    auto f2 = fence.currentFence();
    if (f2.epoch.value <= f1.epoch.value) return 1;

    auto verdict = fence.checkIncoming(id.nodeId, f1.epoch, f1.instanceId);
    if (verdict != SessionFenceVerdict::InstanceConflict) return 1;

    verdict = fence.checkIncoming(id.nodeId, f1.epoch, f2.instanceId);
    if (verdict != SessionFenceVerdict::StaleEpoch) return 1;

    verdict = fence.checkIncoming(id.nodeId, f2.epoch, f2.instanceId);
    if (verdict != SessionFenceVerdict::Accept) return 1;
    return 0;
}

int r7_p1_noSplitBrain() {
    auto idA = makeIdentity(Platform::Mac);
    auto idB = makeIdentity(Platform::Win);

    TopologyVersionManager mgrA, mgrB;
    mgrA.setLocalNodeId(idA.nodeId);
    mgrB.setLocalNodeId(idB.nodeId);
    mgrA.setTopologyAuthority(idA.nodeId);
    mgrB.setTopologyAuthority(idA.nodeId);

    if (!mgrA.isCoordinator()) return 1;
    if (mgrB.isCoordinator()) return 1;

    TopologyChangeProposal proposal;
    proposal.proposedBy = idB.nodeId;
    auto err = mgrB.proposeChange(proposal);
    if (!err) return 1;

    auto auth = NodeId::generate();
    err = mgrA.setTopologyAuthority(auth);
    if (!err) return 1;
    return 0;
}

int r7_p2_noVoidOwner() {
    auto id = makeIdentity(Platform::Mac);
    TopologyVersionManager mgr;
    mgr.setLocalNodeId(id.nodeId);
    mgr.setTopologyAuthority(id.nodeId);

    auto versionBefore = mgr.currentVersion().value;

    mgr.markCoordinatorOffline();
    if (!mgr.isCommitLocked()) return 1;

    auto versionAfter = mgr.currentVersion().value;
    if (versionAfter != versionBefore) return 1;

    TopologyChangeProposal proposal;
    proposal.proposedBy = id.nodeId;
    auto err = mgr.proposeChange(proposal);
    if (!err) return 1;

    if (mgr.currentVersion().value != versionBefore) return 1;
    return 0;
}

int r7_p3_recoverable() {
    NodeIdentityManager mgr;
    auto id = makeIdentity(Platform::Mac);
    mgr.initialize(id);
    mgr.incrementEpoch();

    auto err = mgr.persist();
    if (err) return 1;

    NodeIdentityManager restored;
    restored = NodeIdentityManager("test_p3_recovery.bin");
    restored.initialize(id);
    restored.incrementEpoch();
    err = restored.persist();
    if (err) return 1;

    NodeIdentityManager loaded("test_p3_recovery.bin");
    err = loaded.recoverNodeId();
    if (err) return 1;
    if (loaded.getNodeId() != id.nodeId) return 1;
    if (loaded.currentEpoch().value != 1) return 1;
    return 0;
}

int r7_d_topoAtomic005() {
    auto id = makeIdentity(Platform::Mac);
    TopologyVersionManager mgr;
    mgr.setLocalNodeId(id.nodeId);
    mgr.setTopologyAuthority(id.nodeId);

    TopologyChangeProposal proposal;
    proposal.proposedBy = id.nodeId;
    proposal.proposedVersion.value = 3;

    auto err = mgr.proposeChange(proposal);
    if (err) return 1;
    if (mgr.currentVersion().value != 0) return 1;

    err = mgr.validateProposal();
    if (err) return 1;
    if (mgr.currentVersion().value != 0) return 1;

    err = mgr.prepareCommit();
    if (err) return 1;
    if (mgr.currentVersion().value != 0) return 1;

    err = mgr.authorizeActivation();
    if (err) return 1;
    if (mgr.currentVersion().value != 0) return 1;

    err = mgr.activateVersion();
    if (err) return 1;
    if (mgr.currentVersion().value != 3) return 1;
    return 0;
}

int r7_d_topoCoord005() {
    auto id = makeIdentity(Platform::Mac);
    TopologyVersionManager mgr;
    mgr.setLocalNodeId(id.nodeId);
    mgr.setTopologyAuthority(id.nodeId);

    if (!mgr.isAuthorityConfigured()) return 1;
    auto auth = mgr.authorityNodeId();
    if (!auth || *auth != id.nodeId) return 1;

    auto other = NodeId::generate();
    auto err = mgr.setCoordinator(other);
    if (!err) return 1;

    mgr.markCoordinatorOffline();
    if (mgr.isCoordinator()) return 1;
    if (!mgr.isCommitLocked()) return 1;

    mgr.markCoordinatorOnline();
    if (!mgr.isCoordinator()) return 1;
    if (mgr.isCommitLocked()) return 1;
    return 0;
}

}  // namespace

int main() {
    if (r2_registrationSuccessPath()) { std::puts("FAIL: R2 registrationSuccessPath"); return 1; }
    if (r3_registrationAtomicRollback()) { std::puts("FAIL: R3 registrationAtomicRollback"); return 1; }
    if (r4_sessionAdmission()) { std::puts("FAIL: R4 sessionAdmission"); return 1; }
    if (r5_identityRecoveryPersistence()) { std::puts("FAIL: R5 identityRecoveryPersistence"); return 1; }
    if (r6_topologyMultiNode()) { std::puts("FAIL: R6 topologyMultiNode"); return 1; }
    if (r7_amend01_nodeIdNotPublicKey()) { std::puts("FAIL: R7 AMEND-01 nodeIdNotPublicKey"); return 1; }
    if (r7_amend02_pairingStateSeparation()) { std::puts("FAIL: R7 AMEND-02 pairingStateSeparation"); return 1; }
    if (r7_amend03_topologyAtomicCommit()) { std::puts("FAIL: R7 AMEND-03 topologyAtomicCommit"); return 1; }
    if (r7_amend04_sessionFencing()) { std::puts("FAIL: R7 AMEND-04 sessionFencing"); return 1; }
    if (r7_p1_noSplitBrain()) { std::puts("FAIL: R7 P1 noSplitBrain"); return 1; }
    if (r7_p2_noVoidOwner()) { std::puts("FAIL: R7 P2 noVoidOwner"); return 1; }
    if (r7_p3_recoverable()) { std::puts("FAIL: R7 P3 recoverable"); return 1; }
    if (r7_d_topoAtomic005()) { std::puts("FAIL: R7 D-TOPO-ATOMIC-005"); return 1; }
    if (r7_d_topoCoord005()) { std::puts("FAIL: R7 D-TOPO-COORD-005"); return 1; }
    std::puts("ALL DESIGN CONTRACT TESTS PASS");
    return 0;
}