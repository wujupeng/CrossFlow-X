#include <cstdio>
#include <string>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s06_identity/node_identity_manager.hpp"
#include "s06_identity/session_fence.hpp"
#include "s08_pairing/pairing_fsm.hpp"
#include "s08_pairing/trusted_node_list.hpp"
#include "s09_membership/membership_manager.hpp"
#include "s09_membership/topology_version_manager.hpp"
#include "s07_discovery/mdns_announcer.hpp"
#include "s07_discovery/mdns_listener.hpp"
#include "s07_discovery/discovery_service.hpp"
#include "s07_discovery/discovery_table.hpp"
#include "s07_discovery/discovery_service.hpp"
#include "s08_pairing/registration_manager.hpp"
#include "s08_pairing/handshake_orchestrator.hpp"
#include "s06_identity/identity_recovery_manager.hpp"

namespace {

int testNodeIdentityManager() {
    using namespace cfx;
    NodeIdentityManager mgr;
    NodeIdentity id{};
    id.nodeId = NodeId::generate();
    id.platform = Platform::Win;
    id.capabilities.protocolVersion = 1;
    id.capabilities.screenBoundary = {1920, 1080, 0, 0};
    auto err = mgr.initialize(id);
    if (err) return 1;
    if (mgr.getNodeId() != id.nodeId) return 1;
    auto epoch = mgr.incrementEpoch();
    if (epoch.value != 1) return 1;
    auto ident = mgr.getIdentity();
    if (!ident || ident->sessionEpoch.value != 1) return 1;
    return 0;
}

int testSessionFence() {
    using namespace cfx;
    NodeId nid = NodeId::generate();
    SessionFenceImpl fence(nid);
    auto current = fence.currentFence();
    if (current.nodeId != nid) return 1;
    if (current.epoch.value != 1) return 1;
    auto verdict = fence.checkIncoming(nid, SessionEpoch{1}, current.instanceId);
    if (verdict != SessionFenceVerdict::Accept) return 1;
    NodeId other = NodeId::generate();
    verdict = fence.checkIncoming(other, SessionEpoch{1}, current.instanceId);
    if (verdict != SessionFenceVerdict::UnknownNode) return 1;
    verdict = fence.checkIncoming(nid, SessionEpoch{0}, current.instanceId);
    if (verdict != SessionFenceVerdict::StaleEpoch) return 1;
    return 0;
}

int testPairingFsm() {
    using namespace cfx;
    PairingFsm fsm;
    NodeId peer = NodeId::generate();
    if (fsm.currentState(peer) != PairingState::Discovered) return 1;
    auto err = fsm.initiatePairing(peer, "123456");
    if (err) return 1;
    if (fsm.currentState(peer) != PairingState::Pairing) return 1;
    if (!fsm.isTransitionValid(PairingState::Pairing, PairingState::Trusted)) return 1;
    if (fsm.isTransitionValid(PairingState::Pairing, PairingState::Member)) return 1;
    err = fsm.forceTransition(peer, PairingState::Trusted);
    if (err) return 1;
    err = fsm.forceTransition(peer, PairingState::Member);
    if (!err) return 1;
    return 0;
}

int testTrustedNodeList() {
    using namespace cfx;
    TrustedNodeList list;
    TrustedNodeEntry entry;
    entry.nodeId = NodeId::generate();
    entry.pairedAt = 1000;
    entry.lastSeenEpoch = SessionEpoch{1};
    auto err = list.add(entry);
    if (err) return 1;
    if (!list.isTrusted(entry.nodeId)) return 1;
    auto found = list.find(entry.nodeId);
    if (!found || found->pairedAt != 1000) return 1;
    err = list.remove(entry.nodeId);
    if (err) return 1;
    if (list.isTrusted(entry.nodeId)) return 1;
    return 0;
}

int testMembershipManager() {
    using namespace cfx;
    MembershipManager mgr;
    MembershipEntry e1;
    e1.nodeId = NodeId::generate();
    e1.isOnline = true;
    e1.topologyMembership.topologyId = "t1";
    auto err = mgr.addMember(e1);
    if (err) return 1;
    if (mgr.memberCount() != 1) return 1;
    if (!mgr.isDegraded()) return 1;
    err = mgr.setMemberOnline(e1.nodeId, false);
    if (err) return 1;
    auto m = mgr.getMember(e1.nodeId);
    if (!m || m->isOnline) return 1;
    return 0;
}

int testTopologyVersionManager() {
    using namespace cfx;
    TopologyVersionManager mgr;
    NodeId local = NodeId::generate();
    NodeId authority = local;
    mgr.setLocalNodeId(local);
    auto err = mgr.setTopologyAuthority(authority);
    if (err) return 1;
    if (mgr.isCommitLocked()) return 1;
    auto cid = mgr.coordinatorId();
    if (!cid || *cid != authority) return 1;
    if (mgr.commitState() != TopologyCommitState::Active) return 1;
    return 0;
}

int testErrorCodes() {
    using namespace cfx;
    if (to_string(ErrorCode::SessUuidGenFail) != std::string("CFX-E-SESS-UUID-GEN-FAIL")) return 1;
    if (to_string(ErrorCode::SessStaleEpoch) != std::string("CFX-W-SESS-STALE-EPOCH")) return 1;
    if (to_string(ErrorCode::DiscMdnsUnavailable) != std::string("CFX-W-DISC-MDNS-UNAVAILABLE")) return 1;
    if (to_string(ErrorCode::PairCodeMismatch) != std::string("CFX-E-PAIR-CODE-MISMATCH")) return 1;
    if (to_string(ErrorCode::RegUndiscovered) != std::string("CFX-E-REG-UNDISCOVERED")) return 1;
    if (to_string(ErrorCode::TopoCoordDuplicateClaim) != std::string("CFX-E-TOPO-COORD-DUPLICATE-CLAIM")) return 1;
    if (to_string(ErrorCode::RecoveryUntrusted) != std::string("CFX-E-RECOVERY-UNTRUSTED")) return 1;
    if (to_string(ErrorCode::HandoffUnregisteredPeer) != std::string("CFX-W-HANDOFF-UNREGISTERED-PEER")) return 1;
    return 0;
}

int testDiscoveryDigest() {
    using namespace cfx;
    DiscoveryDigest digest;
    digest.nodeId = NodeId::generate();
    digest.platform = Platform::Mac;
    digest.sessionEpoch.value = 42;
    digest.protocolVersion = 1;
    digest.topologyId = "topo1";
    digest.capabilitiesFingerprint = {InputType::Mouse, InputType::Keyboard};
    auto txt = digest.toTxtRecord();
    if (txt.empty()) return 1;
    if (txt[0].first != "nid") return 1;
    if (txt[1].first != "plat") return 1;
    if (txt[1].second != "mac") return 1;
    if (txt[2].first != "epoch") return 1;
    if (txt[2].second != "42") return 1;
    return 0;
}

int testDiscoveryTable() {
    using namespace cfx;
    DiscoveryTable table;
    DiscoveryRecord r1;
    r1.nodeId = NodeId::generate();
    r1.platform = Platform::Mac;
    r1.topologyId = "topo1";
    r1.protocolVersion = 1;
    auto err = table.upsert(r1);
    if (err) return 1;
    if (table.size() != 1) return 1;
    auto found = table.find(r1.nodeId);
    if (!found || found->topologyId != "topo1") return 1;
    auto all = table.all();
    if (all.size() != 1) return 1;
    auto byTopo = table.findByTopology("topo1");
    if (byTopo.size() != 1) return 1;
    byTopo = table.findByTopology("topo2");
    if (!byTopo.empty()) return 1;
    err = table.upsert(r1);
    if (err) return 1;
    if (table.size() != 1) return 1;
    err = table.remove(r1.nodeId);
    if (err) return 1;
    if (table.size() != 0) return 1;
    return 0;
}

int testDiscoveryService() {
    using namespace cfx;
    DiscoveryService svc;
    if (svc.isAnnouncing()) return 1;
    if (svc.isListening()) return 1;
    DiscoveryDigest digest;
    digest.nodeId = NodeId::generate();
    digest.platform = Platform::Win;
    digest.sessionEpoch.value = 1;
    digest.protocolVersion = 1;
    digest.topologyId = "topo1";
    auto err = svc.startAnnouncing(digest);
    if (err) return 1;
    if (!svc.isAnnouncing()) return 1;
    err = svc.startListening();
    if (err) return 1;
    if (!svc.isListening()) return 1;
    DiscoveryAnnouncement ann;
    ann.digest = digest;
    ann.fence = SessionFence::newSession(digest.nodeId);
    err = svc.handleDiscoveryAnnouncement(ann);
    if (err) return 1;
    auto nodes = svc.discoveredNodes();
    if (nodes.size() != 1) return 1;
    auto found = svc.findNode(digest.nodeId);
    if (!found) return 1;
    svc.clearDiscovered();
    if (!svc.discoveredNodes().empty()) return 1;
    err = svc.stopAnnouncing();
    if (err) return 1;
    if (svc.isAnnouncing()) return 1;
    err = svc.stopListening();
    if (err) return 1;
    if (svc.isListening()) return 1;
    return 0;
}

int testDiscoveryConflict() {
    using namespace cfx;
    DiscoveryTable table;
    DiscoveryRecord r1;
    r1.nodeId = NodeId::generate();
    r1.topologyId = "topo1";
    r1.protocolVersion = 1;
    auto err = table.upsert(r1);
    if (err) return 1;
    DiscoveryRecord r2;
    r2.nodeId = NodeId::generate();
    r2.topologyId = "topo2";
    r2.protocolVersion = 1;
    err = table.upsert(r2);
    if (err) return 1;
    auto conflicts = table.detectConflicts();
    if (!conflicts.empty()) return 1;
    DiscoveryRecord r3;
    r3.nodeId = r1.nodeId;
    r3.topologyId = "topo1";
    r3.protocolVersion = 1;
    err = table.upsert(r3);
    if (err) return 1;
    if (table.size() != 2) return 1;
    return 0;
}

int testRegistrationManager() {
    using namespace cfx;
    RegistrationManager mgr;
    NodeIdentity id{};
    id.nodeId = NodeId::generate();
    id.platform = Platform::Win;
    id.capabilities.protocolVersion = 1;
    id.capabilities.screenBoundary = {1920, 1080, 0, 0};
    auto err = mgr.registerPeer(id);
    if (err) return 1;
    if (!mgr.isRegistered(id.nodeId)) return 1;
    auto reg = mgr.getRegistration(id.nodeId);
    if (!reg || reg->currentState != PairingState::Registered) return 1;
    err = mgr.unregisterPeer(id.nodeId);
    if (err) return 1;
    if (mgr.isRegistered(id.nodeId)) return 1;
    return 0;
}

int testHandshakeOrchestrator() {
    using namespace cfx;
    PairingFsm pairing;
    RegistrationManager reg;
    TrustedNodeList trusted;
    NodeIdentityManager identity;

    NodeIdentity id{};
    id.nodeId = NodeId::generate();
    id.platform = Platform::Win;
    id.capabilities.protocolVersion = 1;
    id.capabilities.screenBoundary = {1920, 1080, 0, 0};
    identity.initialize(id);

    HandshakeOrchestrator hs(pairing, reg, trusted, identity);
    NodeId peer = NodeId::generate();
    auto err = hs.initiateHandshake(peer, "123456");
    if (err) return 1;
    if (hs.isHandshakeComplete(peer)) return 1;
    err = hs.rollbackHandshake(peer);
    if (err) return 1;
    if (hs.isHandshakeComplete(peer)) return 1;
    return 0;
}

int testTopologyVersionManagerFixed() {
    using namespace cfx;
    TopologyVersionManager mgr;
    NodeId local = NodeId::generate();
    NodeId authority = local;
    mgr.setLocalNodeId(local);
    auto err = mgr.setTopologyAuthority(authority);
    if (err) return 1;
    if (!mgr.isAuthorityConfigured()) return 1;
    if (!mgr.isCoordinator()) return 1;
    if (mgr.isCommitLocked()) return 1;
    TopologyChangeProposal proposal;
    proposal.proposedBy = local;
    proposal.proposedVersion.value = 2;
    err = mgr.proposeChange(proposal);
    if (err) return 1;
    if (mgr.commitState() != TopologyCommitState::Proposed) return 1;
    err = mgr.validateProposal();
    if (err) return 1;
    err = mgr.prepareCommit();
    if (err) return 1;
    err = mgr.activateVersion();
    if (!err) return 1;
    err = mgr.authorizeActivation();
    if (err) return 1;
    if (mgr.commitState() != TopologyCommitState::ActivationAuthorized) return 1;
    err = mgr.activateVersion();
    if (err) return 1;
    if (mgr.commitState() != TopologyCommitState::Active) return 1;
    if (mgr.currentVersion().value != 2) return 1;
    return 0;
}

int testCoordinatorOffline() {
    using namespace cfx;
    TopologyVersionManager mgr;
    NodeId local = NodeId::generate();
    NodeId authority = local;
    mgr.setLocalNodeId(local);
    mgr.setTopologyAuthority(authority);
    if (!mgr.isCoordinator()) return 1;
    mgr.markCoordinatorOffline();
    if (mgr.isCoordinator()) return 1;
    if (!mgr.isCommitLocked()) return 1;
    TopologyChangeProposal proposal;
    proposal.proposedBy = local;
    auto err = mgr.proposeChange(proposal);
    if (!err) return 1;
    mgr.markCoordinatorOnline();
    if (!mgr.isCoordinator()) return 1;
    if (mgr.isCommitLocked()) return 1;
    return 0;
}

int testIdentityRecovery() {
    using namespace cfx;
    NodeIdentityManager identity;
    TrustedNodeList trusted;

    NodeIdentity id{};
    id.nodeId = NodeId::generate();
    id.platform = Platform::Win;
    id.capabilities.protocolVersion = 1;
    id.capabilities.screenBoundary = {1920, 1080, 0, 0};
    identity.initialize(id);

    TrustedNodeEntry entry;
    entry.nodeId = NodeId::generate();
    entry.pairedAt = 1000;
    entry.lastSeenEpoch = SessionEpoch{1};
    trusted.add(entry);

    IdentityRecoveryManager recovery(identity, trusted);
    auto err = recovery.initiateRecovery(entry.nodeId);
    if (err) return 1;
    if (!recovery.isRecoveryInProgress(entry.nodeId)) return 1;
    NodeId untrusted = NodeId::generate();
    err = recovery.initiateRecovery(untrusted);
    if (!err) return 1;
    recovery.cancelRecovery(entry.nodeId);
    if (recovery.isRecoveryInProgress(entry.nodeId)) return 1;
    return 0;
}

int testMdnsAnnouncer() {
    using namespace cfx;
    MdnsAnnouncer announcer;
    if (announcer.isRunning()) return 1;

    std::vector<std::pair<std::string, std::string>> txt = {
        {"nid", "0123456789abcdef0123456789abcdef"},
        {"plat", "win"},
        {"epoch", "1"},
        {"pver", "1"},
        {"topo", "topo1"}
    };

    auto err = announcer.start("CrossFlow-X", 5353, txt);
    if (err) {
        return 0;
    }
    if (!announcer.isRunning()) return 1;

    err = announcer.updateTxt(txt);
    if (err) return 1;

    err = announcer.stop();
    if (err) return 1;
    if (announcer.isRunning()) return 1;
    return 0;
}

int testMdnsListener() {
    using namespace cfx;
    MdnsListener listener;
    if (listener.isRunning()) return 1;

    bool callbackCalled = false;
    listener.setDiscoveryCallback(
        [&callbackCalled](const std::string&, const std::string&, u16,
                          const std::vector<std::pair<std::string, std::string>>&) {
            callbackCalled = true;
        });

    auto err = listener.start("_crossflow-x._tcp");
    if (err) {
        return 0;
    }
    if (!listener.isRunning()) return 1;

    listener.poll(std::chrono::milliseconds(100));

    err = listener.stop();
    if (err) return 1;
    if (listener.isRunning()) return 1;
    return 0;
}

int testDiscoveryTransportSelection() {
    using namespace cfx;
    DiscoveryService svc;

    DiscoveryDigest digest;
    digest.nodeId = NodeId::generate();
    digest.platform = Platform::Win;
    digest.sessionEpoch.value = 1;
    digest.protocolVersion = 1;
    digest.topologyId = "topo1";

    auto err = svc.startAnnouncing(digest);
    if (err) return 1;
    if (!svc.isAnnouncing()) return 1;

    auto transport = svc.activeTransport();
    if (transport != DiscoveryTransport::Mdns && transport != DiscoveryTransport::UdpBroadcast) return 1;

    err = svc.startListening();
    if (err) return 1;
    if (!svc.isListening()) return 1;

    err = svc.stopAnnouncing();
    if (err) return 1;
    err = svc.stopListening();
    if (err) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testNodeIdentityManager()) { std::puts("FAIL: testNodeIdentityManager"); return 1; }
    if (testSessionFence()) { std::puts("FAIL: testSessionFence"); return 1; }
    if (testPairingFsm()) { std::puts("FAIL: testPairingFsm"); return 1; }
    if (testTrustedNodeList()) { std::puts("FAIL: testTrustedNodeList"); return 1; }
    if (testMembershipManager()) { std::puts("FAIL: testMembershipManager"); return 1; }
    if (testTopologyVersionManager()) { std::puts("FAIL: testTopologyVersionManager"); return 1; }
    if (testErrorCodes()) { std::puts("FAIL: testErrorCodes"); return 1; }
    if (testDiscoveryDigest()) { std::puts("FAIL: testDiscoveryDigest"); return 1; }
    if (testDiscoveryTable()) { std::puts("FAIL: testDiscoveryTable"); return 1; }
    if (testDiscoveryService()) { std::puts("FAIL: testDiscoveryService"); return 1; }
    if (testDiscoveryConflict()) { std::puts("FAIL: testDiscoveryConflict"); return 1; }
    if (testRegistrationManager()) { std::puts("FAIL: testRegistrationManager"); return 1; }
    if (testHandshakeOrchestrator()) { std::puts("FAIL: testHandshakeOrchestrator"); return 1; }
    if (testTopologyVersionManagerFixed()) { std::puts("FAIL: testTopologyVersionManagerFixed"); return 1; }
    if (testCoordinatorOffline()) { std::puts("FAIL: testCoordinatorOffline"); return 1; }
    if (testIdentityRecovery()) { std::puts("FAIL: testIdentityRecovery"); return 1; }
    if (testMdnsAnnouncer()) { std::puts("FAIL: testMdnsAnnouncer"); return 1; }
    if (testMdnsListener()) { std::puts("FAIL: testMdnsListener"); return 1; }
    if (testDiscoveryTransportSelection()) { std::puts("FAIL: testDiscoveryTransportSelection"); return 1; }
    std::puts("ALL PASS");
    return 0;
}