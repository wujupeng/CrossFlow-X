#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s07_discovery/i_discovery_service.hpp"
#include "s07_discovery/discovery_table.hpp"
#include "s07_discovery/mdns_announcer.hpp"
#include "s07_discovery/mdns_listener.hpp"
#include "s07_discovery/mdns.hpp"

namespace cfx {

enum class DiscoveryTransport {
    None,
    Mdns,
    UdpBroadcast,
    ManualConfig
};

class DiscoveryService : public IDiscoveryService {
public:
    DiscoveryService() = default;

    std::optional<ErrorCode> startAnnouncing(const DiscoveryDigest& digest) noexcept override;
    std::optional<ErrorCode> stopAnnouncing() noexcept override;
    std::optional<ErrorCode> startListening() noexcept override;
    std::optional<ErrorCode> stopListening() noexcept override;

    std::vector<DiscoveryRecord> discoveredNodes() const noexcept override;
    std::optional<DiscoveryRecord> findNode(const NodeId& nodeId) const noexcept override;
    void clearDiscovered() noexcept override;

    std::optional<ErrorCode> updateDigest(const DiscoveryDigest& digest) noexcept override;

    bool isAnnouncing() const noexcept override;
    bool isListening() const noexcept override;

    std::optional<ErrorCode> handleDiscoveryAnnouncement(const DiscoveryAnnouncement& msg) noexcept;
    std::vector<NodeId> detectConflicts() const noexcept;
    void pruneStale(std::chrono::milliseconds maxAge) noexcept;

    DiscoveryTransport activeTransport() const noexcept { return transport_; }

private:
    DiscoveryTable table_;
    std::optional<DiscoveryDigest> currentDigest_;
    bool announcing_{false};
    bool listening_{false};

    MdnsAnnouncer mdnsAnnouncer_;
    MdnsListener mdnsListener_;
    LanBroadcastFallback udpFallback_;
    DiscoveryTransport transport_{DiscoveryTransport::None};

    std::optional<ErrorCode> tryMdnsAnnounce(const DiscoveryDigest& digest) noexcept;
    std::optional<ErrorCode> tryUdpFallback(const DiscoveryDigest& digest) noexcept;
};

}  // namespace cfx