#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s07_discovery/i_discovery_service.hpp"
#include "s07_discovery/discovery_table.hpp"

namespace cfx {

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

private:
    DiscoveryTable table_;
    std::optional<DiscoveryDigest> currentDigest_;
    bool announcing_{false};
    bool listening_{false};
};

}  // namespace cfx