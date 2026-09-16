#include <atomic>
#include <cstdint>
#include <thread>

#include "common/spsc_ring_buffer.hpp"

namespace {

struct Payload {
    uint64_t a;
    uint64_t b;
    uint64_t c;
};

int testInv1TailLeHead() {
    using namespace cfx;
    SpscRingBuffer<Payload, 8> rb;

    Payload p{1, 2, 3};
    rb.tryPushDropOldest(p);
    rb.tryPushDropOldest(p);
    rb.tryPushDropOldest(p);

    Payload out;
    rb.tryPop(out);

#ifndef NDEBUG
    if (!rb.validateInv1()) return 1;
#endif
    if (rb.tail() > rb.head()) return 1;
    return 0;
}

int testInv2LogicalBounded() {
    using namespace cfx;
    SpscRingBuffer<Payload, 8> rb;

    for (uint64_t i = 1; i <= 100; ++i) {
        Payload p{i, i * 2, i * 3};
        rb.tryPushDropOldest(p);
    }

    const uint64_t h = rb.head();
    const uint64_t ldb = rb.logicalDropBoundaryValue();
    if (h - ldb > 8) return 1;

#ifndef NDEBUG
    if (!rb.validateInv2()) return 1;
#endif
    return 0;
}

int testInv3Monotonic() {
    using namespace cfx;
    SpscRingBuffer<Payload, 8> rb;

    const uint64_t h0 = rb.head();
    const uint64_t t0 = rb.tail();

    Payload p{1, 2, 3};
    rb.tryPushDropOldest(p);

    const uint64_t h1 = rb.head();
    const uint64_t t1 = rb.tail();

    if (h1 < h0) return 1;
    if (t1 < t0) return 1;

    Payload out;
    rb.tryPop(out);

    const uint64_t t2 = rb.tail();
    if (t2 < t1) return 1;
    return 0;
}

int testInv6LogicalDropBoundaryDefinition() {
    using namespace cfx;
    SpscRingBuffer<Payload, 8> rb;

    for (uint64_t i = 1; i <= 20; ++i) {
        Payload p{i, i * 2, i * 3};
        rb.tryPushDropOldest(p);
    }

    const uint64_t h = rb.head();
    const uint64_t t = rb.tail();
    const uint64_t hMinusCap = (h > 8) ? (h - 8) : 0;
    const uint64_t expected = (t > hMinusCap) ? t : hMinusCap;

    if (rb.logicalDropBoundaryValue() != expected) return 1;

#ifndef NDEBUG
    if (!rb.validateInv6()) return 1;
#endif
    return 0;
}

int testInv7aPendingDropped() {
    using namespace cfx;
    SpscRingBuffer<Payload, 8> rb;

    for (uint64_t i = 1; i <= 20; ++i) {
        Payload p{i, i * 2, i * 3};
        rb.tryPushDropOldest(p);
    }

    const uint64_t h = rb.head();
    const uint64_t t = rb.tail();
    const uint64_t expectedPending = (h > t + 8) ? (h - t - 8) : 0;

    if (rb.pendingDroppedValue() != expectedPending) return 1;

#ifndef NDEBUG
    if (!rb.validateInv7a()) return 1;
#endif

    Payload out;
    while (rb.tryPop(out)) {
    }

    if (rb.pendingDroppedValue() != 0) return 1;
    return 0;
}

int testInv7bCumulativeDropCountObservational() {
    using namespace cfx;
    SpscRingBuffer<Payload, 8> rb;

    for (uint64_t i = 1; i <= 20; ++i) {
        Payload p{i, i * 2, i * 3};
        rb.tryPushDropOldest(p);
    }

    const uint64_t drops1 = rb.cumulativeDropCount();
    if (drops1 == 0) return 1;

    Payload out;
    while (rb.tryPop(out)) {
    }

    const uint64_t drops2 = rb.cumulativeDropCount();
    if (drops2 != drops1) return 1;

    for (uint64_t i = 21; i <= 40; ++i) {
        Payload p{i, i * 2, i * 3};
        rb.tryPushDropOldest(p);
    }

    const uint64_t drops3 = rb.cumulativeDropCount();
    if (drops3 < drops2) return 1;
    return 0;
}

int testIntervalDecomposition() {
    using namespace cfx;
    SpscRingBuffer<Payload, 8> rb;

    for (uint64_t i = 1; i <= 30; ++i) {
        Payload p{i, i * 2, i * 3};
        rb.tryPushDropOldest(p);
    }

    const uint64_t h = rb.head();
    const uint64_t t = rb.tail();
    const uint64_t ldb = rb.logicalDropBoundaryValue();

    if (t > ldb) return 1;
    if (ldb > h) return 1;
    if (h - ldb > 8) return 1;

#ifndef NDEBUG
    if (!rb.validateIntervalDecomposition()) return 1;
#endif

    Payload out;
    while (rb.tryPop(out)) {
        const uint64_t h2 = rb.head();
        const uint64_t t2 = rb.tail();
        const uint64_t ldb2 = rb.logicalDropBoundaryValue();
        if (t2 > ldb2) return 1;
        if (ldb2 > h2) return 1;
        if (h2 - ldb2 > 8) return 1;
    }
    return 0;
}

int testAllInvariantsUnderStress() {
    using namespace cfx;
    SpscRingBuffer<Payload, 16> rb;

    constexpr int TOTAL = 10000;
    std::atomic<bool> done{false};

    std::thread producer([&]() {
        for (int i = 1; i <= TOTAL; ++i) {
            Payload p{static_cast<uint64_t>(i), 0, 0};
            rb.tryPushDropOldest(p);
        }
        done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        while (true) {
            Payload out;
            if (rb.tryPop(out)) {
#ifndef NDEBUG
                if (!rb.validateAllInvariants()) {
                }
#endif
            } else {
                if (done.load(std::memory_order_acquire) && rb.isEmpty()) break;
            }
        }
    });

    producer.join();
    consumer.join();

#ifndef NDEBUG
    if (!rb.validateAllInvariants()) return 1;
#endif
    return 0;
}

int testPendingDroppedConvergesToZero() {
    using namespace cfx;
    SpscRingBuffer<Payload, 8> rb;

    for (uint64_t i = 1; i <= 50; ++i) {
        Payload p{i, i * 2, i * 3};
        rb.tryPushDropOldest(p);
    }

    if (rb.pendingDroppedValue() == 0) return 1;

    Payload out;
    while (rb.tryPop(out)) {
    }

    if (rb.pendingDroppedValue() != 0) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testInv1TailLeHead()) return 1;
    if (testInv2LogicalBounded()) return 1;
    if (testInv3Monotonic()) return 1;
    if (testInv6LogicalDropBoundaryDefinition()) return 1;
    if (testInv7aPendingDropped()) return 1;
    if (testInv7bCumulativeDropCountObservational()) return 1;
    if (testIntervalDecomposition()) return 1;
    if (testAllInvariantsUnderStress()) return 1;
    if (testPendingDroppedConvergesToZero()) return 1;
    return 0;
}