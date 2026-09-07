#pragma once

#include <cstdint>
#include <functional>

#include "common/domain.hpp"

namespace cfx {

class IInputPlaneChannel {
public:
    virtual ~IInputPlaneChannel() = default;
    virtual void send(const CanonicalInputEvent& event) = 0;
    virtual void onEvent(std::function<void(const CanonicalInputEvent&)> handler) = 0;
    virtual u32 backlog() = 0;
};

}  // namespace cfx