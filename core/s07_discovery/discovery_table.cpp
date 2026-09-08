#include "s07_discovery/discovery_table.hpp"

namespace cfx {

std::optional<ErrorCode> DiscoveryTable::upsert(const DiscoveryRecord& record) noexcept {
    if (record.nodeId.isNull()) return ErrorCode::SessNodeidForge;
    auto key = nodeKey(record.nodeId);
    auto now = std::chrono::steady_clock::now();
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        if (it->second.record.topologyId != record.topologyId && !record.topologyId.empty())
            return ErrorCode::TopoMultipath;
        it->second.record = record;
        it->second.lastUpdated = now;
        return std::nullopt;
    }
    entries_[key] = {record, now, now};
    return std::nullopt;
}

std::optional<ErrorCode> DiscoveryTable::remove(const NodeId& nodeId) noexcept {
    entries_.erase(nodeKey(nodeId));
    return std::nullopt;
}

std::optional<DiscoveryRecord> DiscoveryTable::find(const NodeId& nodeId) const noexcept {
    auto it = entries_.find(nodeKey(nodeId));
    if (it == entries_.end()) return std::nullopt;
    return it->second.record;
}

std::vector<DiscoveryRecord> DiscoveryTable::all() const noexcept {
    std::vector<DiscoveryRecord> result;
    result.reserve(entries_.size());
    for (const auto& [_, entry] : entries_) {
        result.push_back(entry.record);
    }
    return result;
}

std::vector<DiscoveryRecord> DiscoveryTable::findByTopology(const std::string& topologyId) const noexcept {
    std::vector<DiscoveryRecord> result;
    for (const auto& [_, entry] : entries_) {
        if (entry.record.topologyId == topologyId) {
            result.push_back(entry.record);
        }
    }
    return result;
}

void DiscoveryTable::clear() noexcept {
    entries_.clear();
}

std::vector<NodeId> DiscoveryTable::detectConflicts() const noexcept {
    std::vector<NodeId> conflicts;
    for (auto it1 = entries_.begin(); it1 != entries_.end(); ++it1) {
        for (auto it2 = std::next(it1); it2 != entries_.end(); ++it2) {
            const auto& r1 = it1->second.record;
            const auto& r2 = it2->second.record;
            if (r1.nodeId == r2.nodeId) {
                conflicts.push_back(r1.nodeId);
            }
        }
    }
    return conflicts;
}

bool DiscoveryTable::hasNodeIdConflict(const NodeId& nodeId) const noexcept {
    return entries_.count(nodeKey(nodeId)) > 0;
}

void DiscoveryTable::pruneStale(std::chrono::milliseconds maxAge,
                                 std::chrono::steady_clock::time_point now) noexcept {
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (now - it->second.lastUpdated > maxAge) {
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
}

u32 DiscoveryTable::size() const noexcept {
    return static_cast<u32>(entries_.size());
}

}  // namespace cfx