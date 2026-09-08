#pragma once

#include <optional>
#include <string>

#include "common/domain.hpp"
#include "common/error_code.hpp"

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

    std::optional<ErrorCode> start(const DiscoveryDigest& digest, u16 port) noexcept;
    std::optional<ErrorCode> stop() noexcept;
    std::optional<ErrorCode> broadcast(const DiscoveryDigest& digest) noexcept;
    bool isRunning() const noexcept { return running_; }

private:
    bool running_{false};
    u16 port_{0};
};

}  // namespace cfx