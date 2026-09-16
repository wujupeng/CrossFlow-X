#include "mac/dual_channel_spsc.hpp"

namespace cfx {

bool DualChannelSpsc::isStateEvent(const RawInputEventFlat& event) noexcept {
    const auto kind = static_cast<RawEventKind>(event.kind);
    return kind == RawEventKind::KeyPress ||
           kind == RawEventKind::KeyRelease ||
           kind == RawEventKind::MouseButtonPress ||
           kind == RawEventKind::MouseButtonRelease;
}

DualChannelSpsc::ChannelResult DualChannelSpsc::enqueue(const RawInputEventFlat& event) noexcept {
    if (isStateEvent(event)) {
        if (stateChannel_.tryPush(event)) {
            return ChannelResult::Success;
        }
        stateChannelSaturated_.store(true, std::memory_order_release);
        stateChannelSaturatedCount_.fetch_add(1, std::memory_order_relaxed);
        return ChannelResult::StateSaturated;
    }

    dataChannel_.tryPushDropOldest(event);
    if (dataChannel_.cumulativeDropCount() > 0) {
        return ChannelResult::DataDroppedOldest;
    }
    return ChannelResult::Success;
}

}  // namespace cfx