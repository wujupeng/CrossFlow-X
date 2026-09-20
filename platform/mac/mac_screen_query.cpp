#include "mac/mac_screen_query.hpp"
#include "mac/native_coord_normalizer.hpp"
#include "mac/screen_boundary_cache.hpp"
#include "mac/multi_display_merge_policy.hpp"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <cstdio>
#include <memory>

namespace cfx {

MacScreenQuery::MacScreenQuery(NativeCoordNormalizer& normalizer,
                               ScreenBoundaryCache& cache,
                               MultiDisplayMergePolicyConfig& policyConfig)
    : normalizer_(normalizer)
    , cache_(cache)
    , policyConfig_(policyConfig) {}

MacScreenQuery::~MacScreenQuery() = default;

ScreenBoundary MacScreenQuery::primaryBoundary() {
    auto snapshot = cache_.getSnapshot();
    if (snapshot && snapshot->isValid()) {
        return *snapshot;
    }
    return requeryAndNormalize();
}

ScreenBoundary MacScreenQuery::requeryAndNormalize() {
    auto displays = queryAllDisplaysNativeGeometry();
    if (displays.empty()) {
        fprintf(stderr, "[CFX-E-CAP-SCREEN-QUERY-FAIL] no displays found\n");
        return ScreenBoundary{0, 0, 0, 0};
    }

    uint32_t displayCount = static_cast<uint32_t>(displays.size());

    auto policy = policyConfig_.loadPolicy(displayCount);

    if (displayCount > 1) {
        if (!policy.has_value()) {
            fprintf(stderr, "[CFX-E-CAP-MULTIDISPLAY-UNDECLARED] multi-display without merge policy declaration\n");
            return policyConfig_.rejectUndeclared();
        }
        if (policy.value() == MultiDisplayMergePolicy::Unsupported) {
            fprintf(stderr, "[CFX-E-CAP-MULTIDISPLAY-UNDECLARED] multi-display declared unsupported\n");
            return policyConfig_.rejectUndeclared();
        }
    }

    ScreenBoundary boundary;
    if (displayCount == 1) {
        boundary = normalizer_.toScreenBoundary(displays[0]);
    } else {
        auto bbox = normalizer_.computeBoundingBox(displays);
        boundary = normalizer_.toScreenBoundaryMerged(bbox);
    }

    if (!boundary.isValid()) {
        fprintf(stderr, "[CFX-E-CAP-SCREEN-QUERY-FAIL] normalized boundary invalid\n");
        return boundary;
    }

    auto snapshot = std::make_shared<const ScreenBoundary>(boundary);
    cache_.publishSnapshot(snapshot);
    return boundary;
}

std::vector<NativeScreenGeometry> MacScreenQuery::queryAllDisplaysNativeGeometry() noexcept {
    std::vector<NativeScreenGeometry> displays;

#ifdef __APPLE__
    const uint32_t maxDisplays = 16;
    CGDirectDisplayID displayIds[maxDisplays];
    uint32_t displayCount = 0;
    CGError err = CGGetActiveDisplayList(maxDisplays, displayIds, &displayCount);
    if (err != kCGErrorSuccess || displayCount == 0) {
        return displays;
    }

    for (uint32_t i = 0; i < displayCount; ++i) {
        CGDirectDisplayID did = displayIds[i];
        CGRect bounds = CGDisplayBounds(did);
        uint32_t pointsWide = static_cast<uint32_t>(bounds.size.width);
        uint32_t pointsHigh = static_cast<uint32_t>(bounds.size.height);

        double scaleFactor = 1.0;
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(did);
        if (mode) {
            size_t pixelWidth = CGDisplayModeGetPixelWidth(mode);
            if (pointsWide > 0) {
                scaleFactor = static_cast<double>(pixelWidth) / static_cast<double>(pointsWide);
            }
            CGDisplayModeRelease(mode);
        }

        NativeScreenGeometry geom;
        geom.originX = static_cast<int64_t>(bounds.origin.x);
        geom.originY = static_cast<int64_t>(bounds.origin.y);
        geom.width = pointsWide;
        geom.height = pointsHigh;
        geom.apiSource = NativeScreenGeometry::ApiSource::CGDisplay;
        geom.backingScaleFactor = scaleFactor;
        displays.push_back(geom);
    }
#else
    NativeScreenGeometry geom;
    geom.originX = 0;
    geom.originY = 0;
    geom.width = 1920;
    geom.height = 1080;
    geom.apiSource = NativeScreenGeometry::ApiSource::CGDisplay;
    geom.backingScaleFactor = 1.0;
    displays.push_back(geom);
#endif

    return displays;
}

}  // namespace cfx