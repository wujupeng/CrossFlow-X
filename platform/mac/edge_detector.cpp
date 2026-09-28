#include "mac/edge_detector.hpp"

namespace cfx {

std::optional<EdgeOverflowEvent> EdgeDetector::detect(int32_t cursorX,
                                                       int32_t cursorY,
                                                       const ScreenBoundary& boundary) const noexcept {
    if (!boundary.isValid()) {
        return std::nullopt;
    }

    const int32_t width = static_cast<int32_t>(boundary.width);
    const int32_t height = static_cast<int32_t>(boundary.height);

    if (cursorY < 0 || cursorY > height) {
        return std::nullopt;
    }

    if (cursorX < 0) {
        EdgeOverflowEvent event;
        event.direction = EdgeDirection::Left;
        event.overflow = static_cast<uint32_t>(-cursorX);
        event.cursorY = static_cast<uint32_t>(cursorY);
        return event;
    }

    if (cursorX > width) {
        EdgeOverflowEvent event;
        event.direction = EdgeDirection::Right;
        event.overflow = static_cast<uint32_t>(cursorX - width);
        event.cursorY = static_cast<uint32_t>(cursorY);
        return event;
    }

    return std::nullopt;
}

}  // namespace cfx