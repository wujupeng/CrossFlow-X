#include <cstdio>
#include <fstream>
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
#include "s07_discovery/manual_config_fallback.hpp"
#include "s07_discovery/discovery_service.hpp"
#include "s07_discovery/discovery_table.hpp"
#include "s07_discovery/discovery_service.hpp"
#include "s07_discovery/network_change_event_source.hpp"
#include "s08_pairing/registration_manager.hpp"
#include "s08_pairing/handshake_orchestrator.hpp"
#include "s06_identity/identity_recovery_manager.hpp"
#include "s09_membership/topology_change_event_source.hpp"

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

    auto announceDuration = announcer.lastAnnounceDuration();
    if (announceDuration.count() < 0) return 1;

    err = announcer.updateTxt(txt);
    if (err) return 1;

    err = announcer.republish();
    if (err) return 1;

    announcer.startAutoRefresh(std::chrono::seconds(30));
    announcer.stopAutoRefresh();

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

int testMdnsRoundTripDiscovery() {
    using namespace cfx;
    MdnsAnnouncer announcer;

    std::vector<std::pair<std::string, std::string>> txt = {
        {"nid", "fedcba9876543210fedcba9876543210"},
        {"plat", "win"},
        {"epoch", "1"},
        {"pver", "1"},
        {"topo", "roundtrip"}
    };

    auto announceErr = announcer.start("CrossFlow-X-RT", 5353, txt);
    if (announceErr) return 0;

    auto announceDuration = announcer.lastAnnounceDuration();

    MdnsListener listener;
    bool discovered = false;
    std::string discoveredService;
    listener.setDiscoveryCallback(
        [&discovered, &discoveredService](const std::string& serviceName,
                   const std::string&, u16,
                   const std::vector<std::pair<std::string, std::string>>&) {
            discovered = true;
            discoveredService = serviceName;
        });

    auto listenErr = listener.start("_crossflow-x._tcp");
    if (listenErr) {
        announcer.stop();
        return 0;
    }

    auto discoveryStart = std::chrono::steady_clock::now();
    for (int i = 0; i < 10 && !discovered; ++i) {
        listener.poll(std::chrono::milliseconds(100));
    }
    auto discoveryDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - discoveryStart);

    listener.stop();
    announcer.stop();

    if (announceDuration.count() > 500) return 1;
    if (discovered && discoveryDuration.count() > 1000) return 1;

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
    if (transport != DiscoveryTransport::Mdns && transport != DiscoveryTransport::UdpBroadcast && transport != DiscoveryTransport::ManualConfig) return 1;

    err = svc.startListening();
    if (err) return 1;
    if (!svc.isListening()) return 1;

    err = svc.stopAnnouncing();
    if (err) return 1;
    err = svc.stopListening();
    if (err) return 1;
    return 0;
}

int testManualConfigFallback() {
    using namespace cfx;
    ManualConfigFallback fallback;
    if (fallback.isRunning()) return 1;

    std::vector<ManualEndpoint> endpoints;
    ManualEndpoint ep1;
    ep1.host = "192.168.1.100";
    ep1.port = 5353;
    ep1.nodeId = NodeId::generate();
    endpoints.push_back(ep1);

    ManualEndpoint ep2;
    ep2.host = "192.168.1.101";
    ep2.port = 5353;
    ep2.nodeId = NodeId::generate();
    endpoints.push_back(ep2);

    auto err = fallback.loadFromList(endpoints);
    if (err) return 1;
    if (fallback.configuredEndpoints().size() != 2) return 1;

    err = fallback.start();
    if (err) return 1;
    if (!fallback.isRunning()) return 1;

    auto found = fallback.findByNodeId(ep1.nodeId);
    if (!found) return 1;
    if (found->host != "192.168.1.100") return 1;

    auto notFound = fallback.findByNodeId(NodeId::generate());
    if (notFound) return 1;

    err = fallback.stop();
    if (err) return 1;
    if (fallback.isRunning()) return 1;

    ManualConfigFallback empty;
    err = empty.start();
    if (!err) return 1;
    return 0;
}

int testRecoverFromCorruption() {
    using namespace cfx;

    std::remove("test_recovery_corruption.bin");
    std::remove("test_trusted_recovery.bin");
    std::remove("nonexistent_file.bin");
    std::remove("nonexistent_trusted.bin");
    std::remove("test_corrupt_nodeid.bin");
    std::remove("test_corrupt_epoch.bin");
    std::remove("test_corrupt_trusted.bin");
    std::remove("test_corrupt_topology.bin");
    std::remove("test_topology_roundtrip.bin");

    NodeIdentityManager mgr("test_recovery_corruption.bin");
    NodeIdentity id{};
    id.nodeId = NodeId::generate();
    id.platform = Platform::Win;
    id.capabilities.protocolVersion = 1;
    id.capabilities.screenBoundary = {1920, 1080, 0, 0};
    mgr.initialize(id);
    mgr.incrementEpoch();
    mgr.persist();

    NodeIdentityManager restored("test_recovery_corruption.bin");
    auto err = restored.recoverFromCorruption();
    if (err) return 1;
    if (restored.getNodeId() != id.nodeId) return 1;
    if (restored.currentEpoch().value != 1) return 1;

    const auto& result = restored.lastRecoveryResult();
    if (result.nodeIdRegenerated) return 1;
    if (result.epochReset) return 1;

    NodeIdentityManager corrupt("nonexistent_file.bin");
    err = corrupt.recoverFromCorruption();
    if (err) return 1;
    if (!corrupt.lastRecoveryResult().nodeIdRegenerated) return 1;
    if (corrupt.getNodeId().isNull()) return 1;

    {
        std::ofstream ofs("test_corrupt_nodeid.bin", std::ios::trunc);
        ofs << "0 0\n1\n0\n";
    }
    NodeIdentityManager corruptNid("test_corrupt_nodeid.bin");
    err = corruptNid.recoverFromCorruption();
    if (err) return 1;
    if (!corruptNid.lastRecoveryResult().nodeIdRegenerated) return 1;
    if (corruptNid.getNodeId().isNull()) return 1;

    {
        std::ofstream ofs("test_corrupt_epoch.bin", std::ios::trunc);
        auto nid = NodeId::generate();
        ofs << nid.high << ' ' << nid.low << "\n0\n0\n";
    }
    NodeIdentityManager corruptEpoch("test_corrupt_epoch.bin");
    err = corruptEpoch.recoverFromCorruption();
    if (err) return 1;
    if (!corruptEpoch.lastRecoveryResult().epochReset) return 1;
    if (corruptEpoch.currentEpoch().value != 1) return 1;

    {
        std::ofstream ofs("test_corrupt_trusted.bin", std::ios::trunc);
        ofs << "0 0 1000 1\n";
    }
    TrustedNodeList corruptTrustedFile("test_corrupt_trusted.bin");
    err = corruptTrustedFile.recoverFromCorruption();
    if (err) return 1;
    if (!corruptTrustedFile.lastRecoveryResult().listCleared) return 1;
    if (!corruptTrustedFile.all().empty()) return 1;

    TrustedNodeList trusted("test_trusted_recovery.bin");
    TrustedNodeEntry entry;
    entry.nodeId = NodeId::generate();
    entry.pairedAt = 1000;
    entry.lastSeenEpoch = SessionEpoch{1};
    trusted.add(entry);
    trusted.persist();

    TrustedNodeList restoredTrusted("test_trusted_recovery.bin");
    err = restoredTrusted.recoverFromCorruption();
    if (err) return 1;
    if (!restoredTrusted.isTrusted(entry.nodeId)) return 1;

    TrustedNodeList corruptTrusted("nonexistent_trusted.bin");
    err = corruptTrusted.recoverFromCorruption();
    if (err) return 1;
    if (!corruptTrusted.lastRecoveryResult().listCleared) return 1;
    if (!corruptTrusted.all().empty()) return 1;

    std::remove("test_corrupt_topology.bin");
    std::remove("test_topology_roundtrip.bin");

    {
        std::ofstream ofs("test_corrupt_topology.bin", std::ios::trunc);
        auto nid = NodeId::generate();
        ofs << nid.high << ' ' << nid.low << "\n1\n1\n";
    }
    NodeIdentityManager corruptTopology("test_corrupt_topology.bin");
    err = corruptTopology.recoverFromCorruption();
    if (err) return 1;
    if (!corruptTopology.lastRecoveryResult().topologyCleared) return 1;

    {
        NodeIdentityManager topoMgr("test_topology_roundtrip.bin");
        NodeIdentity topoId{};
        topoId.nodeId = NodeId::generate();
        topoId.platform = Platform::Win;
        topoId.capabilities.protocolVersion = 1;
        topoId.capabilities.screenBoundary = {1920, 1080, 0, 0};
        topoMgr.initialize(topoId);
        topoMgr.incrementEpoch();
        TopologyMembership tm;
        tm.topologyId = "topo_test";
        tm.segmentIndex = 2;
        topoMgr.joinTopology(tm);
        topoMgr.persist();
    }
    {
        NodeIdentityManager restoredTopo("test_topology_roundtrip.bin");
        err = restoredTopo.recoverFromCorruption();
        if (err) return 1;
        auto identity = restoredTopo.getIdentity();
        if (!identity) return 1;
        if (!identity->topologyMembership.has_value()) return 1;
        if (identity->topologyMembership->topologyId != "topo_test") return 1;
        if (identity->topologyMembership->segmentIndex != 2) return 1;
        if (restoredTopo.lastRecoveryResult().topologyCleared) return 1;
    }

    return 0;
}

int testDiscoveryManualFallbackIntegration() {
    using namespace cfx;
    DiscoveryService svc;

    std::vector<ManualEndpoint> endpoints;
    ManualEndpoint ep;
    ep.host = "192.168.1.200";
    ep.port = 5353;
    ep.nodeId = NodeId::generate();
    endpoints.push_back(ep);

    svc.setManualConfigEndpoints(endpoints);

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
    if (transport != DiscoveryTransport::Mdns &&
        transport != DiscoveryTransport::UdpBroadcast &&
        transport != DiscoveryTransport::ManualConfig) return 1;

    err = svc.startListening();
    if (err) return 1;
    if (!svc.isListening()) return 1;

    err = svc.stopListening();
    if (err) return 1;
    err = svc.stopAnnouncing();
    if (err) return 1;
    return 0;
}

int testNetworkChangeEventSource() {
    using namespace cfx;
    NetworkChangeEventSource source;
    auto err = source.start();
    if (err) return 1;
    if (!source.isRunning()) return 1;

    bool callbackCalled = false;
    NetworkChangeEvent receivedEvent{};
    source.setCallback([&](const NetworkChangeEvent& evt) {
        callbackCalled = true;
        receivedEvent = evt;
    });

    NetworkChangeEvent testEvent{};
    testEvent.type = NetworkChangeType::IpAddressAdded;
    testEvent.interfaceName = "eth0";
    testEvent.ipAddress = "192.168.1.100";
    source.simulateNetworkChange(testEvent);

    if (!callbackCalled) return 1;
    if (receivedEvent.type != NetworkChangeType::IpAddressAdded) return 1;
    if (receivedEvent.interfaceName != "eth0") return 1;
    if (receivedEvent.ipAddress != "192.168.1.100") return 1;

    err = source.stop();
    if (err) return 1;
    if (source.isRunning()) return 1;

    callbackCalled = false;
    source.simulateNetworkChange(testEvent);
    if (callbackCalled) return 1;

    return 0;
}

int testTopologyChangeEventSource() {
    using namespace cfx;
    TopologyChangeEventSource source;

    bool callbackCalled = false;
    TopologyChangeEvent receivedEvent{};
    source.setCallback([&](const TopologyChangeEvent& evt) {
        callbackCalled = true;
        receivedEvent = evt;
    });

    TopologyVersion oldVer{};
    oldVer.value = 1;
    oldVer.topologyId = "topo-v1";

    TopologyVersion newVer{};
    newVer.value = 2;
    newVer.topologyId = "topo-v2";

    NodeId changedBy{};
    changedBy.high = 100;
    changedBy.low = 200;

    source.notifyTopologyChanged(oldVer, newVer, changedBy);

    if (!callbackCalled) return 1;
    if (receivedEvent.oldVersion.value != 1) return 1;
    if (receivedEvent.newVersion.value != 2) return 1;
    if (receivedEvent.newVersion.topologyId != "topo-v2") return 1;
    if (receivedEvent.changedBy.high != 100) return 1;

    return 0;
}

int testAutoRepublishOnNetworkChange() {
    using namespace cfx;
    NetworkChangeEventSource netSource;

    auto netErr = netSource.start();
    if (netErr) return 1;

    u32 callbackCount = 0;
    netSource.setCallback([&](const NetworkChangeEvent& evt) {
        if (evt.type == NetworkChangeType::IpAddressAdded) callbackCount++;
    });

    NetworkChangeEvent ipChange{};
    ipChange.type = NetworkChangeType::IpAddressAdded;
    ipChange.interfaceName = "eth0";
    ipChange.ipAddress = "10.0.0.5";
    netSource.simulateNetworkChange(ipChange);
    if (callbackCount != 1) return 1;

    netSource.simulateNetworkChange(ipChange);
    if (callbackCount != 2) return 1;

    netSource.setCallback(nullptr);
    netSource.simulateNetworkChange(ipChange);
    if (callbackCount != 2) return 1;

    MdnsAnnouncer announcer;
    std::vector<std::pair<std::string, std::string>> txt;
    txt.push_back({"node", "test-auto-republish"});
    txt.push_back({"topo", "topo-1"});

    auto err = announcer.start("test-auto-republish", 5353, txt);
    if (!err) {
        announcer.subscribeToNetworkChanges(netSource);
        u32 countBefore = announcer.networkChangeRepublishCount();

        NetworkChangeEvent ipChange2{};
        ipChange2.type = NetworkChangeType::IpAddressAdded;
        ipChange2.interfaceName = "eth1";
        ipChange2.ipAddress = "10.0.0.6";
        netSource.simulateNetworkChange(ipChange2);

        u32 countAfter = announcer.networkChangeRepublishCount();
        if (countAfter <= countBefore) return 1;

        announcer.unsubscribeFromNetworkChanges();
        u32 countAfterUnsub = announcer.networkChangeRepublishCount();
        netSource.simulateNetworkChange(ipChange2);
        if (announcer.networkChangeRepublishCount() != countAfterUnsub) return 1;

        announcer.stop();
    }

    netSource.stop();
    return 0;
}

int testAutoRepublishOnTopologyChange() {
    using namespace cfx;
    TopologyChangeEventSource topoSource;

    u32 callbackCount = 0;
    topoSource.setCallback([&](const TopologyChangeEvent& evt) {
        if (evt.newVersion.value > evt.oldVersion.value) callbackCount++;
    });

    TopologyVersion oldVer{};
    oldVer.value = 1;
    TopologyVersion newVer{};
    newVer.value = 2;
    NodeId changedBy{};
    changedBy.high = 1;
    changedBy.low = 2;

    topoSource.notifyTopologyChanged(oldVer, newVer, changedBy);
    if (callbackCount != 1) return 1;

    topoSource.notifyTopologyChanged(oldVer, newVer, changedBy);
    if (callbackCount != 2) return 1;

    topoSource.setCallback(nullptr);
    topoSource.notifyTopologyChanged(oldVer, newVer, changedBy);
    if (callbackCount != 2) return 1;

    MdnsAnnouncer announcer;
    std::vector<std::pair<std::string, std::string>> txt;
    txt.push_back({"node", "test-topo-republish"});
    txt.push_back({"topo", "topo-1"});

    auto err = announcer.start("test-topo-republish", 5353, txt);
    if (!err) {
        announcer.subscribeToTopologyChanges(topoSource);
        u32 countBefore = announcer.topologyChangeRepublishCount();

        topoSource.notifyTopologyChanged(oldVer, newVer, changedBy);

        u32 countAfter = announcer.topologyChangeRepublishCount();
        if (countAfter <= countBefore) return 1;

        announcer.unsubscribeFromTopologyChanges();
        u32 countAfterUnsub = announcer.topologyChangeRepublishCount();
        topoSource.notifyTopologyChanged(oldVer, newVer, changedBy);
        if (announcer.topologyChangeRepublishCount() != countAfterUnsub) return 1;

        announcer.stop();
    }

    return 0;
}

int testLifecycleUnsubscribeNoUseAfterFree() {
    using namespace cfx;
    NetworkChangeEventSource netSource;
    auto netErr = netSource.start();
    if (netErr) return 1;

    u32 callbackCount = 0;

    {
        MdnsAnnouncer announcer;
        std::vector<std::pair<std::string, std::string>> txt;
        txt.push_back({"node", "test-lifecycle"});
        auto err = announcer.start("test-lifecycle", 5353, txt);
        if (!err) {
            announcer.subscribeToNetworkChanges(netSource);
            netSource.setCallback([&](const NetworkChangeEvent&) {
                callbackCount++;
            });
            announcer.unsubscribeFromNetworkChanges();
            announcer.stop();
        }
    }

    NetworkChangeEvent ipChange{};
    ipChange.type = NetworkChangeType::IpAddressAdded;
    ipChange.interfaceName = "eth0";
    ipChange.ipAddress = "10.0.0.99";
    netSource.simulateNetworkChange(ipChange);

    netSource.stop();
    return 0;
}

int testDiscoveryTableDuplicateStaleConflict() {
    using namespace cfx;
    DiscoveryTable table;

    DiscoveryRecord record1{};
    record1.nodeId.high = 1;
    record1.nodeId.low = 100;
    record1.platform = Platform::Win;
    record1.sessionEpoch.value = 5;
    record1.protocolVersion = 1;
    record1.topologyId = "topo-A";

    auto [err1, result1] = table.upsertWithResult(record1);
    if (err1) return 1;
    if (result1 != DiscoveryUpdateResult::Inserted) return 1;

    auto [err2, result2] = table.upsertWithResult(record1);
    if (err2) return 1;
    if (result2 != DiscoveryUpdateResult::DuplicateIgnored) return 1;
    if (table.duplicateCount() != 1) return 1;

    DiscoveryRecord recordStale{};
    recordStale.nodeId = record1.nodeId;
    recordStale.platform = Platform::Win;
    recordStale.sessionEpoch.value = 3;
    recordStale.protocolVersion = 1;
    recordStale.topologyId = "topo-A";

    auto [err3, result3] = table.upsertWithResult(recordStale);
    if (err3) return 1;
    if (result3 != DiscoveryUpdateResult::StaleIgnored) return 1;
    if (table.staleCount() != 1) return 1;

    DiscoveryRecord recordNewer{};
    recordNewer.nodeId = record1.nodeId;
    recordNewer.platform = Platform::Mac;
    recordNewer.sessionEpoch.value = 10;
    recordNewer.protocolVersion = 1;
    recordNewer.topologyId = "topo-A";

    auto [err4, result4] = table.upsertWithResult(recordNewer);
    if (err4) return 1;
    if (result4 != DiscoveryUpdateResult::Updated) return 1;

    auto found = table.find(record1.nodeId);
    if (!found) return 1;
    if (found->sessionEpoch.value != 10) return 1;
    if (found->platform != Platform::Mac) return 1;

    DiscoveryRecord recordConflict{};
    recordConflict.nodeId.high = 1;
    recordConflict.nodeId.low = 100;
    recordConflict.platform = Platform::Win;
    recordConflict.sessionEpoch.value = 15;
    recordConflict.protocolVersion = 1;
    recordConflict.topologyId = "topo-B";

    auto [err5, result5] = table.upsertWithResult(recordConflict);
    if (result5 != DiscoveryUpdateResult::TopologyConflict) return 1;
    if (table.conflictCount() != 1) return 1;

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
    if (testMdnsRoundTripDiscovery()) { std::puts("FAIL: testMdnsRoundTripDiscovery"); return 1; }
    if (testDiscoveryTransportSelection()) { std::puts("FAIL: testDiscoveryTransportSelection"); return 1; }
    if (testManualConfigFallback()) { std::puts("FAIL: testManualConfigFallback"); return 1; }
    if (testRecoverFromCorruption()) { std::puts("FAIL: testRecoverFromCorruption"); return 1; }
    if (testDiscoveryManualFallbackIntegration()) { std::puts("FAIL: testDiscoveryManualFallbackIntegration"); return 1; }
    if (testNetworkChangeEventSource()) { std::puts("FAIL: testNetworkChangeEventSource"); return 1; }
    if (testTopologyChangeEventSource()) { std::puts("FAIL: testTopologyChangeEventSource"); return 1; }
    if (testAutoRepublishOnNetworkChange()) { std::puts("FAIL: testAutoRepublishOnNetworkChange"); return 1; }
    if (testAutoRepublishOnTopologyChange()) { std::puts("FAIL: testAutoRepublishOnTopologyChange"); return 1; }
    if (testLifecycleUnsubscribeNoUseAfterFree()) { std::puts("FAIL: testLifecycleUnsubscribeNoUseAfterFree"); return 1; }
    if (testDiscoveryTableDuplicateStaleConflict()) { std::puts("FAIL: testDiscoveryTableDuplicateStaleConflict"); return 1; }
    std::puts("ALL PASS");
    return 0;
}