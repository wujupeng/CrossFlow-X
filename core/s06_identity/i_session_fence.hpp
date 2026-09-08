#pragma once

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"

namespace cfx {

class ISessionFence {
public:
    virtual ~ISessionFence() = default;

    virtual SessionFenceVerdict checkIncoming(const SessionFence& incoming) const noexcept = 0;
    virtual SessionFenceVerdict checkIncoming(const NodeId& nodeId,
                                                const SessionEpoch& epoch,
                                                const SessionInstanceId& instance) const noexcept = 0;

    virtual SessionFence currentFence() const noexcept = 0;
    virtual SessionEpoch currentEpoch() const noexcept = 0;
    virtual SessionInstanceId currentInstance() const noexcept = 0;

    virtual std::optional<ErrorCode> startNewSession() noexcept = 0;
    virtual std::optional<ErrorCode> updateEpoch(SessionEpoch newEpoch) noexcept = 0;

    virtual bool isBootstrapMessage(const ControlMessage& msg) const noexcept = 0;
    virtual bool isEstablishedSessionMessage(const ControlMessage& msg) const noexcept = 0;
};

}  // namespace cfx