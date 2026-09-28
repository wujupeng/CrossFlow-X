#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>

#include "common/domain.hpp"
#include "mac/screen_boundary_cache.hpp"

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

// --- 基本操作 ---

static void test_initial_state() {
    fprintf(stderr, "[TEST] test_initial_state\n");
    ScreenBoundaryCache cache;
    CFX_TEST_CHECK(!cache.isValid());
    CFX_TEST_CHECK(cache.getSnapshot() == nullptr);
    CFX_TEST_CHECK(!cache.consumeReconfigPending());
}

static void test_publish_and_get() {
    fprintf(stderr, "[TEST] test_publish_and_get\n");
    ScreenBoundaryCache cache;

    auto snap = std::make_shared<const ScreenBoundary>(ScreenBoundary{1920, 1080, 0, 0});
    cache.publishSnapshot(snap);

    CFX_TEST_CHECK(cache.isValid());
    auto retrieved = cache.getSnapshot();
    CFX_TEST_CHECK(retrieved != nullptr);
    CFX_TEST_CHECK(retrieved->width == 1920);
    CFX_TEST_CHECK(retrieved->height == 1080);
}

// --- Immutable Lifetime ---

static void test_immutable_snapshot() {
    fprintf(stderr, "[TEST] test_immutable_snapshot\n");
    ScreenBoundaryCache cache;

    auto snap1 = std::make_shared<const ScreenBoundary>(ScreenBoundary{1920, 1080, 0, 0});
    cache.publishSnapshot(snap1);

    auto ref1 = cache.getSnapshot();
    CFX_TEST_CHECK(ref1->width == 1920);

    auto snap2 = std::make_shared<const ScreenBoundary>(ScreenBoundary{2560, 1440, 0, 0});
    cache.publishSnapshot(snap2);

    auto ref1_still = ref1;
    CFX_TEST_CHECK(ref1_still->width == 1920);

    auto ref2 = cache.getSnapshot();
    CFX_TEST_CHECK(ref2->width == 2560);
}

// --- Single-Load Principle ---

static void test_single_load() {
    fprintf(stderr, "[TEST] test_single_load\n");
    ScreenBoundaryCache cache;

    auto snap = std::make_shared<const ScreenBoundary>(ScreenBoundary{1920, 1080, 0, 0});
    cache.publishSnapshot(snap);

    auto snapshot = cache.getSnapshot();
    CFX_TEST_CHECK(snapshot != nullptr);
    CFX_TEST_CHECK(snapshot->width == 1920);
    CFX_TEST_CHECK(snapshot->height == 1080);
}

// --- New Boundary Only Affects Subsequent detect() ---

static void test_publish_during_use_old_snapshot() {
    fprintf(stderr, "[TEST] test_publish_during_use_old_snapshot\n");
    ScreenBoundaryCache cache;

    auto snap1 = std::make_shared<const ScreenBoundary>(ScreenBoundary{1920, 1080, 0, 0});
    cache.publishSnapshot(snap1);

    auto held = cache.getSnapshot();
    CFX_TEST_CHECK(held->width == 1920);

    auto snap2 = std::make_shared<const ScreenBoundary>(ScreenBoundary{3840, 2160, 0, 0});
    cache.publishSnapshot(snap2);

    CFX_TEST_CHECK(held->width == 1920);

    auto next = cache.getSnapshot();
    CFX_TEST_CHECK(next->width == 3840);
}

// --- Old Snapshot Lifetime by shared_ptr Reference Count ---

static void test_old_snapshot_lifetime() {
    fprintf(stderr, "[TEST] test_old_snapshot_lifetime\n");
    ScreenBoundaryCache cache;

    auto snap1 = std::make_shared<const ScreenBoundary>(ScreenBoundary{1920, 1080, 0, 0});
    cache.publishSnapshot(snap1);

    auto held = cache.getSnapshot();
    CFX_TEST_CHECK(held.use_count() >= 1);

    auto snap2 = std::make_shared<const ScreenBoundary>(ScreenBoundary{2560, 1440, 0, 0});
    cache.publishSnapshot(snap2);

    CFX_TEST_CHECK(held != nullptr);
    CFX_TEST_CHECK(held->width == 1920);
}

// --- reconfigPending Flag ---

static void test_reconfig_pending_basic() {
    fprintf(stderr, "[TEST] test_reconfig_pending_basic\n");
    ScreenBoundaryCache cache;

    CFX_TEST_CHECK(!cache.consumeReconfigPending());

    cache.setReconfigPending();
    CFX_TEST_CHECK(cache.consumeReconfigPending());
    CFX_TEST_CHECK(!cache.consumeReconfigPending());
}

static void test_reconfig_pending_multiple_set() {
    fprintf(stderr, "[TEST] test_reconfig_pending_multiple_set\n");
    ScreenBoundaryCache cache;

    cache.setReconfigPending();
    cache.setReconfigPending();
    cache.setReconfigPending();

    CFX_TEST_CHECK(cache.consumeReconfigPending());
    CFX_TEST_CHECK(!cache.consumeReconfigPending());
}

// --- isValid() Semantics ---

static void test_isValid_after_publish() {
    fprintf(stderr, "[TEST] test_isValid_after_publish\n");
    ScreenBoundaryCache cache;

    CFX_TEST_CHECK(!cache.isValid());

    auto snap = std::make_shared<const ScreenBoundary>(ScreenBoundary{1920, 1080, 0, 0});
    cache.publishSnapshot(snap);
    CFX_TEST_CHECK(cache.isValid());
}

static void test_isValid_with_invalid_boundary() {
    fprintf(stderr, "[TEST] test_isValid_with_invalid_boundary\n");
    ScreenBoundaryCache cache;

    auto invalid_snap = std::make_shared<const ScreenBoundary>(ScreenBoundary{0, 0, 0, 0});
    cache.publishSnapshot(invalid_snap);
    CFX_TEST_CHECK(!cache.isValid());
}

// --- Concurrency: publish from one thread, read from another ---

static void test_concurrent_publish_read() {
    fprintf(stderr, "[TEST] test_concurrent_publish_read\n");
    ScreenBoundaryCache cache;

    auto snap1 = std::make_shared<const ScreenBoundary>(ScreenBoundary{1920, 1080, 0, 0});
    cache.publishSnapshot(snap1);

    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};
    std::atomic<bool> all_valid{true};

    std::thread reader([&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            auto snap = cache.getSnapshot();
            if (snap && !snap->isValid()) {
                all_valid.store(false, std::memory_order_relaxed);
            }
            reads.fetch_add(1, std::memory_order_relaxed);
        }
    });

    for (int i = 0; i < 100; ++i) {
        auto snap = std::make_shared<const ScreenBoundary>(
            ScreenBoundary{static_cast<uint32_t>(1920 + i), 1080, 0, 0});
        cache.publishSnapshot(snap);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    stop.store(true, std::memory_order_relaxed);
    reader.join();

    CFX_TEST_CHECK(all_valid.load());
    CFX_TEST_CHECK(reads.load() > 0);
}

// --- Multiple publish cycles ---

static void test_multiple_publish_cycles() {
    fprintf(stderr, "[TEST] test_multiple_publish_cycles\n");
    ScreenBoundaryCache cache;

    for (int i = 0; i < 1000; ++i) {
        auto snap = std::make_shared<const ScreenBoundary>(
            ScreenBoundary{static_cast<uint32_t>(1920 + i), 1080, 0, 0});
        cache.publishSnapshot(snap);

        auto retrieved = cache.getSnapshot();
        CFX_TEST_CHECK(retrieved != nullptr);
        CFX_TEST_CHECK(retrieved->width == static_cast<uint32_t>(1920 + i));
    }
}

int main() {
    fprintf(stderr, "=== test_screen_boundary_cache ===\n");

    test_initial_state();
    test_publish_and_get();
    test_immutable_snapshot();
    test_single_load();
    test_publish_during_use_old_snapshot();
    test_old_snapshot_lifetime();
    test_reconfig_pending_basic();
    test_reconfig_pending_multiple_set();
    test_isValid_after_publish();
    test_isValid_with_invalid_boundary();
    test_concurrent_publish_read();
    test_multiple_publish_cycles();

    fprintf(stderr, "=== ALL PASS ===\n");
    return 0;
}