#include "s07_discovery/discovery_service.hpp"

namespace cfx {

std::optional<ErrorCode> DiscoveryService::startAnnouncing(const DiscoveryDigest& digest) noexcept {
    if (digest.nodeId.isNull()) return ErrorCode::SessNodeidForge;
    currentDigest_ = digest;
    announcing_ = true;
    return std::nullopt;
}

std::optional<ErrorCode> DiscoveryService::stopAnnouncing() noexcept {
    announcing_ = false;
    currentDigest_.reset();
    return std::nullopt;
}

std::optional<ErrorCode> DiscoveryService::startListening() noexcept {
    listening_ = true;
    return std::nullopt;
}

std::optional<ErrorCode> DiscoveryService::stopListening() noexcept {
    listening_ = false;
    return std::nullopt;
}

std::vector<DiscoveryRecord> DiscoveryService::discoveredNodes() const noexcept {
    return table_.all();
}

std::optional<DiscoveryRecord> DiscoveryService::findNode(const NodeId& nodeId) const noexcept {
    return table_.find(nodeId);
}

void DiscoveryService::clearDiscovered() noexcept {
    table_.clear();
}

std::optional<ErrorCode> DiscoveryService::updateDigest(const DiscoveryDigest& digest) noexcept {
    if (!announcing_) return ErrorCode::DiscAnnounceLost;
    currentDigest_ = digest;
    return std::nullopt;
}

bool DiscoveryService::isAnnouncing() const noexcept { return announcing_; }
bool DiscoveryService::isListening() const noexcept { return listening_; }

std::optional<ErrorCode> DiscoveryService::handleDiscoveryAnnouncement(const DiscoveryAnnouncement& msg) noexcept {
    if (!listening_) return ErrorCode::DiscAnnounceLost;
    DiscoveryRecord record;
    record.nodeId = msg.digest.nodeId;
    record.platform = msg.digest.platform;
    record.capabilitiesFingerprint = msg.digest.capabilitiesFingerprint;
    record.sessionEpoch = msg.digest.sessionEpoch;
    record.protocolVersion = msg.digest.protocolVersion;
    record.topologyId = msg.digest.topologyId;
    record.lastSeenAt = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    return table_.upsert(record);
}

std::vector<NodeId> DiscoveryService::detectConflicts() const noexcept {
    return table_.detectConflicts();
}

void DiscoveryService::pruneStale(std::chrono::milliseconds maxAge) noexcept {
    table_.pruneStale(maxAge, std::chrono::steady_clock::now());
}

}  // namespace cfx