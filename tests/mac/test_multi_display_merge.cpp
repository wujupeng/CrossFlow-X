#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <vector>

#include "common/domain.hpp"
#include "mac/multi_display_merge_policy.hpp"

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

static ScreenBoundary makeBoundary(uint32_t w, uint32_t h, uint32_t ox = 0, uint32_t oy = 0) {
    return ScreenBoundary{w, h, ox, oy};
}

// --- loadPolicy: single display ---

static void test_loadPolicy_single_display_returns_nullopt() {
    fprintf(stderr, "[TEST] test_loadPolicy_single_display_returns_nullopt\n");
    MultiDisplayMergePolicyConfig config;
    auto policy = config.loadPolicy(1);
    CFX_TEST_CHECK(!policy.has_value());
}

static void test_loadPolicy_zero_displays_returns_nullopt() {
    fprintf(stderr, "[TEST] test_loadPolicy_zero_displays_returns_nullopt\n");
    MultiDisplayMergePolicyConfig config;
    auto policy = config.loadPolicy(0);
    CFX_TEST_CHECK(!policy.has_value());
}

// --- loadPolicy: multi display ---

static void test_loadPolicy_multi_display_undeclared_returns_nullopt() {
    fprintf(stderr, "[TEST] test_loadPolicy_multi_display_undeclared_returns_nullopt\n");
    MultiDisplayMergePolicyConfig config;
    auto policy = config.loadPolicy(2);
    CFX_TEST_CHECK(!policy.has_value());
}

static void test_loadPolicy_multi_display_merge_bbox() {
    fprintf(stderr, "[TEST] test_loadPolicy_multi_display_merge_bbox\n");
    MultiDisplayMergePolicyConfig config;
    config.setPolicy(MultiDisplayMergePolicy::MergeBoundingBox);
    auto policy = config.loadPolicy(2);
    CFX_TEST_CHECK(policy.has_value());
    CFX_TEST_CHECK(policy.value() == MultiDisplayMergePolicy::MergeBoundingBox);
}

static void test_loadPolicy_multi_display_unsupported() {
    fprintf(stderr, "[TEST] test_loadPolicy_multi_display_unsupported\n");
    MultiDisplayMergePolicyConfig config;
    config.setPolicy(MultiDisplayMergePolicy::Unsupported);
    auto policy = config.loadPolicy(2);
    CFX_TEST_CHECK(policy.has_value());
    CFX_TEST_CHECK(policy.value() == MultiDisplayMergePolicy::Unsupported);
}

static void test_loadPolicy_multi_display_three_displays() {
    fprintf(stderr, "[TEST] test_loadPolicy_multi_display_three_displays\n");
    MultiDisplayMergePolicyConfig config;
    config.setPolicy(MultiDisplayMergePolicy::MergeBoundingBox);
    auto policy = config.loadPolicy(3);
    CFX_TEST_CHECK(policy.has_value());
    CFX_TEST_CHECK(policy.value() == MultiDisplayMergePolicy::MergeBoundingBox);
}

// --- getPolicy ---

static void test_getPolicy_initially_nullopt() {
    fprintf(stderr, "[TEST] test_getPolicy_initially_nullopt\n");
    MultiDisplayMergePolicyConfig config;
    CFX_TEST_CHECK(!config.getPolicy().has_value());
}

static void test_getPolicy_after_set() {
    fprintf(stderr, "[TEST] test_getPolicy_after_set\n");
    MultiDisplayMergePolicyConfig config;
    config.setPolicy(MultiDisplayMergePolicy::MergeBoundingBox);
    CFX_TEST_CHECK(config.getPolicy().has_value());
    CFX_TEST_CHECK(config.getPolicy().value() == MultiDisplayMergePolicy::MergeBoundingBox);
}

static void test_getPolicy_runtime_immutable() {
    fprintf(stderr, "[TEST] test_getPolicy_runtime_immutable\n");
    MultiDisplayMergePolicyConfig config;
    config.setPolicy(MultiDisplayMergePolicy::MergeBoundingBox);
    auto p1 = config.getPolicy();
    auto p2 = config.getPolicy();
    CFX_TEST_CHECK(p1.has_value() && p2.has_value());
    CFX_TEST_CHECK(p1.value() == p2.value());
}

// --- rejectUndeclared ---

static void test_rejectUndeclared_returns_invalid() {
    fprintf(stderr, "[TEST] test_rejectUndeclared_returns_invalid\n");
    MultiDisplayMergePolicyConfig config;
    auto rejected = config.rejectUndeclared();
    CFX_TEST_CHECK(!rejected.isValid());
}

static void test_rejectUndeclared_zero_width_height() {
    fprintf(stderr, "[TEST] test_rejectUndeclared_zero_width_height\n");
    MultiDisplayMergePolicyConfig config;
    auto rejected = config.rejectUndeclared();
    CFX_TEST_CHECK(rejected.width == 0);
    CFX_TEST_CHECK(rejected.height == 0);
    CFX_TEST_CHECK(rejected.originX == 0);
    CFX_TEST_CHECK(rejected.originY == 0);
}

// --- mergeBoundingBox: horizontal arrangement ---

static void test_mergeBoundingBox_horizontal_two_displays() {
    fprintf(stderr, "[TEST] test_mergeBoundingBox_horizontal_two_displays\n");
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(1920, 1080, 1920, 0),
    };
    auto merged = MultiDisplayMergePolicyConfig::mergeBoundingBox(displays);
    CFX_TEST_CHECK(merged.isValid());
    CFX_TEST_CHECK(merged.width == 3840);
    CFX_TEST_CHECK(merged.height == 1080);
    CFX_TEST_CHECK(merged.originX == 0);
    CFX_TEST_CHECK(merged.originY == 0);
}

static void test_mergeBoundingBox_horizontal_three_displays() {
    fprintf(stderr, "[TEST] test_mergeBoundingBox_horizontal_three_displays\n");
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(1920, 1080, 1920, 0),
        makeBoundary(1920, 1080, 3840, 0),
    };
    auto merged = MultiDisplayMergePolicyConfig::mergeBoundingBox(displays);
    CFX_TEST_CHECK(merged.isValid());
    CFX_TEST_CHECK(merged.width == 5760);
    CFX_TEST_CHECK(merged.height == 1080);
    CFX_TEST_CHECK(merged.originX == 0);
    CFX_TEST_CHECK(merged.originY == 0);
}

// --- mergeBoundingBox: vertical arrangement ---

static void test_mergeBoundingBox_vertical_two_displays() {
    fprintf(stderr, "[TEST] test_mergeBoundingBox_vertical_two_displays\n");
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(1920, 1080, 0, 1080),
    };
    auto merged = MultiDisplayMergePolicyConfig::mergeBoundingBox(displays);
    CFX_TEST_CHECK(merged.isValid());
    CFX_TEST_CHECK(merged.width == 1920);
    CFX_TEST_CHECK(merged.height == 2160);
    CFX_TEST_CHECK(merged.originX == 0);
    CFX_TEST_CHECK(merged.originY == 0);
}

// --- mergeBoundingBox: gaps between displays ---

static void test_mergeBoundingBox_with_horizontal_gap() {
    fprintf(stderr, "[TEST] test_mergeBoundingBox_with_horizontal_gap\n");
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(1920, 1080, 2000, 0),
    };
    auto merged = MultiDisplayMergePolicyConfig::mergeBoundingBox(displays);
    CFX_TEST_CHECK(merged.isValid());
    CFX_TEST_CHECK(merged.width == 3920);
    CFX_TEST_CHECK(merged.height == 1080);
    CFX_TEST_CHECK(merged.originX == 0);
    CFX_TEST_CHECK(merged.originY == 0);
}

static void test_mergeBoundingBox_with_vertical_gap() {
    fprintf(stderr, "[TEST] test_mergeBoundingBox_with_vertical_gap\n");
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(1920, 1080, 0, 1200),
    };
    auto merged = MultiDisplayMergePolicyConfig::mergeBoundingBox(displays);
    CFX_TEST_CHECK(merged.isValid());
    CFX_TEST_CHECK(merged.width == 1920);
    CFX_TEST_CHECK(merged.height == 2280);
    CFX_TEST_CHECK(merged.originX == 0);
    CFX_TEST_CHECK(merged.originY == 0);
}

// --- mergeBoundingBox: offset origin ---

static void test_mergeBoundingBox_offset_origin() {
    fprintf(stderr, "[TEST] test_mergeBoundingBox_offset_origin\n");
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 100, 50),
        makeBoundary(1920, 1080, 2020, 50),
    };
    auto merged = MultiDisplayMergePolicyConfig::mergeBoundingBox(displays);
    CFX_TEST_CHECK(merged.isValid());
    CFX_TEST_CHECK(merged.width == 3840);
    CFX_TEST_CHECK(merged.height == 1080);
    CFX_TEST_CHECK(merged.originX == 0);
    CFX_TEST_CHECK(merged.originY == 0);
}

// --- mergeBoundingBox: different sizes ---

static void test_mergeBoundingBox_different_sizes() {
    fprintf(stderr, "[TEST] test_mergeBoundingBox_different_sizes\n");
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(2560, 1440, 1920, 0),
    };
    auto merged = MultiDisplayMergePolicyConfig::mergeBoundingBox(displays);
    CFX_TEST_CHECK(merged.isValid());
    CFX_TEST_CHECK(merged.width == 4480);
    CFX_TEST_CHECK(merged.height == 1440);
    CFX_TEST_CHECK(merged.originX == 0);
    CFX_TEST_CHECK(merged.originY == 0);
}

// --- mergeBoundingBox: edge cases ---

static void test_mergeBoundingBox_single_display() {
    fprintf(stderr, "[TEST] test_mergeBoundingBox_single_display\n");
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
    };
    auto merged = MultiDisplayMergePolicyConfig::mergeBoundingBox(displays);
    CFX_TEST_CHECK(merged.isValid());
    CFX_TEST_CHECK(merged.width == 1920);
    CFX_TEST_CHECK(merged.height == 1080);
    CFX_TEST_CHECK(merged.originX == 0);
    CFX_TEST_CHECK(merged.originY == 0);
}

static void test_mergeBoundingBox_empty_returns_invalid() {
    fprintf(stderr, "[TEST] test_mergeBoundingBox_empty_returns_invalid\n");
    std::vector<ScreenBoundary> displays;
    auto merged = MultiDisplayMergePolicyConfig::mergeBoundingBox(displays);
    CFX_TEST_CHECK(!merged.isValid());
}

// --- merge: full state machine ---

static void test_merge_single_display() {
    fprintf(stderr, "[TEST] test_merge_single_display\n");
    MultiDisplayMergePolicyConfig config;
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
    };
    auto result = config.merge(displays);
    CFX_TEST_CHECK(result.isValid());
    CFX_TEST_CHECK(result.width == 1920);
    CFX_TEST_CHECK(result.height == 1080);
    CFX_TEST_CHECK(result.originX == 0);
    CFX_TEST_CHECK(result.originY == 0);
}

static void test_merge_single_display_with_origin_normalized() {
    fprintf(stderr, "[TEST] test_merge_single_display_with_origin_normalized\n");
    MultiDisplayMergePolicyConfig config;
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 500, 300),
    };
    auto result = config.merge(displays);
    CFX_TEST_CHECK(result.isValid());
    CFX_TEST_CHECK(result.width == 1920);
    CFX_TEST_CHECK(result.height == 1080);
    CFX_TEST_CHECK(result.originX == 0);
    CFX_TEST_CHECK(result.originY == 0);
}

static void test_merge_multi_display_merge_bbox() {
    fprintf(stderr, "[TEST] test_merge_multi_display_merge_bbox\n");
    MultiDisplayMergePolicyConfig config;
    config.setPolicy(MultiDisplayMergePolicy::MergeBoundingBox);
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(1920, 1080, 1920, 0),
    };
    auto result = config.merge(displays);
    CFX_TEST_CHECK(result.isValid());
    CFX_TEST_CHECK(result.width == 3840);
    CFX_TEST_CHECK(result.height == 1080);
    CFX_TEST_CHECK(result.originX == 0);
    CFX_TEST_CHECK(result.originY == 0);
}

static void test_merge_multi_display_unsupported() {
    fprintf(stderr, "[TEST] test_merge_multi_display_unsupported\n");
    MultiDisplayMergePolicyConfig config;
    config.setPolicy(MultiDisplayMergePolicy::Unsupported);
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(1920, 1080, 1920, 0),
    };
    auto result = config.merge(displays);
    CFX_TEST_CHECK(!result.isValid());
}

static void test_merge_multi_display_undeclared() {
    fprintf(stderr, "[TEST] test_merge_multi_display_undeclared\n");
    MultiDisplayMergePolicyConfig config;
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(1920, 1080, 1920, 0),
    };
    auto result = config.merge(displays);
    CFX_TEST_CHECK(!result.isValid());
}

static void test_merge_empty_returns_invalid() {
    fprintf(stderr, "[TEST] test_merge_empty_returns_invalid\n");
    MultiDisplayMergePolicyConfig config;
    std::vector<ScreenBoundary> displays;
    auto result = config.merge(displays);
    CFX_TEST_CHECK(!result.isValid());
}

static void test_merge_three_displays_merge_bbox() {
    fprintf(stderr, "[TEST] test_merge_three_displays_merge_bbox\n");
    MultiDisplayMergePolicyConfig config;
    config.setPolicy(MultiDisplayMergePolicy::MergeBoundingBox);
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(1920, 1080, 1920, 0),
        makeBoundary(1920, 1080, 3840, 0),
    };
    auto result = config.merge(displays);
    CFX_TEST_CHECK(result.isValid());
    CFX_TEST_CHECK(result.width == 5760);
    CFX_TEST_CHECK(result.height == 1080);
    CFX_TEST_CHECK(result.originX == 0);
    CFX_TEST_CHECK(result.originY == 0);
}

// --- Closure-1 C1-5: multi-display undeclared -> isValid() == false ---

static void test_closure1_c1_5_undeclared_invalid() {
    fprintf(stderr, "[TEST] test_closure1_c1_5_undeclared_invalid\n");
    MultiDisplayMergePolicyConfig config;
    std::vector<ScreenBoundary> displays = {
        makeBoundary(1920, 1080, 0, 0),
        makeBoundary(2560, 1440, 1920, 0),
    };
    auto result = config.merge(displays);
    CFX_TEST_CHECK(!result.isValid());
}

int main() {
    fprintf(stderr, "=== test_multi_display_merge ===\n");

    test_loadPolicy_single_display_returns_nullopt();
    test_loadPolicy_zero_displays_returns_nullopt();
    test_loadPolicy_multi_display_undeclared_returns_nullopt();
    test_loadPolicy_multi_display_merge_bbox();
    test_loadPolicy_multi_display_unsupported();
    test_loadPolicy_multi_display_three_displays();

    test_getPolicy_initially_nullopt();
    test_getPolicy_after_set();
    test_getPolicy_runtime_immutable();

    test_rejectUndeclared_returns_invalid();
    test_rejectUndeclared_zero_width_height();

    test_mergeBoundingBox_horizontal_two_displays();
    test_mergeBoundingBox_horizontal_three_displays();
    test_mergeBoundingBox_vertical_two_displays();
    test_mergeBoundingBox_with_horizontal_gap();
    test_mergeBoundingBox_with_vertical_gap();
    test_mergeBoundingBox_offset_origin();
    test_mergeBoundingBox_different_sizes();
    test_mergeBoundingBox_single_display();
    test_mergeBoundingBox_empty_returns_invalid();

    test_merge_single_display();
    test_merge_single_display_with_origin_normalized();
    test_merge_multi_display_merge_bbox();
    test_merge_multi_display_unsupported();
    test_merge_multi_display_undeclared();
    test_merge_empty_returns_invalid();
    test_merge_three_displays_merge_bbox();

    test_closure1_c1_5_undeclared_invalid();

    fprintf(stderr, "=== ALL PASS ===\n");
    return 0;
}