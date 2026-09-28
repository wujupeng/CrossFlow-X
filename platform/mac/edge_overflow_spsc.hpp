#pragma once

#include "common/domain.hpp"
#include "common/spsc_ring_buffer.hpp"

#include <cstdint>

namespace cfx {

class EdgeOverflowSpsc {
public:
    static constexpr uint64_t CAPACITY = 64;

    EdgeOverflowSpsc() noexcept = default;
    ~EdgeOverflowSpsc() noexcept = default;

    EdgeOverflowSpsc(const EdgeOverflowSpsc&) = delete;
    EdgeOverflowSpsc& operator=(const EdgeOverflowSpsc&) = delete;

    bool tryPushDropOldest(const EdgeOverflowEvent& event) noexcept;

    bool tryPop(EdgeOverflowEvent& outEvent) noexcept;

    uint64_t droppedOldestCount() const noexcept;

    bool isEmpty() const noexcept;

    uint64_t head() const noexcept;
    uint64_t tail() const noexcept;

private:
    SpscRingBuffer<EdgeOverflowEvent, CAPACITY> queue_;
};

}  // namespace cfx