#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

class DiscoveryTable {
public:
    DiscoveryTable() = default;

    std::optional<ErrorCode> upsert(const DiscoveryRecord& record) noexcept;
    std::optional<ErrorCode> remove(const NodeId& nodeId) noexcept;
    std::optional<DiscoveryRecord> find(const NodeId& nodeId) const noexcept;
    std::vector<DiscoveryRecord> all() const noexcept;
    std::vector<DiscoveryRecord> findByTopology(const std::string& topologyId) const noexcept;
    void clear() noexcept;

    std::vector<NodeId> detectConflicts() const noexcept;
    bool hasNodeIdConflict(const NodeId& nodeId) const noexcept;

    void pruneStale(std::chrono::milliseconds maxAge,
                     std::chrono::steady_clock::time_point now) noexcept;

    u32 size() const noexcept;

private:
    struct Entry {
        DiscoveryRecord record;
        std::chrono::steady_clock::time_point insertedAt;
        std::chrono::steady_clock::time_point lastUpdated;
    };
    std::unordered_map<u64, Entry> entries_;

    static u64 nodeKey(const NodeId& nid) noexcept {
        return nid.high ^ (nid.low * 0x9E3779B97F4A7C15ULL);
    }
};

}  // namespace cfx