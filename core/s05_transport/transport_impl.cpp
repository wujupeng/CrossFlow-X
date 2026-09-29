#include "s05_transport/transport_impl.hpp"

#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace cfx {

TransportImpl::TransportImpl(const TransportConfig& config) noexcept
    : config_(config) {}

TransportImpl::~TransportImpl() noexcept {
    disconnect();
}

bool TransportImpl::connect() noexcept {
    connected_.store(true, std::memory_order_relaxed);
    if (linkStateHandler_) {
        LinkStateEvent event{};
        event.state = LinkState::Connected;
        linkStateHandler_(event);
    }
    return true;
}

void TransportImpl::disconnect() noexcept {
    if (connected_.load(std::memory_order_relaxed)) {
        connected_.store(false, std::memory_order_relaxed);
        if (linkStateHandler_) {
            LinkStateEvent event{};
            event.state = LinkState::Disconnected;
            linkStateHandler_(event);
        }
    }
}

void TransportImpl::send(const CanonicalInputEvent& event) {
    if (!connected_.load(std::memory_order_relaxed)) {
        backlog_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    auto data = serializeEvent(event);
    (void)data;

    totalSent_.fetch_add(1, std::memory_order_relaxed);
    uint32_t bl = backlog_.load(std::memory_order_relaxed);
    if (bl > 0) {
        backlog_.fetch_sub(1, std::memory_order_relaxed);
    }
}

void TransportImpl::onEvent(std::function<void(const CanonicalInputEvent&)> handler) {
    eventHandler_ = std::move(handler);
}

u32 TransportImpl::backlog() {
    return backlog_.load(std::memory_order_relaxed);
}

SendResult TransportImpl::send(const ControlMessage& msg) {
    if (!connected_.load(std::memory_order_relaxed)) {
        return {false, 0};
    }
    (void)msg;
    return {true, 0};
}

void TransportImpl::onMessage(std::function<void(const ControlMessage&)> handler) {
    messageHandler_ = std::move(handler);
}

void TransportImpl::onLinkState(std::function<void(const LinkStateEvent&)> handler) {
    linkStateHandler_ = std::move(handler);
}

std::vector<uint8_t> TransportImpl::serializeEvent(const CanonicalInputEvent& event) const noexcept {
    std::vector<uint8_t> data(sizeof(CanonicalInputEvent));
    std::memcpy(data.data(), &event, sizeof(CanonicalInputEvent));
    return data;
}

CanonicalInputEvent TransportImpl::deserializeEvent(const uint8_t* data, size_t len) const noexcept {
    CanonicalInputEvent event{};
    if (len >= sizeof(CanonicalInputEvent)) {
        std::memcpy(&event, data, sizeof(CanonicalInputEvent));
    }
    return event;
}

}  // namespace cfx
