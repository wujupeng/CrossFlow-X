#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
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
    bool isConnected() const noexcept { return linkUp_.load(std::memory_order_relaxed); }

    uint64_t totalSent() const noexcept { return totalSent_.load(std::memory_order_relaxed); }
    uint64_t totalReceived() const noexcept { return totalReceived_.load(std::memory_order_relaxed); }

private:
    TransportConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> linkUp_{false};
    std::atomic<uint32_t> backlog_{0};
    std::atomic<uint64_t> totalSent_{0};
    std::atomic<uint64_t> totalReceived_{0};

    std::function<void(const CanonicalInputEvent&)> eventHandler_;
    std::function<void(const ControlMessage&)> messageHandler_;
    std::function<void(const LinkStateEvent&)> linkStateHandler_;

    std::mutex sendMutex_;
    std::mutex handlerMutex_;
    std::mutex socketMutex_;

#ifdef _WIN32
    using SocketFd = unsigned long long;
    static constexpr SocketFd kInvalidSocket = ~0ULL;
#else
    using SocketFd = int;
    static constexpr SocketFd kInvalidSocket = -1;
#endif

    SocketFd inputSocket_{kInvalidSocket};
    SocketFd controlSocket_{kInvalidSocket};
    SocketFd listenInputSocket_{kInvalidSocket};
    SocketFd listenControlSocket_{kInvalidSocket};

    std::thread inputReceiveThread_;
    std::thread controlReceiveThread_;
    std::atomic<bool> receiving_{false};

    void closeSocket(SocketFd fd) noexcept;
    bool sendRaw(SocketFd fd, const void* data, size_t len) noexcept;
    bool recvRaw(SocketFd fd, void* data, size_t len) noexcept;

    bool createClientSockets() noexcept;
    bool createServerSockets() noexcept;
    SocketFd acceptConnection(SocketFd listenFd) noexcept;
    bool reacceptInput() noexcept;
    bool reacceptControl() noexcept;
    void inputReceiveLoop() noexcept;
    void controlReceiveLoop() noexcept;

    std::vector<uint8_t> serializeEvent(const CanonicalInputEvent& event) const noexcept;
    CanonicalInputEvent deserializeEvent(const uint8_t* data, size_t len) const noexcept;
    std::vector<uint8_t> serializeMessage(const ControlMessage& msg) const noexcept;
    ControlMessage deserializeMessage(const uint8_t* data, size_t len) const noexcept;

    void notifyLinkState(LinkState state) noexcept;
};

}  // namespace cfx
