#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"

namespace cfx {

class UdpBroadcast {
public:
    UdpBroadcast() = default;
    ~UdpBroadcast();

    UdpBroadcast(const UdpBroadcast&) = delete;
    UdpBroadcast& operator=(const UdpBroadcast&) = delete;

    std::optional<ErrorCode> open(u16 port) noexcept;
    void close() noexcept;

    std::optional<ErrorCode> send(const std::vector<u8>& data,
                                   const std::string& address = "255.255.255.255",
                                   u16 targetPort = 0) noexcept;

    std::optional<std::vector<u8>> receive(std::chrono::milliseconds timeout) noexcept;

    bool isOpen() const noexcept { return socketFd_ >= 0; }

private:
    int socketFd_{-1};
    u16 port_{0};
};

}  // namespace cfx