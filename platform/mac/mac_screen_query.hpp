#pragma once

#include "common/domain.hpp"
#include "common/platform_ports.hpp"

#include <vector>

namespace cfx {

class NativeCoordNormalizer;
class ScreenBoundaryCache;
class MultiDisplayMergePolicyConfig;
struct NativeScreenGeometry;

class MacScreenQuery : public IScreenQuery {
public:
    MacScreenQuery(NativeCoordNormalizer& normalizer,
                   ScreenBoundaryCache& cache,
                   MultiDisplayMergePolicyConfig& policyConfig);
    ~MacScreenQuery() override;

    MacScreenQuery(const MacScreenQuery&) = delete;
    MacScreenQuery& operator=(const MacScreenQuery&) = delete;

    ScreenBoundary primaryBoundary() override;

    ScreenBoundary requeryAndNormalize();

private:
    NativeCoordNormalizer& normalizer_;
    ScreenBoundaryCache& cache_;
    MultiDisplayMergePolicyConfig& policyConfig_;

#ifdef __APPLE__
    std::vector<NativeScreenGeometry> queryAllDisplaysNativeGeometry() noexcept;
#else
    std::vector<NativeScreenGeometry> queryAllDisplaysNativeGeometry() noexcept;
#endif
};

}  // namespace cfx