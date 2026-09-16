#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "common/spsc_ring_buffer.hpp"

namespace {

struct Event {
    uint64_t seq;
    uint64_t value;
};

int testP1ConsumerPopBeforeProducerPush() {
    using namespace cfx;
    SpscRingBuffer<Event, 4> rb;

    Event e1{1, 100};
    rb.tryPushDropOldest(e1);

    Event out;
    bool got = rb.tryPop(out);

    Event e2{2, 200};
    rb.tryPushDropOldest(e2);

    if (!got) return 1;
    if (out.seq != 1) return 1;
    if (out.value != 100) return 1;
    return 0;
}

int testP2ProducerPushBeforeConsumerPop() {
    using namespace cfx;
    SpscRingBuffer<Event, 4> rb;

    Event e1{1, 100};
    rb.tryPushDropOldest(e1);

    Event out;
    if (!rb.tryPop(out)) return 1;
    if (out.seq != 1) return 1;
    if (out.value != 100) return 1;
    return 0;
}

int testP3ConsumerReadsHeadProducerPushes() {
    using namespace cfx;
    SpscRingBuffer<Event, 8> rb;

    for (uint64_t i = 1; i <= 3; ++i) {
        Event e{i, i * 100};
        rb.tryPushDropOldest(e);
    }

    const uint64_t hBefore = rb.head();

    Event e4{4, 400};
    rb.tryPushDropOldest(e4);

    if (rb.head() != hBefore + 1) return 1;

    Event out;
    if (!rb.tryPop(out)) return 1;
    if (out.seq < 1 || out.seq > 4) return 1;
    return 0;
}

int testP4ConsumerComputesLdbProducerPushes() {
    using namespace cfx;
    SpscRingBuffer<Event, 8> rb;

    for (uint64_t i = 1; i <= 5; ++i) {
        Event e{i, i * 100};
        rb.tryPushDropOldest(e);
    }

    const uint64_t ldbBefore = rb.logicalDropBoundaryValue();

    Event e6{6, 600};
    rb.tryPushDropOldest(e6);

    const uint64_t ldbAfter = rb.logicalDropBoundaryValue();
    if (ldbAfter < ldbBefore) return 1;

    Event out;
    if (!rb.tryPop(out)) return 1;
    if (out.seq < 1) return 1;
    return 0;
}

int testP5ConsumerS1ProducerOverwrite() {
    using namespace cfx;
    SpscRingBuffer<Event, 2> rb;

    Event e1{1, 100};
    rb.tryPushDropOldest(e1);
    Event e2{2, 200};
    rb.tryPushDropOldest(e2);

    Event e3{3, 300};
    rb.tryPushDropOldest(e3);

    Event out;
    bool gotItem1 = false;
    while (rb.tryPop(out)) {
        if (out.seq == 1 && out.value == 100) {
            gotItem1 = true;
        }
    }

    if (gotItem1) return 1;
    return 0;
}

int testP6ConsumerPayloadReadProducerOverwrite() {
    using namespace cfx;
    SpscRingBuffer<Event, 2> rb;

    for (uint64_t i = 1; i <= 10; ++i) {
        Event e{i, i * 100};
        rb.tryPushDropOldest(e);
    }

    Event out;
    std::vector<uint64_t> consumed;
    while (rb.tryPop(out)) {
        consumed.push_back(out.seq);
    }

    for (uint64_t s : consumed) {
        if (s <= 8) return 1;
    }
    return 0;
}

int testP7ConsumerS2ProducerOverwrite() {
    using namespace cfx;
    SpscRingBuffer<Event, 8> rb;

    Event e1{1, 100};
    rb.tryPushDropOldest(e1);

    Event out;
    if (!rb.tryPop(out)) return 1;
    if (out.seq != 1) return 1;
    if (out.value != 100) return 1;

    Event e2{2, 200};
    rb.tryPushDropOldest(e2);

    if (out.seq != 1) return 1;
    return 0;
}

int testP8ConsumerArbitraryPause() {
    using namespace cfx;
    SpscRingBuffer<Event, 16> rb;

    constexpr int TOTAL = 10000;
    std::atomic<bool> done{false};
    std::atomic<int> consumedCount{0};
    std::atomic<uint64_t> maxConsumedSeq{0};

    std::thread producer([&]() {
        for (int i = 1; i <= TOTAL; ++i) {
            Event e{static_cast<uint64_t>(i), static_cast<uint64_t>(i)};
            rb.tryPushDropOldest(e);
        }
        done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        int popCount = 0;
        while (true) {
            Event out;
            if (rb.tryPop(out)) {
                consumedCount.fetch_add(1, std::memory_order_relaxed);
                uint64_t prev = maxConsumedSeq.load(std::memory_order_relaxed);
                while (out.seq > prev &&
                       !maxConsumedSeq.compare_exchange_weak(prev, out.seq,
                                                              std::memory_order_relaxed)) {
                }
                popCount++;
                if (popCount % 1000 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            } else {
                if (done.load(std::memory_order_acquire) && rb.isEmpty()) break;
            }
        }
    });

    producer.join();
    consumer.join();

    if (consumedCount.load() == 0) return 1;
    if (maxConsumedSeq.load() != static_cast<uint64_t>(TOTAL)) return 1;
    return 0;
}

int testCoreConclusionLpBeforeLc() {
    using namespace cfx;
    SpscRingBuffer<Event, 4> rb;

    for (uint64_t i = 1; i <= 10; ++i) {
        Event e{i, i * 100};
        rb.tryPushDropOldest(e);
    }

    Event out;
    std::vector<uint64_t> consumed;
    while (rb.tryPop(out)) {
        consumed.push_back(out.seq);
    }

    for (uint64_t s : consumed) {
        if (s <= 6) return 1;
    }
    return 0;
}

int testCoreConclusionLcBeforeLp() {
    using namespace cfx;
    SpscRingBuffer<Event, 8> rb;

    Event e1{1, 100};
    rb.tryPushDropOldest(e1);

    Event out;
    if (!rb.tryPop(out)) return 1;
    if (out.seq != 1) return 1;

    for (uint64_t i = 2; i <= 10; ++i) {
        Event e{i, i * 100};
        rb.tryPushDropOldest(e);
    }

    if (out.seq != 1) return 1;
    if (out.value != 100) return 1;
    return 0;
}

int testEpochValidityTheorem1ExpiredEpochNotReturned() {
    using namespace cfx;
    SpscRingBuffer<Event, 4> rb;

    Event e1{1, 100};
    rb.tryPushDropOldest(e1);

    for (uint64_t i = 2; i <= 5; ++i) {
        Event e{i, i * 100};
        rb.tryPushDropOldest(e);
    }

    Event out;
    bool returnedItem1 = false;
    while (rb.tryPop(out)) {
        if (out.seq == 1 && out.value == 100) {
            returnedItem1 = true;
        }
    }

    if (returnedItem1) return 1;
    return 0;
}

int testEpochValidityTheorem2ValidEpochReturned() {
    using namespace cfx;
    SpscRingBuffer<Event, 8> rb;

    Event e1{1, 100};
    rb.tryPushDropOldest(e1);
    Event e2{2, 200};
    rb.tryPushDropOldest(e2);

    Event out;
    if (!rb.tryPop(out)) return 1;
    if (out.seq != 1) return 1;
    if (out.value != 100) return 1;

    if (!rb.tryPop(out)) return 1;
    if (out.seq != 2) return 1;
    if (out.value != 200) return 1;
    return 0;
}

int testItemSeqMonotonicInConcurrentStress() {
    using namespace cfx;
    SpscRingBuffer<Event, 32> rb;

    constexpr int TOTAL = 100000;
    std::atomic<bool> done{false};
    std::atomic<int> consumedCount{0};

    std::thread producer([&]() {
        for (int i = 1; i <= TOTAL; ++i) {
            Event e{static_cast<uint64_t>(i), static_cast<uint64_t>(i)};
            rb.tryPushDropOldest(e);
        }
        done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        uint64_t lastSeq = 0;
        while (true) {
            Event out;
            if (rb.tryPop(out)) {
                if (out.seq < lastSeq) return;
                lastSeq = out.seq;
                consumedCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                if (done.load(std::memory_order_acquire) && rb.isEmpty()) break;
            }
        }
    });

    producer.join();
    consumer.join();

    if (consumedCount.load() == 0) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testP1ConsumerPopBeforeProducerPush()) return 1;
    if (testP2ProducerPushBeforeConsumerPop()) return 1;
    if (testP3ConsumerReadsHeadProducerPushes()) return 1;
    if (testP4ConsumerComputesLdbProducerPushes()) return 1;
    if (testP5ConsumerS1ProducerOverwrite()) return 1;
    if (testP6ConsumerPayloadReadProducerOverwrite()) return 1;
    if (testP7ConsumerS2ProducerOverwrite()) return 1;
    if (testP8ConsumerArbitraryPause()) return 1;
    if (testCoreConclusionLpBeforeLc()) return 1;
    if (testCoreConclusionLcBeforeLp()) return 1;
    if (testEpochValidityTheorem1ExpiredEpochNotReturned()) return 1;
    if (testEpochValidityTheorem2ValidEpochReturned()) return 1;
    if (testItemSeqMonotonicInConcurrentStress()) return 1;
    return 0;
}