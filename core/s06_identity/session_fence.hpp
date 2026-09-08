#pragma once

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "common/messages.hpp"
#include "s06_identity/i_session_fence.hpp"

namespace cfx {

class SessionFenceImpl : public ISessionFence {
public:
    SessionFenceImpl() = default;
    explicit SessionFenceImpl(NodeId localNodeId);

    SessionFenceVerdict checkIncoming(const SessionFence& incoming) const noexcept override;
    SessionFenceVerdict checkIncoming(const NodeId& nodeId,
                                       const SessionEpoch& epoch,
                                       const SessionInstanceId& instance) const noexcept override;

    SessionFence currentFence() const noexcept override;
    SessionEpoch currentEpoch() const noexcept override;
    SessionInstanceId currentInstance() const noexcept override;

    std::optional<ErrorCode> startNewSession() noexcept override;
    std::optional<ErrorCode> updateEpoch(SessionEpoch newEpoch) noexcept override;

    bool isBootstrapMessage(const ControlMessage& msg) const noexcept override;
    bool isEstablishedSessionMessage(const ControlMessage& msg) const noexcept override;

private:
    SessionFence fence_;
};

}  // namespace cfx