#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "mac/mac_screen_query.hpp"
#include "mac/native_coord_normalizer.hpp"
#include "mac/screen_boundary_cache.hpp"
#include "mac/multi_display_merge_policy.hpp"

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

static void test_native_coord_normalizer_single_display() {
    fprintf(stderr, "[TEST] test_native_coord_normalizer_single_display\n");
    NativeCoordNormalizer normalizer;

    NativeScreenGeometry geom;
    geom.originX = 0;
    geom.originY = 0;
    geom.width = 1920;
    geom.height = 1080;
    geom.backingScaleFactor = 1.0;

    NativeCoord native{100, 200};
    auto normalized = normalizer.normalizeSingleDisplay(native, geom);
    CFX_TEST_CHECK(normalized.x == 100);
    CFX_TEST_CHECK(normalized.y == 200);

    NativeCoord native_offset{1920 + 50, 1080 + 60};
    auto normalized2 = normalizer.normalizeSingleDisplay(native_offset, geom);
    CFX_TEST_CHECK(normalized2.x == 1970);
    CFX_TEST_CHECK(normalized2.y == 1140);

    auto boundary = normalizer.toScreenBoundary(geom);
    CFX_TEST_CHECK(boundary.width == 1920);
    CFX_TEST_CHECK(boundary.height == 1080);
    CFX_TEST_CHECK(boundary.originX == 0);
    CFX_TEST_CHECK(boundary.originY == 0);
    CFX_TEST_CHECK(boundary.isValid());
}

static void test_native_coord_normalizer_multi_display() {
    fprintf(stderr, "[TEST] test_native_coord_normalizer_multi_display\n");
    NativeCoordNormalizer normalizer;

    std::vector<NativeScreenGeometry> displays(2);
    displays[0].originX = 0;
    displays[0].originY = 0;
    displays[0].width = 1920;
    displays[0].height = 1080;
    displays[0].backingScaleFactor = 1.0;

    displays[1].originX = 1920;
    displays[1].originY = 0;
    displays[1].width = 1920;
    displays[1].height = 1080;
    displays[1].backingScaleFactor = 1.0;

    auto bbox = normalizer.computeBoundingBox(displays);
    CFX_TEST_CHECK(bbox.left == 0);
    CFX_TEST_CHECK(bbox.top == 0);
    CFX_TEST_CHECK(bbox.right == 3840);
    CFX_TEST_CHECK(bbox.bottom == 1080);

    auto boundary = normalizer.toScreenBoundaryMerged(bbox);
    CFX_TEST_CHECK(boundary.width == 3840);
    CFX_TEST_CHECK(boundary.height == 1080);
    CFX_TEST_CHECK(boundary.isValid());

    NativeCoord native_on_second{1920 + 100, 200};
    auto normalized = normalizer.normalizeMultiDisplay(native_on_second, bbox);
    CFX_TEST_CHECK(normalized.x == 2020);
    CFX_TEST_CHECK(normalized.y == 200);
}

static void test_native_coord_normalizer_pixels_to_points() {
    fprintf(stderr, "[TEST] test_native_coord_normalizer_pixels_to_points\n");
    NativeCoordNormalizer normalizer;

    CFX_TEST_CHECK(normalizer.pixelsToPoints(100, 1.0) == 100);
    CFX_TEST_CHECK(normalizer.pixelsToPoints(200, 2.0) == 100);
    CFX_TEST_CHECK(normalizer.pixelsToPoints(150, 1.5) == 100);
    CFX_TEST_CHECK(normalizer.pixelsToPoints(100, 0.0) == 100);
}

static void test_screen_boundary_cache_basic() {
    fprintf(stderr, "[TEST] test_screen_boundary_cache_basic\n");
    ScreenBoundaryCache cache;

    CFX_TEST_CHECK(!cache.isValid());

    auto snap = cache.getSnapshot();
    CFX_TEST_CHECK(snap == nullptr);

    auto boundary = std::make_shared<const ScreenBoundary>(ScreenBoundary{1920, 1080, 0, 0});
    cache.publishSnapshot(boundary);

    CFX_TEST_CHECK(cache.isValid());
    auto retrieved = cache.getSnapshot();
    CFX_TEST_CHECK(retrieved != nullptr);
    CFX_TEST_CHECK(retrieved->width == 1920);
    CFX_TEST_CHECK(retrieved->height == 1080);
}

static void test_screen_boundary_cache_reconfig_pending() {
    fprintf(stderr, "[TEST] test_screen_boundary_cache_reconfig_pending\n");
    ScreenBoundaryCache cache;

    CFX_TEST_CHECK(!cache.consumeReconfigPending());

    cache.setReconfigPending();
    CFX_TEST_CHECK(cache.consumeReconfigPending());
    CFX_TEST_CHECK(!cache.consumeReconfigPending());
}

static void test_multi_display_merge_policy_single() {
    fprintf(stderr, "[TEST] test_multi_display_merge_policy_single\n");
    MultiDisplayMergePolicyConfig config;

    auto policy = config.loadPolicy(1);
    CFX_TEST_CHECK(policy.has_value());
    CFX_TEST_CHECK(policy.value() == MultiDisplayMergePolicy::MergeBoundingBox);
}

static void test_multi_display_merge_policy_multi_undeclared() {
    fprintf(stderr, "[TEST] test_multi_display_merge_policy_multi_undeclared\n");
    MultiDisplayMergePolicyConfig config;

    auto policy = config.loadPolicy(2);
    CFX_TEST_CHECK(!policy.has_value());

    auto rejected = config.rejectUndeclared();
    CFX_TEST_CHECK(!rejected.isValid());
}

static void test_multi_display_merge_policy_multi_declared() {
    fprintf(stderr, "[TEST] test_multi_display_merge_policy_multi_declared\n");
    MultiDisplayMergePolicyConfig config;
    config.setPolicy(MultiDisplayMergePolicy::MergeBoundingBox);

    auto policy = config.loadPolicy(2);
    CFX_TEST_CHECK(policy.has_value());
    CFX_TEST_CHECK(policy.value() == MultiDisplayMergePolicy::MergeBoundingBox);
}

static void test_mac_screen_query_requery() {
    fprintf(stderr, "[TEST] test_mac_screen_query_requery\n");
    NativeCoordNormalizer normalizer;
    ScreenBoundaryCache cache;
    MultiDisplayMergePolicyConfig policyConfig;

    MacScreenQuery query(normalizer, cache, policyConfig);

    auto boundary = query.requeryAndNormalize();
    CFX_TEST_CHECK(boundary.isValid());
    CFX_TEST_CHECK(boundary.width > 0);
    CFX_TEST_CHECK(boundary.height > 0);

    CFX_TEST_CHECK(cache.isValid());
    auto snap = cache.getSnapshot();
    CFX_TEST_CHECK(snap != nullptr);
    CFX_TEST_CHECK(snap->width == boundary.width);
    CFX_TEST_CHECK(snap->height == boundary.height);
}

static void test_mac_screen_query_primary_boundary_empty_cache() {
    fprintf(stderr, "[TEST] test_mac_screen_query_primary_boundary_empty_cache\n");
    NativeCoordNormalizer normalizer;
    ScreenBoundaryCache cache;
    MultiDisplayMergePolicyConfig policyConfig;

    MacScreenQuery query(normalizer, cache, policyConfig);

    CFX_TEST_CHECK(!cache.isValid());

    auto boundary = query.primaryBoundary();
    CFX_TEST_CHECK(boundary.isValid());
    CFX_TEST_CHECK(boundary.width > 0);
    CFX_TEST_CHECK(boundary.height > 0);

    CFX_TEST_CHECK(cache.isValid());
}

static void test_mac_screen_query_primary_boundary_cached() {
    fprintf(stderr, "[TEST] test_mac_screen_query_primary_boundary_cached\n");
    NativeCoordNormalizer normalizer;
    ScreenBoundaryCache cache;
    MultiDisplayMergePolicyConfig policyConfig;

    auto initialBoundary = std::make_shared<const ScreenBoundary>(ScreenBoundary{2560, 1440, 0, 0});
    cache.publishSnapshot(initialBoundary);

    MacScreenQuery query(normalizer, cache, policyConfig);

    auto boundary = query.primaryBoundary();
    CFX_TEST_CHECK(boundary.width == 2560);
    CFX_TEST_CHECK(boundary.height == 1440);
}

static void test_mac_screen_query_iscreen_query_interface() {
    fprintf(stderr, "[TEST] test_mac_screen_query_iscreen_query_interface\n");
    NativeCoordNormalizer normalizer;
    ScreenBoundaryCache cache;
    MultiDisplayMergePolicyConfig policyConfig;

    MacScreenQuery query(normalizer, cache, policyConfig);
    IScreenQuery& iface = query;

    auto boundary = iface.primaryBoundary();
    CFX_TEST_CHECK(boundary.isValid());
    CFX_TEST_CHECK(boundary.width > 0);
    CFX_TEST_CHECK(boundary.height > 0);
}

int main() {
    fprintf(stderr, "=== test_mac_screen_query ===\n");

    test_native_coord_normalizer_single_display();
    test_native_coord_normalizer_multi_display();
    test_native_coord_normalizer_pixels_to_points();
    test_screen_boundary_cache_basic();
    test_screen_boundary_cache_reconfig_pending();
    test_multi_display_merge_policy_single();
    test_multi_display_merge_policy_multi_undeclared();
    test_multi_display_merge_policy_multi_declared();
    test_mac_screen_query_requery();
    test_mac_screen_query_primary_boundary_empty_cache();
    test_mac_screen_query_primary_boundary_cached();
    test_mac_screen_query_iscreen_query_interface();

    fprintf(stderr, "=== ALL PASS ===\n");
    return 0;
}