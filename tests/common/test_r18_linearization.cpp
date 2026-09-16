#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "common/spsc_ring_buffer.hpp"
#include "common/spsc_slot.hpp"

namespace {

struct Event {
    uint64_t seq;
    uint64_t value;
};

struct LPRecord {
    uint64_t order;
    const char* name;
    uint64_t seqValue;
};

class LPRecorder {
public:
    void record(const char* name, uint64_t seqValue) {
        uint64_t ord = counter_.fetch_add(1, std::memory_order_relaxed);
        records_.push_back({ord, name, seqValue});
    }

    uint64_t orderOf(const char* name) const {
        for (const auto& r : records_) {
            if (std::strcmp(r.name, name) == 0) return r.order;
        }
        return UINT64_MAX;
    }

    bool isBefore(const char* a, const char* b) const {
        return orderOf(a) < orderOf(b);
    }

    void clear() {
        counter_.store(0, std::memory_order_relaxed);
        records_.clear();
    }

private:
    std::atomic<uint64_t> counter_{0};
    std::vector<LPRecord> records_;
};

int testP1ConsumerPopBeforeProducerPush() {
    using namespace cfx;
    SpscRingBuffer<Event, 8> rb;
    LPRecorder rec;

    Event e1{1, 100};
    rb.tryPushDropOldest(e1);

    std::atomic<bool> consumerDone{false};
    Event consumerOut{};
    bool consumerGot = false;

    std::thread consumer([&]() {
        consumerGot = rb.tryPop(consumerOut);
        rec.record("Lc", consumerOut.seq);
        consumerDone.store(true, std::memory_order_release);
    });

    std::thread producer([&]() {
        while (!consumerDone.load(std::memory_order_acquire))
            std::this_thread::yield();
        Event e2{2, 200};
        rb.tryPushDropOldest(e2);
        rec.record("Lp", 2);
    });

    consumer.join();
    producer.join();

    if (!consumerGot) return 1;
    if (consumerOut.seq != 1) return 1;
    if (consumerOut.value != 100) return 1;
    if (!rec.isBefore("Lc", "Lp")) return 1;
    return 0;
}

int testP2ProducerPushBeforeConsumerPop() {
    using namespace cfx;
    SpscRingBuffer<Event, 8> rb;
    LPRecorder rec;

    Event e1{1, 100};
    rb.tryPushDropOldest(e1);

    std::atomic<bool> producerDone{false};
    Event consumerOut{};
    bool consumerGot = false;

    std::thread producer([&]() {
        Event e2{2, 200};
        rb.tryPushDropOldest(e2);
        rec.record("Lp", 2);
        producerDone.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        while (!producerDone.load(std::memory_order_acquire))
            std::this_thread::yield();
        consumerGot = rb.tryPop(consumerOut);
        rec.record("Lc", consumerOut.seq);
    });

    producer.join();
    consumer.join();

    if (!consumerGot) return 1;
    if (consumerOut.seq != 1) return 1;
    if (!rec.isBefore("Lp", "Lc")) return 1;
    return 0;
}

int testP3ConsumerReadsHeadProducerPushes() {
    using namespace cfx;
    SpscRingBuffer<Event, 8> rb;
    LPRecorder rec;

    for (uint64_t i = 1; i <= 3; ++i) {
        Event e{i, i * 100};
        rb.tryPushDropOldest(e);
    }

    std::atomic<bool> headRead{false};
    std::atomic<bool> producerPushed{false};
    Event consumerOut{};
    bool consumerGot = false;

    std::thread consumer([&]() {
        const uint64_t h = rb.head();
        rec.record("Lc_head", h);
        headRead.store(true, std::memory_order_release);

        while (!producerPushed.load(std::memory_order_acquire))
            std::this_thread::yield();

        consumerGot = rb.tryPop(consumerOut);
        rec.record("Lc", consumerOut.seq);
    });

    std::thread producer([&]() {
        while (!headRead.load(std::memory_order_acquire))
            std::this_thread::yield();
        Event e4{4, 400};
        rb.tryPushDropOldest(e4);
        rec.record("Lp", 4);
        producerPushed.store(true, std::memory_order_release);
    });

    consumer.join();
    producer.join();

    if (!consumerGot) return 1;
    if (consumerOut.seq > 4) return 1;
    if (!rec.isBefore("Lc_head", "Lp")) return 1;
    return 0;
}

int testP4ConsumerComputesLdbProducerPushes() {
    using namespace cfx;
    SpscRingBuffer<Event, 8> rb;
    LPRecorder rec;

    for (uint64_t i = 1; i <= 5; ++i) {
        Event e{i, i * 100};
        rb.tryPushDropOldest(e);
    }

    std::atomic<bool> ldbComputed{false};
    std::atomic<bool> producerPushed{false};
    Event consumerOut{};
    bool consumerGot = false;
    uint64_t ldbBefore = 0;

    std::thread consumer([&]() {
        ldbBefore = rb.logicalDropBoundaryValue();
        rec.record("Lc_ldb", ldbBefore);
        ldbComputed.store(true, std::memory_order_release);

        while (!producerPushed.load(std::memory_order_acquire))
            std::this_thread::yield();

        consumerGot = rb.tryPop(consumerOut);
        rec.record("Lc", consumerOut.seq);
    });

    std::thread producer([&]() {
        while (!ldbComputed.load(std::memory_order_acquire))
            std::this_thread::yield();
        Event e6{6, 600};
        rb.tryPushDropOldest(e6);
        rec.record("Lp", 6);
        producerPushed.store(true, std::memory_order_release);
    });

    consumer.join();
    producer.join();

    if (!consumerGot) return 1;
    if (!rec.isBefore("Lc_ldb", "Lp")) return 1;
    return 0;
}

int testP5ConsumerS1ProducerOverwrite() {
    using namespace cfx;

    Slot<Event> slot;
    slot.initialize(0);

    Event item1{1, 100};
    slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
    slot.payload.store(item1, std::memory_order_release);
    slot.seq.store(1, std::memory_order_release);
    slot.writeVersion.fetch_add(1, std::memory_order_release);

    std::atomic<bool> consumerS1Read{false};
    std::atomic<bool> producerOverwriteDone{false};

    LPRecorder rec;
    uint64_t consumerS1 = 0;
    uint64_t consumerS2 = 0;
    bool consumerAccepted = false;

    std::thread consumer([&]() {
        uint64_t v1 = slot.writeVersion.load(std::memory_order_acquire);
        consumerS1 = slot.seq.load(std::memory_order_acquire);
        consumerS1Read.store(true, std::memory_order_release);

        while (!producerOverwriteDone.load(std::memory_order_acquire))
            std::this_thread::yield();

        Event item = slot.payload.load(std::memory_order_acquire);
        consumerS2 = slot.seq.load(std::memory_order_acquire);
        uint64_t v2 = slot.writeVersion.load(std::memory_order_acquire);
        rec.record("Lc", consumerS2);

        consumerAccepted = (consumerS1 == consumerS2 && v1 == v2);
    });

    std::thread producer([&]() {
        while (!consumerS1Read.load(std::memory_order_acquire))
            std::this_thread::yield();

        Event item2{2, 200};
        slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
        slot.payload.store(item2, std::memory_order_release);
        slot.seq.store(2, std::memory_order_release);
        slot.writeVersion.fetch_add(1, std::memory_order_release);
        rec.record("Lp", 2);
        rec.record("Ld", 2);

        producerOverwriteDone.store(true, std::memory_order_release);
    });

    consumer.join();
    producer.join();

    if (consumerS1 != 1) return 1;
    if (consumerS2 != 2) return 1;
    if (consumerAccepted) return 1;
    if (!rec.isBefore("Lp", "Lc")) return 1;
    return 0;
}

int testP6ConsumerPayloadReadProducerOverwrite() {
    using namespace cfx;

    Slot<Event> slot;
    slot.initialize(0);

    Event item1{1, 100};
    slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
    slot.payload.store(item1, std::memory_order_release);
    slot.seq.store(1, std::memory_order_release);
    slot.writeVersion.fetch_add(1, std::memory_order_release);

    std::atomic<bool> consumerS1Done{false};
    std::atomic<bool> producerOverwriteDone{false};

    LPRecorder rec;
    uint64_t consumerS1 = 0;
    uint64_t consumerS2 = 0;
    bool consumerAccepted = false;
    Event consumerItem{};

    std::thread consumer([&]() {
        uint64_t v1 = slot.writeVersion.load(std::memory_order_acquire);
        consumerS1 = slot.seq.load(std::memory_order_acquire);
        consumerS1Done.store(true, std::memory_order_release);

        while (!producerOverwriteDone.load(std::memory_order_acquire))
            std::this_thread::yield();

        consumerItem = slot.payload.load(std::memory_order_acquire);
        consumerS2 = slot.seq.load(std::memory_order_acquire);
        uint64_t v2 = slot.writeVersion.load(std::memory_order_acquire);
        rec.record("Lc", consumerS2);

        consumerAccepted = (consumerS1 == consumerS2 && v1 == v2);
    });

    std::thread producer([&]() {
        while (!consumerS1Done.load(std::memory_order_acquire))
            std::this_thread::yield();

        Event item2{2, 200};
        slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
        slot.payload.store(item2, std::memory_order_release);
        slot.seq.store(2, std::memory_order_release);
        slot.writeVersion.fetch_add(1, std::memory_order_release);
        rec.record("Lp", 2);
        rec.record("Ld", 2);

        producerOverwriteDone.store(true, std::memory_order_release);
    });

    consumer.join();
    producer.join();

    if (consumerS1 != 1) return 1;
    if (consumerS2 != 2) return 1;
    if (consumerAccepted) return 1;
    if (!rec.isBefore("Lp", "Lc")) return 1;
    return 0;
}

int testP7ConsumerS2BeforeProducerOverwrite() {
    using namespace cfx;

    Slot<Event> slot;
    slot.initialize(0);

    Event item1{1, 100};
    slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
    slot.payload.store(item1, std::memory_order_release);
    slot.seq.store(1, std::memory_order_release);
    slot.writeVersion.fetch_add(1, std::memory_order_release);

    std::atomic<bool> consumerS2Done{false};
    std::atomic<bool> producerOverwriteDone{false};

    LPRecorder rec;
    uint64_t consumerS1 = 0;
    uint64_t consumerS2 = 0;
    bool consumerAccepted = false;
    Event consumerItem{};

    std::thread consumer([&]() {
        uint64_t v1 = slot.writeVersion.load(std::memory_order_acquire);
        consumerS1 = slot.seq.load(std::memory_order_acquire);
        consumerItem = slot.payload.load(std::memory_order_acquire);
        consumerS2 = slot.seq.load(std::memory_order_acquire);
        uint64_t v2 = slot.writeVersion.load(std::memory_order_acquire);
        rec.record("Lc", consumerS2);

        consumerAccepted = (consumerS1 == consumerS2 && v1 == v2);
        consumerS2Done.store(true, std::memory_order_release);

        while (!producerOverwriteDone.load(std::memory_order_acquire))
            std::this_thread::yield();
    });

    std::thread producer([&]() {
        while (!consumerS2Done.load(std::memory_order_acquire))
            std::this_thread::yield();

        Event item2{2, 200};
        slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
        slot.payload.store(item2, std::memory_order_release);
        slot.seq.store(2, std::memory_order_release);
        slot.writeVersion.fetch_add(1, std::memory_order_release);
        rec.record("Lp", 2);
        rec.record("Ld", 2);

        producerOverwriteDone.store(true, std::memory_order_release);
    });

    consumer.join();
    producer.join();

    if (consumerS1 != 1) return 1;
    if (consumerS2 != 1) return 1;
    if (!consumerAccepted) return 1;
    if (consumerItem.seq != 1) return 1;
    if (consumerItem.value != 100) return 1;
    if (!rec.isBefore("Lc", "Lp")) return 1;
    return 0;
}

int testP8ConsumerArbitraryPause() {
    using namespace cfx;
    SpscRingBuffer<Event, 32> rb;

    constexpr int TOTAL = 50000;
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
                       !maxConsumedSeq.compare_exchange_weak(
                           prev, out.seq, std::memory_order_relaxed)) {
                }
                popCount++;
                if (popCount % 500 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
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

int testCoreConclusionLpBeforeLcOldItemNotReturned() {
    using namespace cfx;

    Slot<Event> slot;
    slot.initialize(0);

    for (int trial = 0; trial < 100; ++trial) {
        Event item1{1, 100};
        slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
        slot.payload.store(item1, std::memory_order_release);
        slot.seq.store(1, std::memory_order_release);
        slot.writeVersion.fetch_add(1, std::memory_order_release);

        std::atomic<bool> s1Read{false};
        std::atomic<bool> overwriteDone{false};
        uint64_t s1 = 0, s2 = 0;

        std::thread consumer([&]() {
            s1 = slot.seq.load(std::memory_order_acquire);
            s1Read.store(true, std::memory_order_release);
            while (!overwriteDone.load(std::memory_order_acquire))
                std::this_thread::yield();
            Event item = slot.payload.load(std::memory_order_acquire);
            s2 = slot.seq.load(std::memory_order_acquire);
        });

        std::thread producer([&]() {
            while (!s1Read.load(std::memory_order_acquire))
                std::this_thread::yield();
            Event item2{2, 200};
            slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
            slot.payload.store(item2, std::memory_order_release);
            slot.seq.store(2, std::memory_order_release);
            slot.writeVersion.fetch_add(1, std::memory_order_release);
            overwriteDone.store(true, std::memory_order_release);
        });

        consumer.join();
        producer.join();

        if (s1 == s2) return 1;
        if (s1 != 1) return 1;
        if (s2 != 2) return 1;
    }
    return 0;
}

int testCoreConclusionLcBeforeLpOldItemLegitimatelyReturned() {
    using namespace cfx;

    Slot<Event> slot;
    slot.initialize(0);

    for (int trial = 0; trial < 100; ++trial) {
        Event item1{1, 100};
        slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
        slot.payload.store(item1, std::memory_order_release);
        slot.seq.store(1, std::memory_order_release);
        slot.writeVersion.fetch_add(1, std::memory_order_release);

        std::atomic<bool> consumerDone{false};
        uint64_t s1 = 0, s2 = 0;
        Event item{};

        std::thread consumer([&]() {
            s1 = slot.seq.load(std::memory_order_acquire);
            item = slot.payload.load(std::memory_order_acquire);
            s2 = slot.seq.load(std::memory_order_acquire);
            consumerDone.store(true, std::memory_order_release);
        });

        std::thread producer([&]() {
            while (!consumerDone.load(std::memory_order_acquire))
                std::this_thread::yield();
            Event item2{2, 200};
            slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
            slot.payload.store(item2, std::memory_order_release);
            slot.seq.store(2, std::memory_order_release);
            slot.writeVersion.fetch_add(1, std::memory_order_release);
        });

        consumer.join();
        producer.join();

        if (s1 != s2) return 1;
        if (s1 != 1) return 1;
        if (item.seq != 1) return 1;
        if (item.value != 100) return 1;
    }
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

    Slot<Event> slot;
    slot.initialize(0);

    Event item1{1, 100};
    slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
    slot.payload.store(item1, std::memory_order_release);
    slot.seq.store(1, std::memory_order_release);
    slot.writeVersion.fetch_add(1, std::memory_order_release);

    std::atomic<bool> consumerDone{false};
    uint64_t s1 = 0, s2 = 0;
    Event item{};

    std::thread consumer([&]() {
        s1 = slot.seq.load(std::memory_order_acquire);
        item = slot.payload.load(std::memory_order_acquire);
        s2 = slot.seq.load(std::memory_order_acquire);
        consumerDone.store(true, std::memory_order_release);
    });

    std::thread producer([&]() {
        while (!consumerDone.load(std::memory_order_acquire))
            std::this_thread::yield();
        Event item2{2, 200};
        slot.writeVersion.fetch_add(1, std::memory_order_acq_rel);
        slot.payload.store(item2, std::memory_order_release);
        slot.seq.store(2, std::memory_order_release);
        slot.writeVersion.fetch_add(1, std::memory_order_release);
    });

    consumer.join();
    producer.join();

    if (s1 != 1) return 1;
    if (s2 != 1) return 1;
    if (s1 != s2) return 1;
    if (item.seq != 1) return 1;
    if (item.value != 100) return 1;
    return 0;
}

int testItemSeqMonotonicInConcurrentStress() {
    using namespace cfx;
    SpscRingBuffer<Event, 32> rb;

    constexpr int TOTAL = 100000;
    std::atomic<bool> done{false};
    std::atomic<int> consumedCount{0};
    std::atomic<bool> monotonicViolation{false};

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
                if (out.seq < lastSeq) {
                    monotonicViolation.store(true, std::memory_order_relaxed);
                    break;
                }
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
    if (monotonicViolation.load()) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testP1ConsumerPopBeforeProducerPush()) { fprintf(stderr, "FAIL: P1\n"); return 1; }
    if (testP2ProducerPushBeforeConsumerPop()) { fprintf(stderr, "FAIL: P2\n"); return 1; }
    if (testP3ConsumerReadsHeadProducerPushes()) { fprintf(stderr, "FAIL: P3\n"); return 1; }
    if (testP4ConsumerComputesLdbProducerPushes()) { fprintf(stderr, "FAIL: P4\n"); return 1; }
    if (testP5ConsumerS1ProducerOverwrite()) { fprintf(stderr, "FAIL: P5\n"); return 1; }
    if (testP6ConsumerPayloadReadProducerOverwrite()) { fprintf(stderr, "FAIL: P6\n"); return 1; }
    if (testP7ConsumerS2BeforeProducerOverwrite()) { fprintf(stderr, "FAIL: P7\n"); return 1; }
    if (testP8ConsumerArbitraryPause()) { fprintf(stderr, "FAIL: P8\n"); return 1; }
    if (testCoreConclusionLpBeforeLcOldItemNotReturned()) { fprintf(stderr, "FAIL: CoreLpBeforeLc\n"); return 1; }
    if (testCoreConclusionLcBeforeLpOldItemLegitimatelyReturned()) { fprintf(stderr, "FAIL: CoreLcBeforeLp\n"); return 1; }
    if (testEpochValidityTheorem1ExpiredEpochNotReturned()) { fprintf(stderr, "FAIL: Theorem1\n"); return 1; }
    if (testEpochValidityTheorem2ValidEpochReturned()) { fprintf(stderr, "FAIL: Theorem2\n"); return 1; }
    if (testItemSeqMonotonicInConcurrentStress()) { fprintf(stderr, "FAIL: Monotonic\n"); return 1; }
    return 0;
}
