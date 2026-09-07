#pragma once

#include <functional>

#include "common/domain.hpp"
#include "common/messages.hpp"

namespace cfx {

class IControlPlaneChannel {
public:
    virtual ~IControlPlaneChannel() = default;
    virtual SendResult send(const ControlMessage& msg) = 0;
    virtual void onMessage(std::function<void(const ControlMessage&)> handler) = 0;
    virtual void onLinkState(std::function<void(const LinkStateEvent&)> handler) = 0;
};

}  // namespace cfx