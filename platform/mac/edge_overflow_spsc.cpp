#include "mac/edge_overflow_spsc.hpp"

namespace cfx {

bool EdgeOverflowSpsc::tryPushDropOldest(const EdgeOverflowEvent& event) noexcept {
    return queue_.tryPushDropOldest(event);
}

bool EdgeOverflowSpsc::tryPop(EdgeOverflowEvent& outEvent) noexcept {
    return queue_.tryPop(outEvent);
}

uint64_t EdgeOverflowSpsc::droppedOldestCount() const noexcept {
    return queue_.cumulativeDropCount();
}

bool EdgeOverflowSpsc::isEmpty() const noexcept {
    return queue_.isEmpty();
}

uint64_t EdgeOverflowSpsc::head() const noexcept {
    return queue_.head();
}

uint64_t EdgeOverflowSpsc::tail() const noexcept {
    return queue_.tail();
}

}  // namespace cfx