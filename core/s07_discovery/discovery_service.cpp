#include "s07_discovery/discovery_service.hpp"

namespace cfx {

std::optional<ErrorCode> DiscoveryService::tryMdnsAnnounce(const DiscoveryDigest& digest) noexcept {
    auto txt = digest.toTxtRecord();
    auto err = mdnsAnnouncer_.start("CrossFlow-X", 5353, txt);
    if (err) return err;
    transport_ = DiscoveryTransport::Mdns;
    return std::nullopt;
}

std::optional<ErrorCode> DiscoveryService::tryUdpFallback(const DiscoveryDigest& digest) noexcept {
    auto err = udpFallback_.start(digest, 5353);
    if (err) return err;
    err = udpFallback_.broadcast(digest);
    if (err) return err;
    transport_ = DiscoveryTransport::UdpBroadcast;
    return std::nullopt;
}

std::optional<ErrorCode> DiscoveryService::startAnnouncing(const DiscoveryDigest& digest) noexcept {
    if (digest.nodeId.isNull()) return ErrorCode::SessNodeidForge;
    currentDigest_ = digest;

    auto mdnsErr = tryMdnsAnnounce(digest);
    if (!mdnsErr) {
        announcing_ = true;
        return std::nullopt;
    }

    auto udpErr = tryUdpFallback(digest);
    if (!udpErr) {
        announcing_ = true;
        return std::nullopt;
    }

    return ErrorCode::DiscMdnsUnavailable;
}

std::optional<ErrorCode> DiscoveryService::stopAnnouncing() noexcept {
    if (transport_ == DiscoveryTransport::Mdns) {
        mdnsAnnouncer_.stop();
    } else if (transport_ == DiscoveryTransport::UdpBroadcast) {
        udpFallback_.stop();
    }
    announcing_ = false;
    currentDigest_.reset();
    transport_ = DiscoveryTransport::None;
    return std::nullopt;
}

std::optional<ErrorCode> DiscoveryService::startListening() noexcept {
    auto err = mdnsListener_.start("_crossflow-x._tcp");
    if (!err) {
        mdnsListener_.setDiscoveryCallback(
            [this](const std::string& serviceName,
                   const std::string& host,
                   u16 port,
                   const std::vector<std::pair<std::string, std::string>>& txtRecord) {
                (void)serviceName; (void)host; (void)port;
                DiscoveryRecord record;
                for (const auto& [key, val] : txtRecord) {
                    if (key == "nid") {
                        u64 high = 0, low = 0;
                        if (val.size() >= 32) {
                            high = std::stoull(val.substr(0, 16), nullptr, 16);
                            low = std::stoull(val.substr(16, 16), nullptr, 16);
                        }
                        record.nodeId = NodeId{high, low};
                    } else if (key == "plat") {
                        record.platform = (val == "mac") ? Platform::Mac : Platform::Win;
                    } else if (key == "epoch") {
                        record.sessionEpoch.value = std::stoull(val);
                    } else if (key == "pver") {
                        record.protocolVersion = static_cast<u16>(std::stoull(val));
                    } else if (key == "topo") {
                        record.topologyId = val;
                    }
                }
                record.lastSeenAt = static_cast<u64>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
                if (!record.nodeId.isNull()) {
                    table_.upsert(record);
                }
            });
        listening_ = true;
        return std::nullopt;
    }

    listening_ = true;
    return std::nullopt;
}

std::optional<ErrorCode> DiscoveryService::stopListening() noexcept {
    mdnsListener_.stop();
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

    if (transport_ == DiscoveryTransport::Mdns) {
        auto txt = digest.toTxtRecord();
        return mdnsAnnouncer_.updateTxt(txt);
    } else if (transport_ == DiscoveryTransport::UdpBroadcast) {
        return udpFallback_.broadcast(digest);
    }
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
