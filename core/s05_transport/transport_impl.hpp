#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "common/domain.hpp"
#include "common/messages.hpp"
#include "s05_transport/i_input_plane_channel.hpp"
#include "s05_transport/i_control_plane_channel.hpp"

namespace cfx {

struct TransportConfig {
    std::string remoteHost;
    uint16_t inputPlanePort{0};
    uint16_t controlPlanePort{0};
    bool isServer{false};
};

class TransportImpl : public IInputPlaneChannel, public IControlPlaneChannel {
public:
    explicit TransportImpl(const TransportConfig& config) noexcept;
    ~TransportImpl() noexcept override;

    TransportImpl(const TransportImpl&) = delete;
    TransportImpl& operator=(const TransportImpl&) = delete;

    void send(const CanonicalInputEvent& event) override;
    void onEvent(std::function<void(const CanonicalInputEvent&)> handler) override;
    u32 backlog() override;

    SendResult send(const ControlMessage& msg) override;
    void onMessage(std::function<void(const ControlMessage&)> handler) override;
    void onLinkState(std::function<void(const LinkStateEvent&)> handler) override;

    bool connect() noexcept;
    void disconnect() noexcept;
    bool isConnected() const noexcept { return connected_.load(std::memory_order_relaxed); }

    uint64_t totalSent() const noexcept { return totalSent_.load(std::memory_order_relaxed); }
    uint64_t totalReceived() const noexcept { return totalReceived_.load(std::memory_order_relaxed); }

private:
    TransportConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<uint32_t> backlog_{0};
    std::atomic<uint64_t> totalSent_{0};
    std::atomic<uint64_t> totalReceived_{0};

    std::function<void(const CanonicalInputEvent&)> eventHandler_;
    std::function<void(const ControlMessage&)> messageHandler_;
    std::function<void(const LinkStateEvent&)> linkStateHandler_;

    std::vector<uint8_t> serializeEvent(const CanonicalInputEvent& event) const noexcept;
    CanonicalInputEvent deserializeEvent(const uint8_t* data, size_t len) const noexcept;
};

}  // namespace cfx