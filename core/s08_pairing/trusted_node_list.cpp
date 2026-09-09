#include "s08_pairing/trusted_node_list.hpp"

#include <fstream>

namespace cfx {

TrustedNodeList::TrustedNodeList(std::string persistPath)
    : persistPath_(std::move(persistPath)) {}

std::optional<ErrorCode> TrustedNodeList::add(const TrustedNodeEntry& entry) noexcept {
    for (auto& e : entries_) {
        if (e.nodeId == entry.nodeId) {
            e = entry;
            return std::nullopt;
        }
    }
    entries_.push_back(entry);
    return std::nullopt;
}

std::optional<ErrorCode> TrustedNodeList::remove(const NodeId& nodeId) noexcept {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
        [&](const TrustedNodeEntry& e) { return e.nodeId == nodeId; }),
        entries_.end());
    return std::nullopt;
}

std::optional<TrustedNodeEntry> TrustedNodeList::find(const NodeId& nodeId) const noexcept {
    for (const auto& e : entries_) {
        if (e.nodeId == nodeId) return e;
    }
    return std::nullopt;
}

bool TrustedNodeList::isTrusted(const NodeId& nodeId) const noexcept {
    return find(nodeId).has_value();
}

std::vector<TrustedNodeEntry> TrustedNodeList::all() const noexcept {
    return entries_;
}

std::optional<ErrorCode> TrustedNodeList::persist() const noexcept {
    if (persistPath_.empty()) return std::nullopt;
    std::ofstream ofs(persistPath_, std::ios::trunc);
    if (!ofs) return ErrorCode::PairTrustPersistFail;
    for (const auto& e : entries_) {
        ofs << e.nodeId.high << ' ' << e.nodeId.low << ' ' << e.pairedAt << ' ' << e.lastSeenEpoch.value << '\n';
    }
    return std::nullopt;
}

std::optional<ErrorCode> TrustedNodeList::load() noexcept {
    if (persistPath_.empty()) return ErrorCode::PairTrustCorrupt;
    std::ifstream ifs(persistPath_);
    if (!ifs) return ErrorCode::PairTrustCorrupt;
    entries_.clear();
    TrustedNodeEntry e{};
    while (ifs >> e.nodeId.high >> e.nodeId.low >> e.pairedAt >> e.lastSeenEpoch.value) {
        entries_.push_back(e);
    }
    return std::nullopt;
}

std::optional<ErrorCode> TrustedNodeList::recoverFromCorruption() noexcept {
    lastRecovery_ = {};

    auto loadErr = load();
    if (loadErr) {
        lastRecovery_.listCleared = true;
        entries_.clear();
        lastRecovery_.logEntries.push_back("CFX-W-PAIR-TRUST-CORRUPT: Trusted List persistence corrupted, cleared all entries");
        return std::nullopt;
    }

    bool hasCorrupt = false;
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->nodeId.isNull() || it->pairedAt == 0) {
            hasCorrupt = true;
            lastRecovery_.logEntries.push_back("CFX-W-PAIR-TRUST-CORRUPT: Removing corrupt trusted entry (null NodeID or invalid pairedAt)");
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }

    if (hasCorrupt) {
        lastRecovery_.listCleared = true;
        persist();
    }

    return std::nullopt;
}

}  // namespace cfx