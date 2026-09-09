#include "s07_discovery/udp_broadcast.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#endif

#include <cstring>
#include <chrono>

namespace cfx {

namespace {

#ifdef _WIN32
class WinsockInit {
public:
    WinsockInit() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    ~WinsockInit() { WSACleanup(); }
};

WinsockInit& winsockInit() {
    static WinsockInit init;
    return init;
}

using SocketType = SOCKET;
constexpr SocketType kInvalidSocket = INVALID_SOCKET;

int closeSocket(SocketType s) { return closesocket(s); }

int pollSocket(SocketType s, int timeoutMs) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(s, &readSet);
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    return ::select(static_cast<int>(s + 1), &readSet, nullptr, nullptr, &tv);
}

#else
using SocketType = int;
constexpr SocketType kInvalidSocket = -1;

int closeSocket(SocketType s) { return ::close(s); }

int pollSocket(SocketType s, int timeoutMs) {
    struct pollfd fd;
    fd.fd = s;
    fd.events = POLLIN;
    fd.revents = 0;
    return ::poll(&fd, 1, timeoutMs);
}
#endif

}  // namespace

UdpBroadcast::~UdpBroadcast() {
    close();
}

std::optional<ErrorCode> UdpBroadcast::open(u16 port) noexcept {
#ifdef _WIN32
    winsockInit();
#endif

    SocketType sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == kInvalidSocket) return ErrorCode::DiscMdnsUnavailable;

    int broadcastEnable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
                   reinterpret_cast<const char*>(&broadcastEnable),
                   sizeof(broadcastEnable)) < 0) {
        closeSocket(sock);
        return ErrorCode::DiscMdnsUnavailable;
    }

    int reuseEnable = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseEnable),
               sizeof(reuseEnable));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        closeSocket(sock);
        return ErrorCode::DiscMdnsUnavailable;
    }

    socketFd_ = static_cast<int>(sock);
    port_ = port;
    return std::nullopt;
}

void UdpBroadcast::close() noexcept {
    if (socketFd_ >= 0) {
        closeSocket(static_cast<SocketType>(socketFd_));
        socketFd_ = -1;
    }
}

std::optional<ErrorCode> UdpBroadcast::send(const std::vector<u8>& data,
                                             const std::string& address,
                                             u16 targetPort) noexcept {
    if (socketFd_ < 0) return ErrorCode::DiscAnnounceLost;
    if (data.empty()) return std::nullopt;

    u16 actualPort = targetPort != 0 ? targetPort : port_;

    struct sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(actualPort);
    inet_pton(AF_INET, address.c_str(), &dest.sin_addr);

    int sent = ::sendto(static_cast<SocketType>(socketFd_),
                        reinterpret_cast<const char*>(data.data()),
                        static_cast<int>(data.size()),
                        0,
                        reinterpret_cast<struct sockaddr*>(&dest),
                        sizeof(dest));

    if (sent < 0) return ErrorCode::DiscAnnounceLost;
    return std::nullopt;
}

std::optional<std::vector<u8>> UdpBroadcast::receive(std::chrono::milliseconds timeout) noexcept {
    if (socketFd_ < 0) return std::nullopt;

    int ready = pollSocket(static_cast<SocketType>(socketFd_),
                           static_cast<int>(timeout.count()));
    if (ready <= 0) return std::nullopt;

    std::vector<u8> buffer(4096);
    struct sockaddr_in src{};
    socklen_t srcLen = sizeof(src);

    int received = ::recvfrom(static_cast<SocketType>(socketFd_),
                              reinterpret_cast<char*>(buffer.data()),
                              static_cast<int>(buffer.size()),
                              0,
                              reinterpret_cast<struct sockaddr*>(&src),
                              &srcLen);

    if (received <= 0) return std::nullopt;
    buffer.resize(static_cast<size_t>(received));
    return buffer;
}

}  // namespace cfx