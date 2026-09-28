#include "mac/multi_display_merge_policy.hpp"

#include <algorithm>
#include <cstdio>

namespace cfx {

MultiDisplayMergePolicyConfig::MultiDisplayMergePolicyConfig()
    : policy_(std::nullopt) {}

MultiDisplayMergePolicyConfig::~MultiDisplayMergePolicyConfig() = default;

std::optional<MultiDisplayMergePolicy> MultiDisplayMergePolicyConfig::loadPolicy(uint32_t displayCount) const noexcept {
    if (displayCount <= 1) {
        return std::nullopt;
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

ScreenBoundary MultiDisplayMergePolicyConfig::mergeBoundingBox(const std::vector<ScreenBoundary>& boundaries) noexcept {
    if (boundaries.empty()) {
        return ScreenBoundary{0, 0, 0, 0};
    }

    uint32_t minX = boundaries[0].originX;
    uint32_t minY = boundaries[0].originY;
    uint32_t maxX = boundaries[0].originX + boundaries[0].width;
    uint32_t maxY = boundaries[0].originY + boundaries[0].height;

    for (size_t i = 1; i < boundaries.size(); ++i) {
        minX = std::min(minX, boundaries[i].originX);
        minY = std::min(minY, boundaries[i].originY);
        maxX = std::max(maxX, boundaries[i].originX + boundaries[i].width);
        maxY = std::max(maxY, boundaries[i].originY + boundaries[i].height);
    }

    return ScreenBoundary{maxX - minX, maxY - minY, 0, 0};
}

ScreenBoundary MultiDisplayMergePolicyConfig::merge(const std::vector<ScreenBoundary>& boundaries) const noexcept {
    if (boundaries.empty()) {
        return ScreenBoundary{0, 0, 0, 0};
    }

    uint32_t displayCount = static_cast<uint32_t>(boundaries.size());

    if (displayCount == 1) {
        return ScreenBoundary{boundaries[0].width, boundaries[0].height, 0, 0};
    }

    auto policy = loadPolicy(displayCount);

    if (!policy.has_value()) {
        fprintf(stderr, "[CFX-E-CAP-MULTIDISPLAY-UNDECLARED] multi-display without merge policy declaration\n");
        return rejectUndeclared();
    }

    if (policy.value() == MultiDisplayMergePolicy::Unsupported) {
        fprintf(stderr, "[CFX-E-CAP-MULTIDISPLAY-UNDECLARED] multi-display declared unsupported\n");
        return rejectUndeclared();
    }

    return mergeBoundingBox(boundaries);
}

}  // namespace cfx
