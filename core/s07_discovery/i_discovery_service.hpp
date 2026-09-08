#pragma once

#include <optional>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

class IDiscoveryService {
public:
    virtual ~IDiscoveryService() = default;

    virtual std::optional<ErrorCode> startAnnouncing(const DiscoveryDigest& digest) noexcept = 0;
    virtual std::optional<ErrorCode> stopAnnouncing() noexcept = 0;
    virtual std::optional<ErrorCode> startListening() noexcept = 0;
    virtual std::optional<ErrorCode> stopListening() noexcept = 0;

    virtual std::vector<DiscoveryRecord> discoveredNodes() const noexcept = 0;
    virtual std::optional<DiscoveryRecord> findNode(const NodeId& nodeId) const noexcept = 0;
    virtual void clearDiscovered() noexcept = 0;

    virtual std::optional<ErrorCode> updateDigest(const DiscoveryDigest& digest) noexcept = 0;

    virtual bool isAnnouncing() const noexcept = 0;
    virtual bool isListening() const noexcept = 0;
};

}  // namespace cfx