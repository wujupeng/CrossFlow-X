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
    if (nid.isNull()) return ErrorCode::SessNodeidForge;
    if (!identity_) {
        identity_.emplace();
        identity_->nodeId = nid;
    } else {
        identity_->nodeId = nid;
    }
    identity_->sessionEpoch.value = epochVal;
    return std::nullopt;
}

std::optional<ErrorCode> NodeIdentityManager::recoverNodeId() noexcept {
    auto err = load();
    if (err) return err;
    if (!identity_ || identity_->nodeId.isNull()) return ErrorCode::SessNodeidForge;
    return std::nullopt;
}

}  // namespace cfx