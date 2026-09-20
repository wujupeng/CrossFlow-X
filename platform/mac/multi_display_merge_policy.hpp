#pragma once

#include "common/domain.hpp"

#include <optional>

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

    std::optional<MultiDisplayMergePolicy> loadPolicy(uint32_t displayCount);

    std::optional<MultiDisplayMergePolicy> getPolicy() const noexcept;

    ScreenBoundary rejectUndeclared() const noexcept;

    void setPolicy(MultiDisplayMergePolicy policy) noexcept;

private:
    std::optional<MultiDisplayMergePolicy> policy_;
};

}  // namespace cfx