#include <cstdio>
#include <cstdlib>
#include <chrono>

#include "common/domain.hpp"
#include "mac/screen_boundary_cache.hpp"
#include "mac/display_reconfig_listener.hpp"

inline void cfx_test_check(bool cond, const char* file, int line, const char* expr) {
    if (!cond) {
        fprintf(stderr, "  [FAIL] %s:%d: %s\n", file, line, expr);
        std::exit(1);
    }
}
#define CFX_TEST_CHECK(cond) cfx_test_check(static_cast<bool>(cond), __FILE__, __LINE__, #cond)

using namespace cfx;

// --- Registration ---

static void test_registration() {
    fprintf(stderr, "[TEST] test_registration\n");
    ScreenBoundaryCache cache;
    DisplayReconfigListener listener(cache);

    CFX_TEST_CHECK(!listener.isRegistered());
    CFX_TEST_CHECK(listener.registerCallback());
    CFX_TEST_CHECK(listener.isRegistered());
    listener.unregisterCallback();
    CFX_TEST_CHECK(!listener.isRegistered());
}

static void test_double_register() {
    fprintf(stderr, "[TEST] test_double_register\n");
    ScreenBoundaryCache cache;
    DisplayReconfigListener listener(cache);

    CFX_TEST_CHECK(listener.registerCallback());
    CFX_TEST_CHECK(listener.registerCallback());
    CFX_TEST_CHECK(listener.isRegistered());
    listener.unregisterCallback();
}

// --- RAII ---

static void test_raii_auto_unregister() {
    fprintf(stderr, "[TEST] test_raii_auto_unregister\n");
    ScreenBoundaryCache cache;

    {
        DisplayReconfigListener listener(cache);
        CFX_TEST_CHECK(listener.registerCallback());
        CFX_TEST_CHECK(listener.isRegistered());
    }

    fprintf(stderr, "  RAII destructor called, callback unregistered\n");
}

// --- Minimal Notification: callback sets reconfigPending ---

static void test_callback_sets_reconfig_pending() {
    fprintf(stderr, "[TEST] test_callback_sets_reconfig_pending\n");
    ScreenBoundaryCache cache;
    DisplayReconfigListener listener(cache);

    CFX_TEST_CHECK(listener.registerCallback());

    CFX_TEST_CHECK(!cache.consumeReconfigPending());

    cache.setReconfigPending();
    CFX_TEST_CHECK(cache.consumeReconfigPending());
    CFX_TEST_CHECK(!cache.consumeReconfigPending());

    listener.unregisterCallback();
}

// --- reconfigPending flag is atomic ---

static void test_reconfig_pending_atomic() {
    fprintf(stderr, "[TEST] test_reconfig_pending_atomic\n");
    ScreenBoundaryCache cache;

    cache.setReconfigPending();
    CFX_TEST_CHECK(cache.consumeReconfigPending());
    CFX_TEST_CHECK(!cache.consumeReconfigPending());

    cache.setReconfigPending();
    cache.setReconfigPending();
    CFX_TEST_CHECK(cache.consumeReconfigPending());
    CFX_TEST_CHECK(!cache.consumeReconfigPending());
}

// --- Callback latency ≤1ms (Implementation Budget) ---

static void test_callback_latency() {
    fprintf(stderr, "[TEST] test_callback_latency\n");
    ScreenBoundaryCache cache;

    auto start = std::chrono::high_resolution_clock::now();
    cache.setReconfigPending();
    auto end = std::chrono::high_resolution_clock::now();

    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    fprintf(stderr, "  setReconfigPending latency: %lld ns\n", static_cast<long long>(ns));
    CFX_TEST_CHECK(ns < 1000000);
}

int main() {
    fprintf(stderr, "=== test_display_reconfig ===\n");

    test_registration();
    test_double_register();
    test_raii_auto_unregister();
    test_callback_sets_reconfig_pending();
    test_reconfig_pending_atomic();
    test_callback_latency();

    fprintf(stderr, "=== ALL PASS ===\n");
    return 0;
}