#include "s06_identity/session_fence.hpp"

#include <variant>

namespace cfx {

SessionFenceImpl::SessionFenceImpl(NodeId localNodeId) {
    fence_ = SessionFence::newSession(localNodeId);
}

SessionFenceVerdict SessionFenceImpl::checkIncoming(const SessionFence& incoming) const noexcept {
    return checkIncoming(incoming.nodeId, incoming.epoch, incoming.instanceId);
}

SessionFenceVerdict SessionFenceImpl::checkIncoming(const NodeId& nodeId,
                                                     const SessionEpoch& epoch,
                                                     const SessionInstanceId& instance) const noexcept {
    if (nodeId.isNull()) return SessionFenceVerdict::UnknownNode;
    if (nodeId != fence_.nodeId) return SessionFenceVerdict::UnknownNode;
    if (instance.nodeId == nodeId && instance.createdAt != fence_.instanceId.createdAt && instance.isActive)
        return SessionFenceVerdict::InstanceConflict;
    if (!epoch.isMonotonicAfter(fence_.epoch) && epoch.value != fence_.epoch.value)
        return SessionFenceVerdict::StaleEpoch;
    return SessionFenceVerdict::Accept;
}

SessionFence SessionFenceImpl::currentFence() const noexcept { return fence_; }
SessionEpoch SessionFenceImpl::currentEpoch() const noexcept { return fence_.epoch; }
SessionInstanceId SessionFenceImpl::currentInstance() const noexcept { return fence_.instanceId; }

std::optional<ErrorCode> SessionFenceImpl::startNewSession() noexcept {
    fence_.epoch.increment();
    fence_.instanceId.nodeId = fence_.nodeId;
    fence_.instanceId.createdAt++;
    fence_.instanceId.isActive = true;
    return std::nullopt;
}

std::optional<ErrorCode> SessionFenceImpl::updateEpoch(SessionEpoch newEpoch) noexcept {
    if (!newEpoch.isMonotonicAfter(fence_.epoch)) return ErrorCode::SessStaleEpoch;
    fence_.epoch = newEpoch;
    return std::nullopt;
}

bool SessionFenceImpl::isBootstrapMessage(const ControlMessage& msg) const noexcept {
    return std::holds_alternative<DiscoveryAnnouncement>(msg)
        || std::holds_alternative<PairingRequest>(msg)
        || std::holds_alternative<PairingResponse>(msg);
}

bool SessionFenceImpl::isEstablishedSessionMessage(const ControlMessage& msg) const noexcept {
    return std::holds_alternative<RegistrationRequest>(msg)
        || std::holds_alternative<RegistrationResponse>(msg)
        || std::holds_alternative<MembershipChangeNotification>(msg)
        || std::holds_alternative<IdentityRecoveryRequest>(msg)
        || std::holds_alternative<IdentityRecoveryResponse>(msg)
        || std::holds_alternative<GoodbyeAnnouncement>(msg);
}

}  // namespace cfx