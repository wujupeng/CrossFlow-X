#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "common/spsc_ring_buffer.hpp"

namespace {

struct TestEvent {
    uint64_t seq;
    uint64_t timestamp;
    uint32_t type;
    uint32_t flags;
};

int testTryPushNormalAndFull() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 4> rb;

    TestEvent ev{1, 100, 0, 0};
    if (!rb.tryPush(ev)) return 1;
    ev = {2, 200, 0, 0};
    if (!rb.tryPush(ev)) return 1;
    ev = {3, 300, 0, 0};
    if (!rb.tryPush(ev)) return 1;
    ev = {4, 400, 0, 0};
    if (!rb.tryPush(ev)) return 1;

    ev = {5, 500, 0, 0};
    if (rb.tryPush(ev)) return 1;

    if (rb.head() != 4) return 1;
    if (rb.tail() != 0) return 1;
    return 0;
}

int testTryPopNormal() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 4> rb;

    TestEvent ev{1, 100, 0, 0};
    rb.tryPush(ev);
    ev = {2, 200, 0, 0};
    rb.tryPush(ev);

    TestEvent out;
    if (!rb.tryPop(out)) return 1;
    if (out.seq != 1) return 1;
    if (out.timestamp != 100) return 1;

    if (!rb.tryPop(out)) return 1;
    if (out.seq != 2) return 1;
    if (out.timestamp != 200) return 1;

    if (rb.tryPop(out)) return 1;
    if (rb.head() != 2) return 1;
    if (rb.tail() != 2) return 1;
    return 0;
}

int testTryPopEmpty() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 4> rb;

    TestEvent out;
    if (rb.tryPop(out)) return 1;
    if (!rb.isEmpty()) return 1;
    return 0;
}

int testTryPushDropOldestOverwrite() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 4> rb;

    for (uint64_t i = 1; i <= 4; ++i) {
        TestEvent ev{i, i * 100, 0, 0};
        rb.tryPushDropOldest(ev);
    }

    if (rb.head() != 4) return 1;
    if (rb.tail() != 0) return 1;
    if (rb.cumulativeDropCount() != 0) return 1;

    TestEvent ev{5, 500, 0, 0};
    rb.tryPushDropOldest(ev);
    if (rb.head() != 5) return 1;
    if (rb.cumulativeDropCount() != 1) return 1;

    TestEvent out;
    bool foundNew = false;
    for (int j = 0; j < 4; ++j) {
        if (rb.tryPop(out)) {
            if (out.seq == 5) {
                foundNew = true;
            }
        }
    }
    if (!foundNew) return 1;
    return 0;
}

int testDropOldestSemantics() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 4> rb;

    for (uint64_t i = 1; i <= 10; ++i) {
        TestEvent ev{i, i * 100, 0, 0};
        rb.tryPushDropOldest(ev);
    }

    if (rb.head() != 10) return 1;
    if (rb.cumulativeDropCount() < 6) return 1;

    TestEvent out;
    std::vector<uint64_t> consumed;
    while (rb.tryPop(out)) {
        consumed.push_back(out.seq);
    }

    if (consumed.empty()) return 1;
    for (uint64_t v : consumed) {
        if (v < 7) return 1;
    }
    return 0;
}

int testLogicalDropBoundaryAlignment() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 4> rb;

    for (uint64_t i = 1; i <= 10; ++i) {
        TestEvent ev{i, i * 100, 0, 0};
        rb.tryPushDropOldest(ev);
    }

    if (rb.head() != 10) return 1;
    if (rb.tail() != 0) return 1;

    const uint64_t ldb = rb.logicalDropBoundaryValue();
    if (ldb != 6) return 1;

    if (rb.pendingDroppedValue() != 6) return 1;

    TestEvent out;
    if (!rb.tryPop(out)) return 1;

    if (rb.tail() < 6) return 1;
    if (rb.pendingDroppedValue() == 0) {
    }
    return 0;
}

int testInv2HoldsAfterDropOldest() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 4> rb;

    for (uint64_t i = 1; i <= 100; ++i) {
        TestEvent ev{i, i * 100, 0, 0};
        rb.tryPushDropOldest(ev);
    }

    const uint64_t h = rb.head();
    const uint64_t ldb = rb.logicalDropBoundaryValue();
    if (h - ldb > 4) return 1;

    TestEvent out;
    while (rb.tryPop(out)) {
        const uint64_t h2 = rb.head();
        const uint64_t ldb2 = rb.logicalDropBoundaryValue();
        if (h2 - ldb2 > 4) return 1;
    }
    return 0;
}

int testConcurrentProducerConsumer() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 64> rb;

    constexpr int TOTAL = 50000;
    std::atomic<bool> producerDone{false};
    std::atomic<int> consumedCount{0};
    std::atomic<int> lastConsumedSeq{0};

    std::thread producer([&]() {
        for (int i = 1; i <= TOTAL; ++i) {
            TestEvent ev{static_cast<uint64_t>(i), static_cast<uint64_t>(i), 0, 0};
            rb.tryPushDropOldest(ev);
        }
        producerDone.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        while (true) {
            TestEvent out;
            if (rb.tryPop(out)) {
                consumedCount.fetch_add(1, std::memory_order_relaxed);
                lastConsumedSeq.store(static_cast<int>(out.seq), std::memory_order_relaxed);
            } else {
                if (producerDone.load(std::memory_order_acquire) && rb.isEmpty()) {
                    break;
                }
            }
        }
    });

    producer.join();
    consumer.join();

    if (consumedCount.load() == 0) return 1;
    if (lastConsumedSeq.load() != TOTAL) return 1;
    return 0;
}

int testCumulativeDropCountMonotonic() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 4> rb;

    for (uint64_t i = 1; i <= 6; ++i) {
        TestEvent ev{i, i * 100, 0, 0};
        rb.tryPushDropOldest(ev);
    }
    const uint64_t drops1 = rb.cumulativeDropCount();

    TestEvent out;
    while (rb.tryPop(out)) {
    }

    for (uint64_t i = 7; i <= 12; ++i) {
        TestEvent ev{i, i * 100, 0, 0};
        rb.tryPushDropOldest(ev);
    }
    const uint64_t drops2 = rb.cumulativeDropCount();

    if (drops2 < drops1) return 1;
    return 0;
}

int testTryPushAfterConsumption() {
    using namespace cfx;
    SpscRingBuffer<TestEvent, 4> rb;

    for (uint64_t i = 1; i <= 4; ++i) {
        TestEvent ev{i, i * 100, 0, 0};
        if (!rb.tryPush(ev)) return 1;
    }

    TestEvent ev{5, 500, 0, 0};
    if (rb.tryPush(ev)) return 1;

    TestEvent out;
    if (!rb.tryPop(out)) return 1;
    if (out.seq != 1) return 1;

    ev = {5, 500, 0, 0};
    if (!rb.tryPush(ev)) return 1;

    if (rb.head() != 5) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testTryPushNormalAndFull()) return 1;
    if (testTryPopNormal()) return 1;
    if (testTryPopEmpty()) return 1;
    if (testTryPushDropOldestOverwrite()) return 1;
    if (testDropOldestSemantics()) return 1;
    if (testLogicalDropBoundaryAlignment()) return 1;
    if (testInv2HoldsAfterDropOldest()) return 1;
    if (testConcurrentProducerConsumer()) return 1;
    if (testCumulativeDropCountMonotonic()) return 1;
    if (testTryPushAfterConsumption()) return 1;
    return 0;
}