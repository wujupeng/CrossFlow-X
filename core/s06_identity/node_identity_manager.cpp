#include "s06_identity/node_identity_manager.hpp"

#include <fstream>
#include <sstream>

namespace cfx {

NodeIdentityManager::NodeIdentityManager(std::string persistPath)
    : persistPath_(std::move(persistPath)) {}

std::optional<NodeIdentity> NodeIdentityManager::getIdentity() const noexcept {
    return identity_;
}

NodeId NodeIdentityManager::getNodeId() const noexcept {
    if (identity_) return identity_->nodeId;
    return NodeId{0, 0};
}

SessionEpoch NodeIdentityManager::currentEpoch() const noexcept {
    if (identity_) return identity_->sessionEpoch;
    return SessionEpoch{};
}

std::optional<ErrorCode> NodeIdentityManager::initialize(NodeIdentity identity) noexcept {
    if (!identity.isValid()) return ErrorCode::SessNodeidForge;
    if (identity_ && identity_->nodeId != identity.nodeId) return ErrorCode::SessNodeidForge;
    identity_ = identity;
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::updatePublicKey(const PublicKey& newKey) noexcept {
    if (!identity_) return ErrorCode::SessUuidGenFail;
    identity_->publicKey = newKey;
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::updateCapabilities(const Capabilities& caps) noexcept {
    if (!identity_) return ErrorCode::SessUuidGenFail;
    if (!caps.isValid()) return ErrorCode::RegCapIncompat;
    identity_->capabilities = caps;
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::addEndpointAddress(const EndpointAddress& addr) noexcept {
    if (!identity_) return ErrorCode::SessUuidGenFail;
    for (const auto& existing : identity_->endpointAddresses) {
        if (existing.value == addr.value && existing.port == addr.port) return std::nullopt;
    }
    identity_->endpointAddresses.push_back(addr);
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::removeEndpointAddress(const EndpointAddress& addr) noexcept {
    if (!identity_) return ErrorCode::SessUuidGenFail;
    auto& addrs = identity_->endpointAddresses;
    addrs.erase(std::remove_if(addrs.begin(), addrs.end(),
        [&](const EndpointAddress& e) { return e.value == addr.value && e.port == addr.port; }),
        addrs.end());
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::joinTopology(const TopologyMembership& membership) noexcept {
    if (!identity_) return ErrorCode::SessUuidGenFail;
    identity_->topologyMembership = membership;
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::leaveTopology() noexcept {
    if (!identity_) return ErrorCode::SessUuidGenFail;
    identity_->topologyMembership.reset();
    return std::nullopt;
}

SessionEpoch NodeIdentityManager::incrementEpoch() noexcept {
    if (!identity_) return SessionEpoch{};
    identity_->sessionEpoch.increment();
    return identity_->sessionEpoch;
}

std::optional<ErrorCode> NodeIdentityManager::persist() const noexcept {
    if (!identity_) return ErrorCode::SessUuidGenFail;
    if (persistPath_.empty()) return std::nullopt;
    std::ofstream ofs(persistPath_, std::ios::trunc);
    if (!ofs) return ErrorCode::SessPersistCorrupt;
    ofs << identity_->nodeId.high << ' ' << identity_->nodeId.low << '\n';
    ofs << identity_->sessionEpoch.value << '\n';
    if (identity_->topologyMembership.has_value()) {
        const auto& tm = identity_->topologyMembership.value();
        ofs << 1 << ' ' << tm.topologyId << ' ' << tm.segmentIndex << '\n';
    } else {
        ofs << 0 << '\n';
    }
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::load() noexcept {
    if (persistPath_.empty()) return ErrorCode::SessPersistCorrupt;
    std::ifstream ifs(persistPath_);
    if (!ifs) return ErrorCode::SessPersistCorrupt;
    NodeId nid{};
    if (!(ifs >> nid.high >> nid.low)) return ErrorCode::SessPersistCorrupt;
    u64 epochVal{};
    if (!(ifs >> epochVal)) return ErrorCode::SessPersistCorrupt;
    int hasTopology = 0;
    if (!(ifs >> hasTopology)) return ErrorCode::SessPersistCorrupt;
    if (nid.isNull()) return ErrorCode::SessNodeidForge;
    if (!identity_) {
        identity_.emplace();
        identity_->nodeId = nid;
    } else {
        identity_->nodeId = nid;
    }
    identity_->sessionEpoch.value = epochVal;
    if (hasTopology == 1) {
        std::string topologyId;
        u32 segmentIndex = 0;
        if (!(ifs >> topologyId >> segmentIndex)) return ErrorCode::SessPersistCorrupt;
        TopologyMembership tm;
        tm.topologyId = topologyId;
        tm.segmentIndex = segmentIndex;
        identity_->topologyMembership = tm;
    } else {
        identity_->topologyMembership.reset();
    }
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::recoverNodeId() noexcept {
    auto err = load();
    if (err) return err;
    if (!identity_ || identity_->nodeId.isNull()) return ErrorCode::SessNodeidForge;
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::recoverFromCorruption() noexcept {
    lastRecovery_ = {};

    auto loadErr = load();
    if (loadErr) {
        lastRecovery_.nodeIdRegenerated = true;
        lastRecovery_.logEntries.push_back("CFX-W-SESS-PERSIST-CORRUPT: NodeID persistence corrupted, regenerating");

        if (!identity_) {
            identity_.emplace();
        }
        identity_->nodeId = NodeId::generate();
        identity_->sessionEpoch = SessionEpoch{1};
        lastRecovery_.epochReset = true;
        lastRecovery_.logEntries.push_back("CFX-W-SESS-PERSIST-CORRUPT: Session Epoch reset to 1");

        identity_->topologyMembership.reset();
        lastRecovery_.topologyCleared = true;
        lastRecovery_.logEntries.push_back("CFX-W-SESS-PERSIST-CORRUPT: Topology Membership cleared");

        auto persistErr = persist();
        if (persistErr) {
            lastRecovery_.logEntries.push_back("CFX-E-SESS-UUID-GEN-FAIL: Failed to persist recovered identity");
        }
        return std::nullopt;
    }

    if (!identity_ || identity_->nodeId.isNull()) {
        lastRecovery_.nodeIdRegenerated = true;
        lastRecovery_.logEntries.push_back("CFX-W-SESS-PERSIST-CORRUPT: NodeID is null, regenerating");

        if (!identity_) {
            identity_.emplace();
        }
        identity_->nodeId = NodeId::generate();
    }

    if (identity_->sessionEpoch.value == 0) {
        lastRecovery_.epochReset = true;
        identity_->sessionEpoch = SessionEpoch{1};
        lastRecovery_.logEntries.push_back("CFX-W-SESS-PERSIST-CORRUPT: Session Epoch was 0, reset to 1");
    }

    if (identity_->topologyMembership.has_value()) {
        const auto& tm = identity_->topologyMembership.value();
        if (tm.topologyId.empty()) {
            lastRecovery_.topologyCleared = true;
            identity_->topologyMembership.reset();
            lastRecovery_.logEntries.push_back("CFX-W-SESS-PERSIST-CORRUPT: Topology Membership corrupted (empty topologyId), cleared");
        }
    }

    if (lastRecovery_.nodeIdRegenerated || lastRecovery_.epochReset || lastRecovery_.topologyCleared) {
        persist();
    }

    return std::nullopt;
}

}  // namespace cfx