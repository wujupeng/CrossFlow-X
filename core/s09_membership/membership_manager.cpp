#include "s09_membership/membership_manager.hpp"

namespace cfx {

std::optional<ErrorCode> MembershipManager::addMember(const MembershipEntry& entry) noexcept {
    for (auto& m : members_) {
        if (m.nodeId == entry.nodeId) {
            m = entry;
            return std::nullopt;
        }
    }
    members_.push_back(entry);
    return std::nullopt;
}

std::optional<ErrorCode> MembershipManager::removeMember(const NodeId& nodeId) noexcept {
    members_.erase(std::remove_if(members_.begin(), members_.end(),
        [&](const MembershipEntry& m) { return m.nodeId == nodeId; }),
        members_.end());
    return std::nullopt;
}

std::optional<MembershipEntry> MembershipManager::getMember(const NodeId& nodeId) const noexcept {
    for (const auto& m : members_) {
        if (m.nodeId == nodeId) return m;
    }
    return std::nullopt;
}

std::vector<MembershipEntry> MembershipManager::allMembers() const noexcept {
    return members_;
}

u32 MembershipManager::memberCount() const noexcept {
    return static_cast<u32>(members_.size());
}

bool MembershipManager::isCircular() const noexcept {
    if (members_.size() < 3) return false;
    for (const auto& m : members_) {
        if (!m.topologyMembership.leftNeighbor || !m.topologyMembership.rightNeighbor) return false;
    }
    return true;
}

bool MembershipManager::isDegraded() const noexcept {
    if (members_.empty()) return true;
    u32 onlineCount = 0;
    for (const auto& m : members_) {
        if (m.isOnline) ++onlineCount;
    }
    return onlineCount < 3 || !isCircular();
}

std::optional<ErrorCode> MembershipManager::setMemberOnline(const NodeId& nodeId, bool online) noexcept {
    for (auto& m : members_) {
        if (m.nodeId == nodeId) {
            m.isOnline = online;
            return std::nullopt;
        }
    }
    return ErrorCode::SessUnknownNode;
}

}  // namespace cfx