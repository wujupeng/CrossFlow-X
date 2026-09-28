#pragma once

#include "common/domain.hpp"

#include <optional>
#include <vector>

namespace cfx {

enum class MultiDisplayMergePolicy : uint8_t {
    MergeBoundingBox,
    Unsupported,
};

class MultiDisplayMergePolicyConfig {
public:
    MultiDisplayMergePolicyConfig();
    ~MultiDisplayMergePolicyConfig();

    MultiDisplayMergePolicyConfig(const MultiDisplayMergePolicyConfig&) = delete;
    MultiDisplayMergePolicyConfig& operator=(const MultiDisplayMergePolicyConfig&) = delete;

    std::optional<MultiDisplayMergePolicy> loadPolicy(uint32_t displayCount) const noexcept;

    std::optional<MultiDisplayMergePolicy> getPolicy() const noexcept;

    ScreenBoundary rejectUndeclared() const noexcept;

    void setPolicy(MultiDisplayMergePolicy policy) noexcept;

    static ScreenBoundary mergeBoundingBox(const std::vector<ScreenBoundary>& boundaries) noexcept;

    ScreenBoundary merge(const std::vector<ScreenBoundary>& boundaries) const noexcept;

private:
    std::optional<MultiDisplayMergePolicy> policy_;
};

}  // namespace cfx
