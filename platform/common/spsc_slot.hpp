#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "common/atomic_payload.hpp"

namespace cfx {

template <typename T>
struct Slot {
    AtomicPayload<T> payload;
    std::atomic<uint64_t> seq{0};

    void initialize(uint64_t initialSeq) noexcept {
        seq.store(initialSeq, std::memory_order_relaxed);
    }
};

template <typename T>
class SeqlockValidator {
public:
    static std::optional<T> tryRead(const Slot<T>& slot, uint64_t expectedSeq) noexcept {
        const uint64_t s1 = slot.seq.load(std::memory_order_acquire);
        if (s1 != expectedSeq) {
            return std::nullopt;
        }
        T item = slot.payload.load(std::memory_order_relaxed);
        const uint64_t s2 = slot.seq.load(std::memory_order_acquire);
        if (s1 != s2) {
            return std::nullopt;
        }
        return item;
    }

    static void write(Slot<T>& slot, const T& item, uint64_t newSeq) noexcept {
        slot.payload.store(item, std::memory_order_relaxed);
        slot.seq.store(newSeq, std::memory_order_release);
    }
};

}  // namespace cfx