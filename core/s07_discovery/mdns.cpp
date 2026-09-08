#include "s07_discovery/mdns.hpp"

namespace cfx {

std::optional<ErrorCode> LanBroadcastFallback::start(const DiscoveryDigest& digest, u16 port) noexcept {
    (void)digest;
    port_ = port;
    running_ = true;
    return std::nullopt;
}

std::optional<ErrorCode> LanBroadcastFallback::stop() noexcept {
    running_ = false;
    return std::nullopt;
}

std::optional<ErrorCode> LanBroadcastFallback::broadcast(const DiscoveryDigest& digest) noexcept {
    if (!running_) return ErrorCode::DiscAnnounceLost;
    (void)digest;
    return std::nullopt;
}

}  // namespace cfx