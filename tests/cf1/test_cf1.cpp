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
    NodeId coord = NodeId::generate();
    auto err = mgr.setCoordinator(coord);
    if (err) return 1;
    if (mgr.isCommitLocked()) return 1;
    auto cid = mgr.coordinatorId();
    if (!cid || *cid != coord) return 1;
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
    std::puts("ALL PASS");
    return 0;
}