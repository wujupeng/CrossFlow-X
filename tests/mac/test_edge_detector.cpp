#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <vector>

#include "common/domain.hpp"
#include "mac/edge_detector.hpp"
#include "mac/edge_overflow_spsc.hpp"

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

static ScreenBoundary makeBoundary(uint32_t w, uint32_t h) {
    return ScreenBoundary{w, h, 0, 0};
}

// --- 左越界 ---

static void test_left_overflow_basic() {
    fprintf(stderr, "[TEST] test_left_overflow_basic\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(-10, 200, boundary);
    CFX_TEST_CHECK(result.has_value());
    CFX_TEST_CHECK(result->direction == EdgeDirection::Left);
    CFX_TEST_CHECK(result->overflow == 10);
    CFX_TEST_CHECK(result->cursorY == 200);
}

static void test_left_overflow_large() {
    fprintf(stderr, "[TEST] test_left_overflow_large\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(-1000, 540, boundary);
    CFX_TEST_CHECK(result.has_value());
    CFX_TEST_CHECK(result->direction == EdgeDirection::Left);
    CFX_TEST_CHECK(result->overflow == 1000);
    CFX_TEST_CHECK(result->cursorY == 540);
}

static void test_left_overflow_at_origin() {
    fprintf(stderr, "[TEST] test_left_overflow_at_origin\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(-1, 0, boundary);
    CFX_TEST_CHECK(result.has_value());
    CFX_TEST_CHECK(result->overflow == 1);
    CFX_TEST_CHECK(result->cursorY == 0);
}

// --- 右越界 ---

static void test_right_overflow_basic() {
    fprintf(stderr, "[TEST] test_right_overflow_basic\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(1920 + 10, 200, boundary);
    CFX_TEST_CHECK(result.has_value());
    CFX_TEST_CHECK(result->direction == EdgeDirection::Right);
    CFX_TEST_CHECK(result->overflow == 10);
    CFX_TEST_CHECK(result->cursorY == 200);
}

static void test_right_overflow_large() {
    fprintf(stderr, "[TEST] test_right_overflow_large\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(1920 + 500, 1080, boundary);
    CFX_TEST_CHECK(result.has_value());
    CFX_TEST_CHECK(result->direction == EdgeDirection::Right);
    CFX_TEST_CHECK(result->overflow == 500);
    CFX_TEST_CHECK(result->cursorY == 1080);
}

static void test_right_overflow_at_edge() {
    fprintf(stderr, "[TEST] test_right_overflow_at_edge\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(1921, 540, boundary);
    CFX_TEST_CHECK(result.has_value());
    CFX_TEST_CHECK(result->overflow == 1);
}

// --- 上下不触发 ---

static void test_top_no_trigger() {
    fprintf(stderr, "[TEST] test_top_no_trigger\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(-10, -1, boundary);
    CFX_TEST_CHECK(!result.has_value());

    auto result2 = detector.detect(500, -1, boundary);
    CFX_TEST_CHECK(!result2.has_value());

    auto result3 = detector.detect(2000, -1, boundary);
    CFX_TEST_CHECK(!result3.has_value());
}

static void test_bottom_no_trigger() {
    fprintf(stderr, "[TEST] test_bottom_no_trigger\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(-10, 1081, boundary);
    CFX_TEST_CHECK(!result.has_value());

    auto result2 = detector.detect(500, 1081, boundary);
    CFX_TEST_CHECK(!result2.has_value());

    auto result3 = detector.detect(2000, 1081, boundary);
    CFX_TEST_CHECK(!result3.has_value());
}

// --- cursorY 越界返回 nullopt（禁止 clamp）---

static void test_cursorY_overflow_no_clamp_left() {
    fprintf(stderr, "[TEST] test_cursorY_overflow_no_clamp_left\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(-10, -100, boundary);
    CFX_TEST_CHECK(!result.has_value());

    auto result2 = detector.detect(-10, 2000, boundary);
    CFX_TEST_CHECK(!result2.has_value());
}

static void test_cursorY_overflow_no_clamp_right() {
    fprintf(stderr, "[TEST] test_cursorY_overflow_no_clamp_right\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(2000, -100, boundary);
    CFX_TEST_CHECK(!result.has_value());

    auto result2 = detector.detect(2000, 2000, boundary);
    CFX_TEST_CHECK(!result2.has_value());
}

// --- 未越界 ---

static void test_no_overflow_inside() {
    fprintf(stderr, "[TEST] test_no_overflow_inside\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto result = detector.detect(0, 0, boundary);
    CFX_TEST_CHECK(!result.has_value());

    auto result2 = detector.detect(1920, 1080, boundary);
    CFX_TEST_CHECK(!result2.has_value());

    auto result3 = detector.detect(960, 540, boundary);
    CFX_TEST_CHECK(!result3.has_value());
}

// --- overflow 自然非负 u32 ---

static void test_overflow_type_safety() {
    fprintf(stderr, "[TEST] test_overflow_type_safety\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    auto left = detector.detect(-1, 0, boundary);
    CFX_TEST_CHECK(left.has_value());
    CFX_TEST_CHECK(left->overflow == 1u);

    auto right = detector.detect(1921, 0, boundary);
    CFX_TEST_CHECK(right.has_value());
    CFX_TEST_CHECK(right->overflow == 1u);

    CFX_TEST_CHECK(left->cursorY == 0u);
    CFX_TEST_CHECK(right->cursorY == 0u);
}

// --- invalid boundary ---

static void test_invalid_boundary() {
    fprintf(stderr, "[TEST] test_invalid_boundary\n");
    EdgeDetector detector;
    auto invalid = ScreenBoundary{0, 0, 0, 0};

    auto result = detector.detect(-10, 200, invalid);
    CFX_TEST_CHECK(!result.has_value());

    auto result2 = detector.detect(100, 100, invalid);
    CFX_TEST_CHECK(!result2.has_value());
}

// --- 确定性 ---

static void test_determinism() {
    fprintf(stderr, "[TEST] test_determinism\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    for (int i = 0; i < 100; ++i) {
        auto r1 = detector.detect(-50, 200, boundary);
        auto r2 = detector.detect(-50, 200, boundary);
        CFX_TEST_CHECK(r1.has_value() == r2.has_value());
        if (r1 && r2) {
            CFX_TEST_CHECK(r1->direction == r2->direction);
            CFX_TEST_CHECK(r1->overflow == r2->overflow);
            CFX_TEST_CHECK(r1->cursorY == r2->cursorY);
        }
    }
}

// --- ≤500µs 延迟验证 ---

static void test_latency_under_500us() {
    fprintf(stderr, "[TEST] test_latency_under_500us\n");
    EdgeDetector detector;
    auto boundary = makeBoundary(1920, 1080);

    constexpr int ITERATIONS = 10000;
    int32_t cursorXs[] = {-100, 0, 500, 1920, 2000};
    int32_t cursorYs[] = {-1, 0, 540, 1080, 1081};
    int numCases = 5;

    uint64_t maxNs = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        int32_t cx = cursorXs[i % numCases];
        int32_t cy = cursorYs[(i / numCases) % numCases];

        auto start = std::chrono::high_resolution_clock::now();
        auto result = detector.detect(cx, cy, boundary);
        auto end = std::chrono::high_resolution_clock::now();

        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        if (static_cast<uint64_t>(ns) > maxNs) {
            maxNs = static_cast<uint64_t>(ns);
        }
        (void)result;
    }

    fprintf(stderr, "  max latency: %llu ns (%.3f µs)\n",
            static_cast<unsigned long long>(maxNs),
            static_cast<double>(maxNs) / 1000.0);
    CFX_TEST_CHECK(maxNs < 500000);
}

// --- EdgeOverflowSpsc 基本操作 ---

static void test_edge_overflow_spsc_basic() {
    fprintf(stderr, "[TEST] test_edge_overflow_spsc_basic\n");
    EdgeOverflowSpsc spsc;

    CFX_TEST_CHECK(spsc.isEmpty());
    CFX_TEST_CHECK(spsc.droppedOldestCount() == 0);
    CFX_TEST_CHECK(spsc.head() == 0);
    CFX_TEST_CHECK(spsc.tail() == 0);

    EdgeOverflowEvent event{EdgeDirection::Left, 10, 200};
    CFX_TEST_CHECK(spsc.tryPushDropOldest(event));
    CFX_TEST_CHECK(!spsc.isEmpty());
    CFX_TEST_CHECK(spsc.head() == 1);
    CFX_TEST_CHECK(spsc.tail() == 0);

    EdgeOverflowEvent out;
    CFX_TEST_CHECK(spsc.tryPop(out));
    CFX_TEST_CHECK(out.direction == EdgeDirection::Left);
    CFX_TEST_CHECK(out.overflow == 10);
    CFX_TEST_CHECK(out.cursorY == 200);
    CFX_TEST_CHECK(spsc.isEmpty());
}

static void test_edge_overflow_spsc_capacity() {
    fprintf(stderr, "[TEST] test_edge_overflow_spsc_capacity\n");
    CFX_TEST_CHECK(EdgeOverflowSpsc::CAPACITY == 64);
}

static void test_edge_overflow_spsc_fill_and_drain() {
    fprintf(stderr, "[TEST] test_edge_overflow_spsc_fill_and_drain\n");
    EdgeOverflowSpsc spsc;

    for (uint32_t i = 0; i < 64; ++i) {
        EdgeOverflowEvent e{EdgeDirection::Left, i, i * 2};
        CFX_TEST_CHECK(spsc.tryPushDropOldest(e));
    }
    CFX_TEST_CHECK(!spsc.isEmpty());

    for (uint32_t i = 0; i < 64; ++i) {
        EdgeOverflowEvent out;
        CFX_TEST_CHECK(spsc.tryPop(out));
        CFX_TEST_CHECK(out.overflow == i);
        CFX_TEST_CHECK(out.cursorY == i * 2);
    }
    CFX_TEST_CHECK(spsc.isEmpty());
}

static void test_edge_overflow_spsc_drop_oldest() {
    fprintf(stderr, "[TEST] test_edge_overflow_spsc_drop_oldest\n");
    EdgeOverflowSpsc spsc;

    for (uint32_t i = 0; i < 64; ++i) {
        EdgeOverflowEvent e{EdgeDirection::Right, i, 0};
        spsc.tryPushDropOldest(e);
    }
    CFX_TEST_CHECK(spsc.droppedOldestCount() == 0);

    EdgeOverflowEvent extra{EdgeDirection::Left, 999, 0};
    spsc.tryPushDropOldest(extra);
    CFX_TEST_CHECK(spsc.droppedOldestCount() >= 1);

    EdgeOverflowEvent out;
    uint32_t popped = 0;
    while (spsc.tryPop(out)) {
        ++popped;
    }
    CFX_TEST_CHECK(popped == 64);
}

static void test_edge_overflow_spsc_mixed_directions() {
    fprintf(stderr, "[TEST] test_edge_overflow_spsc_mixed_directions\n");
    EdgeOverflowSpsc spsc;

    spsc.tryPushDropOldest({EdgeDirection::Left, 10, 100});
    spsc.tryPushDropOldest({EdgeDirection::Right, 20, 200});
    spsc.tryPushDropOldest({EdgeDirection::Left, 30, 300});

    EdgeOverflowEvent out;
    CFX_TEST_CHECK(spsc.tryPop(out));
    CFX_TEST_CHECK(out.direction == EdgeDirection::Left);
    CFX_TEST_CHECK(out.overflow == 10);

    CFX_TEST_CHECK(spsc.tryPop(out));
    CFX_TEST_CHECK(out.direction == EdgeDirection::Right);
    CFX_TEST_CHECK(out.overflow == 20);

    CFX_TEST_CHECK(spsc.tryPop(out));
    CFX_TEST_CHECK(out.direction == EdgeDirection::Left);
    CFX_TEST_CHECK(out.overflow == 30);

    CFX_TEST_CHECK(spsc.isEmpty());
}

int main() {
    fprintf(stderr, "=== test_edge_detector ===\n");

    test_left_overflow_basic();
    test_left_overflow_large();
    test_left_overflow_at_origin();
    test_right_overflow_basic();
    test_right_overflow_large();
    test_right_overflow_at_edge();
    test_top_no_trigger();
    test_bottom_no_trigger();
    test_cursorY_overflow_no_clamp_left();
    test_cursorY_overflow_no_clamp_right();
    test_no_overflow_inside();
    test_overflow_type_safety();
    test_invalid_boundary();
    test_determinism();
    test_latency_under_500us();
    test_edge_overflow_spsc_basic();
    test_edge_overflow_spsc_capacity();
    test_edge_overflow_spsc_fill_and_drain();
    test_edge_overflow_spsc_drop_oldest();
    test_edge_overflow_spsc_mixed_directions();

    fprintf(stderr, "=== ALL PASS ===\n");
    return 0;
}