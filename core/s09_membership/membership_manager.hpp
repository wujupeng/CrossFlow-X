#pragma once

#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "s09_membership/i_membership_manager.hpp"

namespace cfx {

class MembershipManager : public IMembershipManager {
public:
    MembershipManager() = default;

    std::optional<ErrorCode> addMember(const MembershipEntry& entry) noexcept override;
    std::optional<ErrorCode> removeMember(const NodeId& nodeId) noexcept override;
    std::optional<MembershipEntry> getMember(const NodeId& nodeId) const noexcept override;
    std::vector<MembershipEntry> allMembers() const noexcept override;
    u32 memberCount() const noexcept override;
    bool isCircular() const noexcept override;
    bool isDegraded() const noexcept override;
    std::optional<ErrorCode> setMemberOnline(const NodeId& nodeId, bool online) noexcept override;

private:
    std::vector<MembershipEntry> members_;
};

}  // namespace cfx