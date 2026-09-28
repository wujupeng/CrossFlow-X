#include <cstdio>
#include <cstdlib>
#include <vector>

#include "common/domain.hpp"
#include "mac/native_coord_normalizer.hpp"

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

static NativeScreenGeometry makeGeometry(int64_t ox, int64_t oy, uint32_t w, uint32_t h,
                                          NativeScreenGeometry::ApiSource src = NativeScreenGeometry::ApiSource::CGDisplay,
                                          double scale = 1.0) {
    NativeScreenGeometry g;
    g.originX = ox;
    g.originY = oy;
    g.width = w;
    g.height = h;
    g.apiSource = src;
    g.backingScaleFactor = scale;
    return g;
}

// --- 坐标原点定义 ---

static void test_cgdisplay_origin_left_top() {
    fprintf(stderr, "[TEST] test_cgdisplay_origin_left_top\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::CGDisplay);

    auto result = n.normalizeSingleDisplay({0, 0}, geom);
    CFX_TEST_CHECK(result.x == 0);
    CFX_TEST_CHECK(result.y == 0);
}

static void test_nsscreen_origin_left_bottom_y_flip() {
    fprintf(stderr, "[TEST] test_nsscreen_origin_left_bottom_y_flip\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::NSScreen);

    auto bottom = n.normalizeSingleDisplay({0, 0}, geom);
    CFX_TEST_CHECK(bottom.x == 0);
    CFX_TEST_CHECK(bottom.y == 1080);

    auto top = n.normalizeSingleDisplay({0, 1080}, geom);
    CFX_TEST_CHECK(top.x == 0);
    CFX_TEST_CHECK(top.y == 0);

    auto mid = n.normalizeSingleDisplay({0, 540}, geom);
    CFX_TEST_CHECK(mid.y == 540);
}

// --- Y 轴方向 ---

static void test_cgdisplay_y_axis_down() {
    fprintf(stderr, "[TEST] test_cgdisplay_y_axis_down\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::CGDisplay);

    auto p1 = n.normalizeSingleDisplay({0, 100}, geom);
    auto p2 = n.normalizeSingleDisplay({0, 200}, geom);
    CFX_TEST_CHECK(p2.y > p1.y);
}

static void test_nsscreen_y_axis_up_to_down() {
    fprintf(stderr, "[TEST] test_nsscreen_y_axis_up_to_down\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::NSScreen);

    auto p1 = n.normalizeSingleDisplay({0, 100}, geom);
    auto p2 = n.normalizeSingleDisplay({0, 200}, geom);
    CFX_TEST_CHECK(p2.y < p1.y);
    CFX_TEST_CHECK(p1.y == 980);
    CFX_TEST_CHECK(p2.y == 880);
}

// --- Retina / backing scale ---

static void test_retina_pixels_to_points_2x() {
    fprintf(stderr, "[TEST] test_retina_pixels_to_points_2x\n");
    NativeCoordNormalizer n;
    CFX_TEST_CHECK(n.pixelsToPoints(2880, 2.0) == 1440);
    CFX_TEST_CHECK(n.pixelsToPoints(2160, 2.0) == 1080);
    CFX_TEST_CHECK(n.pixelsToPoints(100, 2.0) == 50);
}

static void test_non_retina_pixels_to_points_1x() {
    fprintf(stderr, "[TEST] test_non_retina_pixels_to_points_1x\n");
    NativeCoordNormalizer n;
    CFX_TEST_CHECK(n.pixelsToPoints(1920, 1.0) == 1920);
    CFX_TEST_CHECK(n.pixelsToPoints(1080, 1.0) == 1080);
}

static void test_retina_3x_scaling() {
    fprintf(stderr, "[TEST] test_retina_3x_scaling\n");
    NativeCoordNormalizer n;
    CFX_TEST_CHECK(n.pixelsToPoints(3024, 3.0) == 1008);
    CFX_TEST_CHECK(n.pixelsToPoints(300, 3.0) == 100);
}

static void test_zero_scale_fallback() {
    fprintf(stderr, "[TEST] test_zero_scale_fallback\n");
    NativeCoordNormalizer n;
    CFX_TEST_CHECK(n.pixelsToPoints(1920, 0.0) == 1920);
    CFX_TEST_CHECK(n.pixelsToPoints(100, -1.0) == 100);
}

// --- 多显示器坐标 ---

static void test_multi_display_horizontal_arrangement() {
    fprintf(stderr, "[TEST] test_multi_display_horizontal_arrangement\n");
    NativeCoordNormalizer n;
    std::vector<NativeScreenGeometry> displays = {
        makeGeometry(0, 0, 1920, 1080),
        makeGeometry(1920, 0, 1920, 1080),
    };

    auto bbox = n.computeBoundingBox(displays);
    CFX_TEST_CHECK(bbox.left == 0);
    CFX_TEST_CHECK(bbox.top == 0);
    CFX_TEST_CHECK(bbox.right == 3840);
    CFX_TEST_CHECK(bbox.bottom == 1080);

    auto on_first = n.normalizeMultiDisplay({100, 200}, bbox);
    CFX_TEST_CHECK(on_first.x == 100);
    CFX_TEST_CHECK(on_first.y == 200);

    auto on_second = n.normalizeMultiDisplay({1920 + 100, 200}, bbox);
    CFX_TEST_CHECK(on_second.x == 2020);
    CFX_TEST_CHECK(on_second.y == 200);
}

static void test_multi_display_vertical_arrangement() {
    fprintf(stderr, "[TEST] test_multi_display_vertical_arrangement\n");
    NativeCoordNormalizer n;
    std::vector<NativeScreenGeometry> displays = {
        makeGeometry(0, 0, 1920, 1080),
        makeGeometry(0, 1080, 1920, 1080),
    };

    auto bbox = n.computeBoundingBox(displays);
    CFX_TEST_CHECK(bbox.left == 0);
    CFX_TEST_CHECK(bbox.top == 0);
    CFX_TEST_CHECK(bbox.right == 1920);
    CFX_TEST_CHECK(bbox.bottom == 2160);

    auto on_top = n.normalizeMultiDisplay({100, 200}, bbox);
    CFX_TEST_CHECK(on_top.x == 100);
    CFX_TEST_CHECK(on_top.y == 200);

    auto on_bottom = n.normalizeMultiDisplay({100, 1080 + 200}, bbox);
    CFX_TEST_CHECK(on_bottom.x == 100);
    CFX_TEST_CHECK(on_bottom.y == 1280);
}

// --- 主显示器与非主显示器 ---

static void test_non_primary_display_negative_origin() {
    fprintf(stderr, "[TEST] test_non_primary_display_negative_origin\n");
    NativeCoordNormalizer n;
    std::vector<NativeScreenGeometry> displays = {
        makeGeometry(-1920, 0, 1920, 1080),
        makeGeometry(0, 0, 1920, 1080),
    };

    auto bbox = n.computeBoundingBox(displays);
    CFX_TEST_CHECK(bbox.left == -1920);
    CFX_TEST_CHECK(bbox.top == 0);
    CFX_TEST_CHECK(bbox.right == 1920);
    CFX_TEST_CHECK(bbox.bottom == 1080);

    auto on_left = n.normalizeMultiDisplay({-1920 + 100, 200}, bbox);
    CFX_TEST_CHECK(on_left.x == 100);
    CFX_TEST_CHECK(on_left.y == 200);

    auto on_right = n.normalizeMultiDisplay({100, 200}, bbox);
    CFX_TEST_CHECK(on_right.x == 2020);
}

// --- 负坐标 ---

static void test_negative_native_coord_cgdisplay() {
    fprintf(stderr, "[TEST] test_negative_native_coord_cgdisplay\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(-1920, 0, 1920, 1080, NativeScreenGeometry::ApiSource::CGDisplay);

    auto result = n.normalizeSingleDisplay({-1920 + 100, 200}, geom);
    CFX_TEST_CHECK(result.x == 100);
    CFX_TEST_CHECK(result.y == 200);
}

static void test_negative_native_coord_nsscreen() {
    fprintf(stderr, "[TEST] test_negative_native_coord_nsscreen\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(-1920, 0, 1920, 1080, NativeScreenGeometry::ApiSource::NSScreen);

    auto result = n.normalizeSingleDisplay({-1920 + 100, 200}, geom);
    CFX_TEST_CHECK(result.x == 100);
    CFX_TEST_CHECK(result.y == 880);
}

// --- Display bounds / toScreenBoundary ---

static void test_to_screen_boundary_origin_zero() {
    fprintf(stderr, "[TEST] test_to_screen_boundary_origin_zero\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(-1920, 100, 1920, 1080);

    auto boundary = n.toScreenBoundary(geom);
    CFX_TEST_CHECK(boundary.width == 1920);
    CFX_TEST_CHECK(boundary.height == 1080);
    CFX_TEST_CHECK(boundary.originX == 0);
    CFX_TEST_CHECK(boundary.originY == 0);
    CFX_TEST_CHECK(boundary.isValid());
}

static void test_to_screen_boundary_merged_origin_zero() {
    fprintf(stderr, "[TEST] test_to_screen_boundary_merged_origin_zero\n");
    NativeCoordNormalizer n;
    DisplayBoundingBox bbox{-1920, -500, 1920, 1080};

    auto boundary = n.toScreenBoundaryMerged(bbox);
    CFX_TEST_CHECK(boundary.width == 3840);
    CFX_TEST_CHECK(boundary.height == 1580);
    CFX_TEST_CHECK(boundary.originX == 0);
    CFX_TEST_CHECK(boundary.originY == 0);
    CFX_TEST_CHECK(boundary.isValid());
}

// --- 边界值 ---

static void test_boundary_values_cgdisplay() {
    fprintf(stderr, "[TEST] test_boundary_values_cgdisplay\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::CGDisplay);

    auto top_left = n.normalizeSingleDisplay({0, 0}, geom);
    CFX_TEST_CHECK(top_left.x == 0);
    CFX_TEST_CHECK(top_left.y == 0);

    auto bottom_right = n.normalizeSingleDisplay({1920, 1080}, geom);
    CFX_TEST_CHECK(bottom_right.x == 1920);
    CFX_TEST_CHECK(bottom_right.y == 1080);

    auto just_inside = n.normalizeSingleDisplay({1919, 1079}, geom);
    CFX_TEST_CHECK(just_inside.x == 1919);
    CFX_TEST_CHECK(just_inside.y == 1079);
}

static void test_boundary_values_nsscreen() {
    fprintf(stderr, "[TEST] test_boundary_values_nsscreen\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::NSScreen);

    auto bottom_left = n.normalizeSingleDisplay({0, 0}, geom);
    CFX_TEST_CHECK(bottom_left.x == 0);
    CFX_TEST_CHECK(bottom_left.y == 1080);

    auto top_right = n.normalizeSingleDisplay({1920, 1080}, geom);
    CFX_TEST_CHECK(top_right.x == 1920);
    CFX_TEST_CHECK(top_right.y == 0);
}

// --- 越界值 ---

static void test_overflow_values_cgdisplay() {
    fprintf(stderr, "[TEST] test_overflow_values_cgdisplay\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::CGDisplay);

    auto beyond_right = n.normalizeSingleDisplay({1920 + 100, 200}, geom);
    CFX_TEST_CHECK(beyond_right.x == 2020);

    auto beyond_bottom = n.normalizeSingleDisplay({100, 1080 + 200}, geom);
    CFX_TEST_CHECK(beyond_bottom.y == 1280);

    auto negative = n.normalizeSingleDisplay({-100, -200}, geom);
    CFX_TEST_CHECK(negative.x == -100);
    CFX_TEST_CHECK(negative.y == -200);
}

// --- D1→D2 转换确定性 ---

static void test_determinism_same_input_same_output() {
    fprintf(stderr, "[TEST] test_determinism_same_input_same_output\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(100, 200, 1920, 1080, NativeScreenGeometry::ApiSource::CGDisplay);

    NativeCoord input{500, 600};
    auto r1 = n.normalizeSingleDisplay(input, geom);
    auto r2 = n.normalizeSingleDisplay(input, geom);
    CFX_TEST_CHECK(r1.x == r2.x);
    CFX_TEST_CHECK(r1.y == r2.y);
}

static void test_determinism_nsscreen_y_flip_consistency() {
    fprintf(stderr, "[TEST] test_determinism_nsscreen_y_flip_consistency\n");
    NativeCoordNormalizer n;
    auto geom = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::NSScreen);

    for (int i = 0; i <= 1080; i += 100) {
        auto result = n.normalizeSingleDisplay({0, static_cast<int64_t>(i)}, geom);
        CFX_TEST_CHECK(result.y == 1080 - i);
    }
}

// --- 不允许隐式假设单显示器 ---

static void test_no_implicit_single_display_assumption() {
    fprintf(stderr, "[TEST] test_no_implicit_single_display_assumption\n");
    NativeCoordNormalizer n;
    std::vector<NativeScreenGeometry> three_displays = {
        makeGeometry(-1920, 0, 1920, 1080),
        makeGeometry(0, 0, 1920, 1080),
        makeGeometry(1920, 0, 1920, 1080),
    };

    auto bbox = n.computeBoundingBox(three_displays);
    CFX_TEST_CHECK(bbox.left == -1920);
    CFX_TEST_CHECK(bbox.right == 3840);
    CFX_TEST_CHECK(bbox.bottom - bbox.top == 1080);

    auto boundary = n.toScreenBoundaryMerged(bbox);
    CFX_TEST_CHECK(boundary.width == 5760);
    CFX_TEST_CHECK(boundary.height == 1080);
    CFX_TEST_CHECK(boundary.originX == 0);
    CFX_TEST_CHECK(boundary.originY == 0);
}

static void test_empty_displays_bbox() {
    fprintf(stderr, "[TEST] test_empty_displays_bbox\n");
    NativeCoordNormalizer n;
    std::vector<NativeScreenGeometry> empty;

    auto bbox = n.computeBoundingBox(empty);
    CFX_TEST_CHECK(bbox.left == 0);
    CFX_TEST_CHECK(bbox.top == 0);
    CFX_TEST_CHECK(bbox.right == 0);
    CFX_TEST_CHECK(bbox.bottom == 0);
}

// --- apiSource 分派 ---

static void test_api_source_dispatch() {
    fprintf(stderr, "[TEST] test_api_source_dispatch\n");
    NativeCoordNormalizer n;
    NativeCoord input{100, 200};

    auto geom_cg = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::CGDisplay);
    auto geom_ns = makeGeometry(0, 0, 1920, 1080, NativeScreenGeometry::ApiSource::NSScreen);

    auto cg_result = n.normalizeSingleDisplay(input, geom_cg);
    auto ns_result = n.normalizeSingleDisplay(input, geom_ns);

    CFX_TEST_CHECK(cg_result.x == ns_result.x);
    CFX_TEST_CHECK(cg_result.y == 200);
    CFX_TEST_CHECK(ns_result.y == 880);
    CFX_TEST_CHECK(cg_result.y != ns_result.y);
}

int main() {
    fprintf(stderr, "=== test_native_coord_normalizer ===\n");

    test_cgdisplay_origin_left_top();
    test_nsscreen_origin_left_bottom_y_flip();
    test_cgdisplay_y_axis_down();
    test_nsscreen_y_axis_up_to_down();
    test_retina_pixels_to_points_2x();
    test_non_retina_pixels_to_points_1x();
    test_retina_3x_scaling();
    test_zero_scale_fallback();
    test_multi_display_horizontal_arrangement();
    test_multi_display_vertical_arrangement();
    test_non_primary_display_negative_origin();
    test_negative_native_coord_cgdisplay();
    test_negative_native_coord_nsscreen();
    test_to_screen_boundary_origin_zero();
    test_to_screen_boundary_merged_origin_zero();
    test_boundary_values_cgdisplay();
    test_boundary_values_nsscreen();
    test_overflow_values_cgdisplay();
    test_determinism_same_input_same_output();
    test_determinism_nsscreen_y_flip_consistency();
    test_no_implicit_single_display_assumption();
    test_empty_displays_bbox();
    test_api_source_dispatch();

    fprintf(stderr, "=== ALL PASS ===\n");
    return 0;
}