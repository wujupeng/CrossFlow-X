#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

struct ManualEndpoint {
    std::string host;
    u16 port{0};
    NodeId nodeId{};
};

class ManualConfigFallback {
public:
    ManualConfigFallback() = default;
    ~ManualConfigFallback() = default;

    ManualConfigFallback(const ManualConfigFallback&) = delete;
    ManualConfigFallback& operator=(const ManualConfigFallback&) = delete;

    std::optional<ErrorCode> loadFromFile(const std::string& path) noexcept;
    std::optional<ErrorCode> loadFromList(const std::vector<ManualEndpoint>& endpoints) noexcept;

    std::optional<ErrorCode> start() noexcept;
    std::optional<ErrorCode> stop() noexcept;
    bool isRunning() const noexcept { return running_; }

    const std::vector<ManualEndpoint>& configuredEndpoints() const noexcept { return endpoints_; }
    std::optional<ManualEndpoint> findByNodeId(const NodeId& nodeId) const noexcept;

private:
    bool running_{false};
    std::vector<ManualEndpoint> endpoints_;
};

}  // namespace cfx