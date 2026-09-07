#pragma once

#include "common/domain.hpp"
#include "common/messages.hpp"

namespace cfx {

class ICoordMapper {
public:
    virtual ~ICoordMapper() = default;
    virtual EntryCoord mapOverflow(const OverflowMapInput& input) = 0;
    virtual EntryCoord clamp(const EntryCoord& coord, const ScreenBoundary& targetBoundary) = 0;
};

}  // namespace cfx