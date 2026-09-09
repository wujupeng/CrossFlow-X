#include "s07_discovery/mdns.hpp"

namespace cfx {

std::optional<ErrorCode> LanBroadcastFallback::start(const DiscoveryDigest& digest, u16 port) noexcept {
    auto err = udp_.open(port);
    if (err) return err;
    port_ = port;
    currentDigest_ = digest;
    running_ = true;
    return std::nullopt;
}

std::optional<ErrorCode> LanBroadcastFallback::stop() noexcept {
    udp_.close();
    running_ = false;
    return std::nullopt;
}

std::vector<u8> LanBroadcastFallback::serializeDigest(const DiscoveryDigest& digest) const noexcept {
    std::vector<u8> data;
    data.reserve(64);

    auto appendU64 = [&](u64 v) {
        for (int i = 7; i >= 0; --i) {
            data.push_back(static_cast<u8>((v >> (i * 8)) & 0xFF));
        }
    };

    appendU64(digest.nodeId.high);
    appendU64(digest.nodeId.low);
    data.push_back(static_cast<u8>(digest.platform));
    appendU64(digest.sessionEpoch.value);
    appendU64(static_cast<u64>(digest.protocolVersion));

    for (char c : digest.topologyId) {
        data.push_back(static_cast<u8>(c));
    }
    data.push_back(0);

    return data;
}

std::optional<ErrorCode> LanBroadcastFallback::broadcast(const DiscoveryDigest& digest) noexcept {
    if (!running_) return ErrorCode::DiscAnnounceLost;

    auto data = serializeDigest(digest);
    auto err = udp_.send(data, "255.255.255.255", port_);
    if (err) return err;

    currentDigest_ = digest;
    return std::nullopt;
}

std::optional<std::vector<u8>> LanBroadcastFallback::receive(std::chrono::milliseconds timeout) noexcept {
    if (!running_) return std::nullopt;
    return udp_.receive(timeout);
}

}  // namespace cfx
