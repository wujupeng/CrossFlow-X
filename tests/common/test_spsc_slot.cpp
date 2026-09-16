#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>

#include "common/spsc_slot.hpp"

namespace {

int testNormalPath() {
    using namespace cfx;
    struct Payload {
        uint64_t a;
        uint64_t b;
    };

    Slot<Payload> slot;
    slot.initialize(1);

    Payload src{100, 200};
    SeqlockValidator<Payload>::write(slot, src, 2);

    auto result = SeqlockValidator<Payload>::tryRead(slot, 2);
    if (!result) return 1;
    if (result->a != 100) return 1;
    if (result->b != 200) return 1;
    return 0;
}

int testOverwriteDetection() {
    using namespace cfx;
    struct Payload {
        uint64_t a;
        uint64_t b;
    };

    Slot<Payload> slot;
    slot.initialize(1);

    Payload first{100, 200};
    SeqlockValidator<Payload>::write(slot, first, 2);

    Payload second{300, 400};
    SeqlockValidator<Payload>::write(slot, second, 3);

    auto result = SeqlockValidator<Payload>::tryRead(slot, 2);
    if (result) return 1;

    auto result2 = SeqlockValidator<Payload>::tryRead(slot, 3);
    if (!result2) return 1;
    if (result2->a != 300) return 1;
    if (result2->b != 400) return 1;
    return 0;
}

int testConcurrentOverwrite() {
    using namespace cfx;
    struct Payload {
        uint64_t a;
        uint64_t b;
        uint64_t c;
    };

    Slot<Payload> slot;
    slot.initialize(0);

    std::atomic<uint64_t> producerSeq{0};
    std::atomic<int> validReads{0};

    std::thread producer([&]() {
        for (uint64_t i = 1; i <= 100000; ++i) {
            Payload p{i, i * 2, i * 3};
            SeqlockValidator<Payload>::write(slot, p, i);
            producerSeq.store(i, std::memory_order_release);
        }
    });

    std::thread consumer([&]() {
        uint64_t expectedSeq = 1;
        while (expectedSeq <= 100000) {
            const uint64_t ps = producerSeq.load(std::memory_order_acquire);
            if (ps < expectedSeq) {
                continue;
            }
            auto result = SeqlockValidator<Payload>::tryRead(slot, expectedSeq);
            if (result) {
                if (result->a == expectedSeq && result->b == expectedSeq * 2 && result->c == expectedSeq * 3) {
                    validReads.fetch_add(1, std::memory_order_relaxed);
                }
            }
            ++expectedSeq;
        }
    });

    producer.join();
    consumer.join();

    if (validReads.load() == 0) return 1;
    return 0;
}

int testEpochMismatch() {
    using namespace cfx;
    struct Payload {
        uint64_t value;
    };

    Slot<Payload> slot;
    slot.initialize(5);

    Payload src{42};
    SeqlockValidator<Payload>::write(slot, src, 6);

    auto result = SeqlockValidator<Payload>::tryRead(slot, 99);
    if (result) return 1;

    auto result2 = SeqlockValidator<Payload>::tryRead(slot, 6);
    if (!result2) return 1;
    if (result2->value != 42) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testNormalPath()) return 1;
    if (testOverwriteDetection()) return 1;
    if (testConcurrentOverwrite()) return 1;
    if (testEpochMismatch()) return 1;
    return 0;
}