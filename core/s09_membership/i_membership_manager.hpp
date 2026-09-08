#pragma once

#include <optional>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

class IMembershipManager {
public:
    virtual ~IMembershipManager() = default;

    virtual std::optional<ErrorCode> addMember(const MembershipEntry& entry) noexcept = 0;
    virtual std::optional<ErrorCode> removeMember(const NodeId& nodeId) noexcept = 0;
    virtual std::optional<MembershipEntry> getMember(const NodeId& nodeId) const noexcept = 0;
    virtual std::vector<MembershipEntry> allMembers() const noexcept = 0;
    virtual u32 memberCount() const noexcept = 0;

    virtual bool isCircular() const noexcept = 0;
    virtual bool isDegraded() const noexcept = 0;
    virtual std::optional<ErrorCode> setMemberOnline(const NodeId& nodeId, bool online) noexcept = 0;
};

}  // namespace cfx