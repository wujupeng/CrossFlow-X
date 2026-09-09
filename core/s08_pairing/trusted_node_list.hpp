#pragma once

#include <string>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "s08_pairing/i_trusted_node_list.hpp"

namespace cfx {

class TrustedNodeList : public ITrustedNodeList {
public:
    TrustedNodeList() = default;
    explicit TrustedNodeList(std::string persistPath);

    std::optional<ErrorCode> add(const TrustedNodeEntry& entry) noexcept override;
    std::optional<ErrorCode> remove(const NodeId& nodeId) noexcept override;
    std::optional<TrustedNodeEntry> find(const NodeId& nodeId) const noexcept override;
    bool isTrusted(const NodeId& nodeId) const noexcept override;
    std::vector<TrustedNodeEntry> all() const noexcept override;
    std::optional<ErrorCode> persist() const noexcept override;
    std::optional<ErrorCode> load() noexcept override;
    std::optional<ErrorCode> recoverFromCorruption() noexcept;

    struct RecoveryResult {
        bool listCleared{false};
        std::vector<std::string> logEntries;
    };
    const RecoveryResult& lastRecoveryResult() const noexcept { return lastRecovery_; }

private:
    std::vector<TrustedNodeEntry> entries_;
    std::string persistPath_;
    RecoveryResult lastRecovery_;
};

}  // namespace cfx