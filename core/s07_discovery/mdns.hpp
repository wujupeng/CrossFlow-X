#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "s07_discovery/udp_broadcast.hpp"

namespace cfx {

class IMdnsAnnouncer {
public:
    virtual ~IMdnsAnnouncer() = default;
    virtual std::optional<ErrorCode> start(const std::string& serviceName,
                                            u16 port,
                                            const std::vector<std::pair<std::string, std::string>>& txtRecord) noexcept = 0;
    virtual std::optional<ErrorCode> stop() noexcept = 0;
    virtual std::optional<ErrorCode> updateTxt(const std::vector<std::pair<std::string, std::string>>& txtRecord) noexcept = 0;
    virtual bool isRunning() const noexcept = 0;
};

class IMdnsListener {
public:
    virtual ~IMdnsListener() = default;
    virtual std::optional<ErrorCode> start(const std::string& serviceType) noexcept = 0;
    virtual std::optional<ErrorCode> stop() noexcept = 0;
    virtual bool isRunning() const noexcept = 0;
};

class LanBroadcastFallback {
public:
    LanBroadcastFallback() = default;
    ~LanBroadcastFallback() = default;

    LanBroadcastFallback(const LanBroadcastFallback&) = delete;
    LanBroadcastFallback& operator=(const LanBroadcastFallback&) = delete;

    std::optional<ErrorCode> start(const DiscoveryDigest& digest, u16 port) noexcept;
    std::optional<ErrorCode> stop() noexcept;
    std::optional<ErrorCode> broadcast(const DiscoveryDigest& digest) noexcept;
    bool isRunning() const noexcept { return running_; }

    std::optional<std::vector<u8>> receive(std::chrono::milliseconds timeout) noexcept;

private:
    UdpBroadcast udp_;
    bool running_{false};
    u16 port_{0};
    DiscoveryDigest currentDigest_;

    std::vector<u8> serializeDigest(const DiscoveryDigest& digest) const noexcept;
};

}  // namespace cfx
