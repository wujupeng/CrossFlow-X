#pragma once

#include <atomic>
#include <cstdint>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "common/spsc_ring_buffer.hpp"
#include "mac/mac_event_field_extractor.hpp"

namespace cfx {

class DualChannelSpsc {
public:
    DualChannelSpsc() noexcept = default;
    ~DualChannelSpsc() noexcept = default;

    DualChannelSpsc(const DualChannelSpsc&) = delete;
    DualChannelSpsc& operator=(const DualChannelSpsc&) = delete;

    enum class ChannelResult : uint8_t {
        Success,
        DataDroppedOldest,
        StateSaturated,
    };

    ChannelResult enqueue(const RawInputEventFlat& event) noexcept;

    bool tryPopData(RawInputEventFlat& out) noexcept {
        return dataChannel_.tryPop(out);
    }

    bool tryPopState(RawInputEventFlat& out) noexcept {
        return stateChannel_.tryPop(out);
    }

    bool isStateChannelSaturated() const noexcept {
        return stateChannelSaturated_.load(std::memory_order_acquire);
    }

    void clearStateChannelSaturated() noexcept {
        stateChannelSaturated_.store(false, std::memory_order_release);
    }

    uint64_t droppedOldestCount() const noexcept {
        return dataChannel_.cumulativeDropCount();
    }

    uint64_t stateChannelSaturatedCount() const noexcept {
        return stateChannelSaturatedCount_.load(std::memory_order_relaxed);
    }

    uint64_t dataChannelHead() const noexcept { return dataChannel_.head(); }
    uint64_t dataChannelTail() const noexcept { return dataChannel_.tail(); }
    uint64_t stateChannelHead() const noexcept { return stateChannel_.head(); }
    uint64_t stateChannelTail() const noexcept { return stateChannel_.tail(); }

    bool dataEmpty() const noexcept { return dataChannel_.isEmpty(); }
    bool stateEmpty() const noexcept { return stateChannel_.isEmpty(); }

    static constexpr uint64_t dataCapacity() noexcept { return SPSC_DATA_CAPACITY; }
    static constexpr uint64_t stateCapacity() noexcept { return SPSC_STATE_CAPACITY; }

private:
    SpscRingBuffer<RawInputEventFlat, SPSC_DATA_CAPACITY> dataChannel_;
    SpscRingBuffer<RawInputEventFlat, SPSC_STATE_CAPACITY> stateChannel_;

    std::atomic<bool> stateChannelSaturated_{false};
    std::atomic<uint64_t> stateChannelSaturatedCount_{0};

    static bool isStateEvent(const RawInputEventFlat& event) noexcept;
};

}  // namespace cfx