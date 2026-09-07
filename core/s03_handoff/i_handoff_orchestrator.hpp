#pragma once

#include "common/domain.hpp"
#include "common/messages.hpp"

namespace cfx {

class IHandoffOrchestrator {
public:
    virtual ~IHandoffOrchestrator() = default;
    virtual HandoffOutcome initiate(const HandoffRequest& req) = 0;
    virtual HandoffResponse handleIncoming(const HandoffRequest& req) = 0;
    virtual void handleResponse(const HandoffResponse& resp) = 0;
    virtual void onLinkDown(const NodeId& remoteNodeId) = 0;
};

}  // namespace cfx