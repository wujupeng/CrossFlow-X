#include "mac/native_coord_normalizer.hpp"

#include <algorithm>
#include <cstdint>

namespace cfx {

NormalizedCoord NativeCoordNormalizer::normalizeSingleDisplay(const NativeCoord& native,
                                                               const NativeScreenGeometry& geometry) const noexcept {
    NormalizedCoord result;
    result.x = static_cast<int32_t>(native.x - geometry.originX);
    result.y = static_cast<int32_t>(native.y - geometry.originY);
    return result;
}

DisplayBoundingBox NativeCoordNormalizer::computeBoundingBox(const std::vector<NativeScreenGeometry>& allDisplays) const noexcept {
    if (allDisplays.empty()) {
        return DisplayBoundingBox{0, 0, 0, 0};
    }

    DisplayBoundingBox bbox;
    bbox.left = allDisplays[0].originX;
    bbox.top = allDisplays[0].originY;
    bbox.right = static_cast<int64_t>(allDisplays[0].originX) + static_cast<int64_t>(allDisplays[0].width);
    bbox.bottom = static_cast<int64_t>(allDisplays[0].originY) + static_cast<int64_t>(allDisplays[0].height);

    for (size_t i = 1; i < allDisplays.size(); ++i) {
        bbox.left = std::min(bbox.left, allDisplays[i].originX);
        bbox.top = std::min(bbox.top, allDisplays[i].originY);
        int64_t right = static_cast<int64_t>(allDisplays[i].originX) + static_cast<int64_t>(allDisplays[i].width);
        int64_t bottom = static_cast<int64_t>(allDisplays[i].originY) + static_cast<int64_t>(allDisplays[i].height);
        bbox.right = std::max(bbox.right, right);
        bbox.bottom = std::max(bbox.bottom, bottom);
    }

    return bbox;
}

NormalizedCoord NativeCoordNormalizer::normalizeMultiDisplay(const NativeCoord& native,
                                                             const DisplayBoundingBox& bbox) const noexcept {
    NormalizedCoord result;
    result.x = static_cast<int32_t>(native.x - bbox.left);
    result.y = static_cast<int32_t>(native.y - bbox.top);
    return result;
}

ScreenBoundary NativeCoordNormalizer::toScreenBoundary(const NativeScreenGeometry& geometry) const noexcept {
    ScreenBoundary boundary;
    boundary.width = geometry.width;
    boundary.height = geometry.height;
    boundary.originX = static_cast<uint32_t>(geometry.originX);
    boundary.originY = static_cast<uint32_t>(geometry.originY);
    return boundary;
}

ScreenBoundary NativeCoordNormalizer::toScreenBoundaryMerged(const DisplayBoundingBox& bbox) const noexcept {
    ScreenBoundary boundary;
    boundary.width = static_cast<uint32_t>(bbox.right - bbox.left);
    boundary.height = static_cast<uint32_t>(bbox.bottom - bbox.top);
    boundary.originX = static_cast<uint32_t>(bbox.left);
    boundary.originY = static_cast<uint32_t>(bbox.top);
    return boundary;
}

uint32_t NativeCoordNormalizer::pixelsToPoints(uint32_t pixels, double backingScaleFactor) const noexcept {
    if (backingScaleFactor <= 0.0) {
        return pixels;
    }
    return static_cast<uint32_t>(static_cast<double>(pixels) / backingScaleFactor + 0.5);
}

}  // namespace cfx