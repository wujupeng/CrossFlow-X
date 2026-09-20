#include "mac/multi_display_merge_policy.hpp"

namespace cfx {

MultiDisplayMergePolicyConfig::MultiDisplayMergePolicyConfig()
    : policy_(std::nullopt) {}

MultiDisplayMergePolicyConfig::~MultiDisplayMergePolicyConfig() = default;

std::optional<MultiDisplayMergePolicy> MultiDisplayMergePolicyConfig::loadPolicy(uint32_t displayCount) {
    if (displayCount <= 1) {
        return MultiDisplayMergePolicy::MergeBoundingBox;
    }
    return policy_;
}

std::optional<MultiDisplayMergePolicy> MultiDisplayMergePolicyConfig::getPolicy() const noexcept {
    return policy_;
}

ScreenBoundary MultiDisplayMergePolicyConfig::rejectUndeclared() const noexcept {
    return ScreenBoundary{0, 0, 0, 0};
}

void MultiDisplayMergePolicyConfig::setPolicy(MultiDisplayMergePolicy policy) noexcept {
    policy_ = policy;
}

}  // namespace cfx