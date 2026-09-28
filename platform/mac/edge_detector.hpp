#pragma once

#include "common/domain.hpp"

#include <optional>

namespace cfx {

class EdgeDetector {
public:
    EdgeDetector() noexcept = default;
    ~EdgeDetector() noexcept = default;

    EdgeDetector(const EdgeDetector&) = delete;
    EdgeDetector& operator=(const EdgeDetector&) = delete;

    std::optional<EdgeOverflowEvent> detect(int32_t cursorX,
                                             int32_t cursorY,
                                             const ScreenBoundary& boundary) const noexcept;
};

}  // namespace cfx