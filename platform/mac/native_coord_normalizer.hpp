#pragma once

#include "common/domain.hpp"

#include <cstdint>
#include <vector>

namespace cfx {

struct NativeCoord {
    int64_t x;
    int64_t y;
};

struct NormalizedCoord {
    int32_t x;
    int32_t y;
};

struct NativeScreenGeometry {
    int64_t originX;
    int64_t originY;
    uint32_t width;
    uint32_t height;
    enum class ApiSource : uint8_t {
        NSScreen,
        CGDisplay,
    };
    ApiSource apiSource{ApiSource::NSScreen};
    double backingScaleFactor{1.0};
};

struct DisplayBoundingBox {
    int64_t left;
    int64_t top;
    int64_t right;
    int64_t bottom;
};

class NativeCoordNormalizer {
public:
    NativeCoordNormalizer() noexcept = default;
    ~NativeCoordNormalizer() noexcept = default;

    NativeCoordNormalizer(const NativeCoordNormalizer&) = delete;
    NativeCoordNormalizer& operator=(const NativeCoordNormalizer&) = delete;

    NormalizedCoord normalizeSingleDisplay(const NativeCoord& native,
                                           const NativeScreenGeometry& geometry) const noexcept;

    DisplayBoundingBox computeBoundingBox(const std::vector<NativeScreenGeometry>& allDisplays) const noexcept;

    NormalizedCoord normalizeMultiDisplay(const NativeCoord& native,
                                          const DisplayBoundingBox& bbox) const noexcept;

    ScreenBoundary toScreenBoundary(const NativeScreenGeometry& geometry) const noexcept;
    ScreenBoundary toScreenBoundaryMerged(const DisplayBoundingBox& bbox) const noexcept;

    uint32_t pixelsToPoints(uint32_t pixels, double backingScaleFactor) const noexcept;
};

}  // namespace cfx