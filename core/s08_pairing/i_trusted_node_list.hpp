#pragma once

#include <optional>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

class ITrustedNodeList {
public:
    virtual ~ITrustedNodeList() = default;

    virtual std::optional<ErrorCode> add(const TrustedNodeEntry& entry) noexcept = 0;
    virtual std::optional<ErrorCode> remove(const NodeId& nodeId) noexcept = 0;
    virtual std::optional<TrustedNodeEntry> find(const NodeId& nodeId) const noexcept = 0;
    virtual bool isTrusted(const NodeId& nodeId) const noexcept = 0;
    virtual std::vector<TrustedNodeEntry> all() const noexcept = 0;

    virtual std::optional<ErrorCode> persist() const noexcept = 0;
    virtual std::optional<ErrorCode> load() noexcept = 0;
};

}  // namespace cfx