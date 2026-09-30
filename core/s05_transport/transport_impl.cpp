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

#ifdef _WIN32
static bool wsaInitialized = false;
static void ensureWsaInit() noexcept {
    if (!wsaInitialized) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        wsaInitialized = true;
    }
}
static void wsaCleanup() noexcept {
    if (wsaInitialized) {
        WSACleanup();
        wsaInitialized = false;
    }
}
#else
static void ensureWsaInit() noexcept {}
static void wsaCleanup() noexcept {}
#endif

TransportImpl::TransportImpl(const TransportConfig& config) noexcept
    : config_(config) {
    ensureWsaInit();
}

TransportImpl::~TransportImpl() noexcept {
    disconnect();
    wsaCleanup();
}

void TransportImpl::closeSocket(SocketFd fd) noexcept {
    if (fd == kInvalidSocket) return;
#ifdef _WIN32
    ::closesocket(static_cast<SOCKET>(fd));
#else
    ::close(static_cast<int>(fd));
#endif
}

bool TransportImpl::sendRaw(SocketFd fd, const void* data, size_t len) noexcept {
    if (fd == kInvalidSocket) return false;
    const auto* ptr = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = ::send(static_cast<SOCKET>(fd), ptr + sent, static_cast<int>(len - sent), 0);
#else
        ssize_t n = ::send(static_cast<int>(fd), ptr + sent, len - sent, 0);
#endif
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool TransportImpl::recvRaw(SocketFd fd, void* data, size_t len) noexcept {
    if (fd == kInvalidSocket) return false;
    auto* ptr = static_cast<char*>(data);
    size_t received = 0;
    while (received < len) {
#ifdef _WIN32
        int n = ::recv(static_cast<SOCKET>(fd), ptr + received, static_cast<int>(len - received), 0);
#else
        ssize_t n = ::recv(static_cast<int>(fd), ptr + received, len - received, 0);
#endif
        if (n <= 0) return false;
        received += static_cast<size_t>(n);
    }
    return true;
}

bool TransportImpl::createClientSockets() noexcept {
#ifdef _WIN32
    SOCKET sock1 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKET sock2 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    int sock1 = ::socket(AF_INET, SOCK_STREAM, 0);
    int sock2 = ::socket(AF_INET, SOCK_STREAM, 0);
#endif

    if (sock1 < 0 || sock2 < 0) {
        closeSocket(static_cast<SocketFd>(sock1));
        closeSocket(static_cast<SocketFd>(sock2));
        return false;
    }

    struct sockaddr_in addr1{};
    addr1.sin_family = AF_INET;
    addr1.sin_port = htons(config_.inputPlanePort);
    inet_pton(AF_INET, config_.remoteHost.c_str(), &addr1.sin_addr);

    struct sockaddr_in addr2{};
    addr2.sin_family = AF_INET;
    addr2.sin_port = htons(config_.controlPlanePort);
    inet_pton(AF_INET, config_.remoteHost.c_str(), &addr2.sin_addr);

    if (::connect(sock1, reinterpret_cast<struct sockaddr*>(&addr1), sizeof(addr1)) < 0) {
        closeSocket(static_cast<SocketFd>(sock1));
        closeSocket(static_cast<SocketFd>(sock2));
        return false;
    }

    if (::connect(sock2, reinterpret_cast<struct sockaddr*>(&addr2), sizeof(addr2)) < 0) {
        closeSocket(static_cast<SocketFd>(sock1));
        closeSocket(static_cast<SocketFd>(sock2));
        return false;
    }

    inputSocket_ = static_cast<SocketFd>(sock1);
    controlSocket_ = static_cast<SocketFd>(sock2);
    return true;
}

bool TransportImpl::createServerSockets() noexcept {
#ifdef _WIN32
    SOCKET listen1 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKET listen2 = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    int listen1 = ::socket(AF_INET, SOCK_STREAM, 0);
    int listen2 = ::socket(AF_INET, SOCK_STREAM, 0);
#endif

    if (listen1 < 0 || listen2 < 0) {
        closeSocket(static_cast<SocketFd>(listen1));
        closeSocket(static_cast<SocketFd>(listen2));
        return false;
    }

    int opt = 1;
    ::setsockopt(listen1, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    ::setsockopt(listen2, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    struct sockaddr_in addr1{};
    addr1.sin_family = AF_INET;
    addr1.sin_port = htons(config_.inputPlanePort);
    addr1.sin_addr.s_addr = INADDR_ANY;

    struct sockaddr_in addr2{};
    addr2.sin_family = AF_INET;
    addr2.sin_port = htons(config_.controlPlanePort);
    addr2.sin_addr.s_addr = INADDR_ANY;

    if (::bind(listen1, reinterpret_cast<struct sockaddr*>(&addr1), sizeof(addr1)) < 0 ||
        ::bind(listen2, reinterpret_cast<struct sockaddr*>(&addr2), sizeof(addr2)) < 0) {
        closeSocket(static_cast<SocketFd>(listen1));
        closeSocket(static_cast<SocketFd>(listen2));
        return false;
    }

    ::listen(listen1, 1);
    ::listen(listen2, 1);

    listenInputSocket_ = static_cast<SocketFd>(listen1);
    listenControlSocket_ = static_cast<SocketFd>(listen2);

#ifdef _WIN32
    SOCKET accepted1 = ::accept(listen1, nullptr, nullptr);
    SOCKET accepted2 = ::accept(listen2, nullptr, nullptr);
#else
    int accepted1 = ::accept(listen1, nullptr, nullptr);
    int accepted2 = ::accept(listen2, nullptr, nullptr);
#endif

    if (accepted1 < 0 || accepted2 < 0) {
        closeSocket(static_cast<SocketFd>(accepted1));
        closeSocket(static_cast<SocketFd>(accepted2));
        return false;
    }

    inputSocket_ = static_cast<SocketFd>(accepted1);
    controlSocket_ = static_cast<SocketFd>(accepted2);
    return true;
}

TransportImpl::SocketFd TransportImpl::acceptConnection(SocketFd listenFd) noexcept {
    if (listenFd == kInvalidSocket) return kInvalidSocket;
#ifdef _WIN32
    SOCKET accepted = ::accept(static_cast<SOCKET>(listenFd), nullptr, nullptr);
    if (accepted == INVALID_SOCKET) return kInvalidSocket;
    return static_cast<SocketFd>(accepted);
#else
    int accepted = ::accept(static_cast<int>(listenFd), nullptr, nullptr);
    if (accepted < 0) return kInvalidSocket;
    return static_cast<SocketFd>(accepted);
#endif
}

bool TransportImpl::reacceptInput() noexcept {
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        closeSocket(inputSocket_);
        inputSocket_ = kInvalidSocket;
    }

    linkUp_.store(false, std::memory_order_relaxed);
    notifyLinkState(LinkState::Disconnected);

    SocketFd listenFd;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        listenFd = listenInputSocket_;
    }

    SocketFd newFd = acceptConnection(listenFd);
    if (newFd == kInvalidSocket) {
        return false;
    }

    if (!receiving_.load(std::memory_order_relaxed)) {
        closeSocket(newFd);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        inputSocket_ = newFd;
    }
    linkUp_.store(true, std::memory_order_relaxed);
    notifyLinkState(LinkState::Connected);
    return true;
}

bool TransportImpl::reacceptControl() noexcept {
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        closeSocket(controlSocket_);
        controlSocket_ = kInvalidSocket;
    }

    SocketFd listenFd;
    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        listenFd = listenControlSocket_;
    }

    SocketFd newFd = acceptConnection(listenFd);
    if (newFd == kInvalidSocket) {
        return false;
    }

    if (!receiving_.load(std::memory_order_relaxed)) {
        closeSocket(newFd);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        controlSocket_ = newFd;
    }
    return true;
}

bool TransportImpl::connect() noexcept {
    if (connected_.load(std::memory_order_relaxed)) {
        return true;
    }

    bool ok = false;
    if (config_.isServer) {
        ok = createServerSockets();
    } else {
        ok = createClientSockets();
    }

    if (!ok) {
        notifyLinkState(LinkState::Disconnected);
        return false;
    }

    connected_.store(true, std::memory_order_relaxed);
    linkUp_.store(true, std::memory_order_relaxed);
    receiving_.store(true, std::memory_order_relaxed);

    inputReceiveThread_ = std::thread([this]() { inputReceiveLoop(); });
    controlReceiveThread_ = std::thread([this]() { controlReceiveLoop(); });

    notifyLinkState(LinkState::Connected);
    return true;
}

void TransportImpl::disconnect() noexcept {
    if (!connected_.load(std::memory_order_relaxed)) {
        return;
    }

    connected_.store(false, std::memory_order_relaxed);
    receiving_.store(false, std::memory_order_relaxed);
    linkUp_.store(false, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(socketMutex_);
        closeSocket(inputSocket_);
        closeSocket(controlSocket_);
        closeSocket(listenInputSocket_);
        closeSocket(listenControlSocket_);
        inputSocket_ = kInvalidSocket;
        controlSocket_ = kInvalidSocket;
        listenInputSocket_ = kInvalidSocket;
        listenControlSocket_ = kInvalidSocket;
    }

    if (inputReceiveThread_.joinable()) {
        inputReceiveThread_.join();
    }
    if (controlReceiveThread_.joinable()) {
        controlReceiveThread_.join();
    }

    notifyLinkState(LinkState::Disconnected);
}

void TransportImpl::send(const CanonicalInputEvent& event) {
    if (!connected_.load(std::memory_order_relaxed)) {
        backlog_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    auto data = serializeEvent(event);

    std::lock_guard<std::mutex> lock(sendMutex_);
    SocketFd fd;
    {
        std::lock_guard<std::mutex> sockLock(socketMutex_);
        fd = inputSocket_;
    }
    uint32_t len = static_cast<uint32_t>(data.size());
    uint32_t netLen = htonl(len);

    if (!sendRaw(fd, &netLen, sizeof(netLen)) ||
        !sendRaw(fd, data.data(), data.size())) {
        backlog_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    totalSent_.fetch_add(1, std::memory_order_relaxed);
    uint32_t bl = backlog_.load(std::memory_order_relaxed);
    if (bl > 0) {
        backlog_.fetch_sub(1, std::memory_order_relaxed);
    }
}

void TransportImpl::onEvent(std::function<void(const CanonicalInputEvent&)> handler) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    eventHandler_ = std::move(handler);
}

u32 TransportImpl::backlog() {
    return backlog_.load(std::memory_order_relaxed);
}

SendResult TransportImpl::send(const ControlMessage& msg) {
    if (!connected_.load(std::memory_order_relaxed)) {
        return {false, 0};
    }

    auto data = serializeMessage(msg);

    std::lock_guard<std::mutex> lock(sendMutex_);
    SocketFd fd;
    {
        std::lock_guard<std::mutex> sockLock(socketMutex_);
        fd = controlSocket_;
    }
    uint32_t len = static_cast<uint32_t>(data.size());
    uint32_t netLen = htonl(len);

    if (!sendRaw(fd, &netLen, sizeof(netLen)) ||
        !sendRaw(fd, data.data(), data.size())) {
        return {false, 0};
    }

    return {true, static_cast<u32>(totalSent_.fetch_add(1, std::memory_order_relaxed) + 1)};
}

void TransportImpl::onMessage(std::function<void(const ControlMessage&)> handler) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    messageHandler_ = std::move(handler);
}

void TransportImpl::onLinkState(std::function<void(const LinkStateEvent&)> handler) {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    linkStateHandler_ = std::move(handler);
}

void TransportImpl::inputReceiveLoop() noexcept {
    while (receiving_.load(std::memory_order_relaxed)) {
        SocketFd fd;
        {
            std::lock_guard<std::mutex> lock(socketMutex_);
            fd = inputSocket_;
        }

        uint32_t netLen = 0;
        if (!recvRaw(fd, &netLen, sizeof(netLen))) {
            if (config_.isServer && receiving_.load(std::memory_order_relaxed)) {
                if (reacceptInput()) continue;
            }
            break;
        }
        uint32_t len = ntohl(netLen);
        if (len == 0 || len > 65536) {
            if (config_.isServer && receiving_.load(std::memory_order_relaxed)) {
                if (reacceptInput()) continue;
            }
            break;
        }

        std::vector<uint8_t> data(len);
        if (!recvRaw(fd, data.data(), len)) {
            if (config_.isServer && receiving_.load(std::memory_order_relaxed)) {
                if (reacceptInput()) continue;
            }
            break;
        }

        auto event = deserializeEvent(data.data(), data.size());
        totalReceived_.fetch_add(1, std::memory_order_relaxed);

        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (eventHandler_) {
            eventHandler_(event);
        }
    }
}

void TransportImpl::controlReceiveLoop() noexcept {
    while (receiving_.load(std::memory_order_relaxed)) {
        SocketFd fd;
        {
            std::lock_guard<std::mutex> lock(socketMutex_);
            fd = controlSocket_;
        }

        uint32_t netLen = 0;
        if (!recvRaw(fd, &netLen, sizeof(netLen))) {
            if (config_.isServer && receiving_.load(std::memory_order_relaxed)) {
                if (reacceptControl()) continue;
            }
            break;
        }
        uint32_t len = ntohl(netLen);
        if (len == 0 || len > 65536) {
            if (config_.isServer && receiving_.load(std::memory_order_relaxed)) {
                if (reacceptControl()) continue;
            }
            break;
        }

        std::vector<uint8_t> data(len);
        if (!recvRaw(fd, data.data(), len)) {
            if (config_.isServer && receiving_.load(std::memory_order_relaxed)) {
                if (reacceptControl()) continue;
            }
            break;
        }

        auto msg = deserializeMessage(data.data(), data.size());

        std::lock_guard<std::mutex> lock(handlerMutex_);
        if (messageHandler_) {
            messageHandler_(msg);
        }
    }
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

std::vector<uint8_t> TransportImpl::serializeMessage(const ControlMessage& msg) const noexcept {
    std::vector<uint8_t> data(sizeof(ControlMessage));
    std::memcpy(data.data(), &msg, sizeof(ControlMessage));
    return data;
}

ControlMessage TransportImpl::deserializeMessage(const uint8_t* data, size_t len) const noexcept {
    ControlMessage msg{};
    if (len >= sizeof(ControlMessage)) {
        std::memcpy(&msg, data, sizeof(ControlMessage));
    }
    return msg;
}

void TransportImpl::notifyLinkState(LinkState state) noexcept {
    std::lock_guard<std::mutex> lock(handlerMutex_);
    if (linkStateHandler_) {
        LinkStateEvent event{};
        event.state = state;
        linkStateHandler_(event);
    }
}

}  // namespace cfx
