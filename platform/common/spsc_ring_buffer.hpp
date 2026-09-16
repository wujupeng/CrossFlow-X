#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

#include "common/spsc_slot.hpp"

namespace cfx {

inline constexpr uint64_t SPSC_DATA_CAPACITY = 256;
inline constexpr uint64_t SPSC_STATE_CAPACITY = 64;

template <typename T, uint64_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity > 0, "Capacity must be > 0");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

public:
    SpscRingBuffer() noexcept {
        for (uint64_t i = 0; i < Capacity; ++i) {
            buffer_[i].initialize(i);
        }
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    bool tryPush(const T& item) noexcept {
        const uint64_t h = head_.load(std::memory_order_relaxed);
        const uint64_t ct = tail_.load(std::memory_order_acquire);
        if (h - ct == Capacity) {
            return false;
        }
        const uint64_t i = h % Capacity;
        buffer_[i].payload.store(item, std::memory_order_relaxed);
        buffer_[i].seq.store(h + 1, std::memory_order_release);
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    bool tryPushDropOldest(const T& item) noexcept {
        const uint64_t h = head_.load(std::memory_order_relaxed);
        const uint64_t i = h % Capacity;
        buffer_[i].payload.store(item, std::memory_order_relaxed);
        buffer_[i].seq.store(h + 1, std::memory_order_release);
        head_.store(h + 1, std::memory_order_release);
        if (h - tail_.load(std::memory_order_acquire) >= Capacity) {
            cumulativeDropCount_.fetch_add(1, std::memory_order_relaxed);
        }
        return true;
    }

    bool tryPop(T& outItem) noexcept {
        uint64_t ct = tail_.load(std::memory_order_relaxed);
        const uint64_t h = head_.load(std::memory_order_acquire);

        const uint64_t logicalDropBoundary = (ct > (h > Capacity ? h - Capacity : 0))
                                                 ? ct
                                                 : (h > Capacity ? h - Capacity : 0);
        if (ct < logicalDropBoundary) {
            tail_.store(logicalDropBoundary, std::memory_order_release);
            ct = logicalDropBoundary;
        }

        if (ct >= h) {
            return false;
        }

        const uint64_t i = ct % Capacity;
        const uint64_t s1 = buffer_[i].seq.load(std::memory_order_acquire);
        if (s1 != ct + 1) {
            tail_.store(ct + 1, std::memory_order_release);
            return tryPop(outItem);
        }

        T item = buffer_[i].payload.load(std::memory_order_relaxed);
        const uint64_t s2 = buffer_[i].seq.load(std::memory_order_acquire);
        if (s1 != s2) {
            tail_.store(ct + 1, std::memory_order_release);
            return tryPop(outItem);
        }

        tail_.store(ct + 1, std::memory_order_release);
        outItem = item;
        return true;
    }

    uint64_t head() const noexcept {
        return head_.load(std::memory_order_acquire);
    }

    uint64_t tail() const noexcept {
        return tail_.load(std::memory_order_acquire);
    }

    uint64_t cumulativeDropCount() const noexcept {
        return cumulativeDropCount_.load(std::memory_order_relaxed);
    }

    uint64_t logicalDropBoundaryValue() const noexcept {
        const uint64_t h = head_.load(std::memory_order_acquire);
        const uint64_t t = tail_.load(std::memory_order_acquire);
        const uint64_t hMinusCap = (h > Capacity) ? (h - Capacity) : 0;
        return (t > hMinusCap) ? t : hMinusCap;
    }

    uint64_t pendingDroppedValue() const noexcept {
        const uint64_t ldb = logicalDropBoundaryValue();
        const uint64_t t = tail_.load(std::memory_order_acquire);
        return (ldb > t) ? (ldb - t) : 0;
    }

    bool isEmpty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    static constexpr uint64_t capacity() noexcept {
        return Capacity;
    }

#ifndef NDEBUG
    bool validateInv1() const noexcept {
        return tail() <= head();
    }

    bool validateInv2() const noexcept {
        const uint64_t h = head();
        const uint64_t ldb = logicalDropBoundaryValue();
        return (h - ldb) <= Capacity;
    }

    bool validateInv6() const noexcept {
        const uint64_t h = head();
        const uint64_t t = tail();
        const uint64_t hMinusCap = (h > Capacity) ? (h - Capacity) : 0;
        const uint64_t expected = (t > hMinusCap) ? t : hMinusCap;
        return logicalDropBoundaryValue() == expected;
    }

    bool validateInv7a() const noexcept {
        const uint64_t h = head();
        const uint64_t t = tail();
        const uint64_t ldb = logicalDropBoundaryValue();
        const uint64_t expectedPending = (ldb > t) ? (ldb - t) : 0;
        const uint64_t hMinusTMinusCap = (h > t + Capacity) ? (h - t - Capacity) : 0;
        return expectedPending == hMinusTMinusCap;
    }

    bool validateIntervalDecomposition() const noexcept {
        const uint64_t h = head();
        const uint64_t t = tail();
        const uint64_t ldb = logicalDropBoundaryValue();
        if (t > ldb) return false;
        if (ldb > h) return false;
        if (h - ldb > Capacity) return false;
        return true;
    }

    bool validateAllInvariants() const noexcept {
        return validateInv1() && validateInv2() && validateInv6() &&
               validateInv7a() && validateIntervalDecomposition();
    }
#endif

private:
    Slot<T> buffer_[Capacity];
    std::atomic<uint64_t> head_{0};
    std::atomic<uint64_t> tail_{0};
    std::atomic<uint64_t> cumulativeDropCount_{0};
};

}  // namespace cfx