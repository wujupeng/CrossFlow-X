#pragma once

#include <optional>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"

namespace cfx {

class IEventNormalizer {
public:
    virtual ~IEventNormalizer() = default;
    virtual std::optional<CanonicalInputEvent> normalize(const RawInputEvent& raw) = 0;
    virtual ModifierState snapshotModifiers() = 0;
    virtual void alignModifiers(const ModifierState& target) = 0;
};

}  // namespace cfx