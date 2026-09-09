#include "s07_discovery/manual_config_fallback.hpp"

#include <fstream>
#include <sstream>

namespace cfx {

std::optional<ErrorCode> ManualConfigFallback::loadFromFile(const std::string& path) noexcept {
    std::ifstream ifs(path);
    if (!ifs) return ErrorCode::DiscManualFallback;

    endpoints_.clear();
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        ManualEndpoint ep;
        ep.host = line.substr(0, colonPos);
        try {
            ep.port = static_cast<u16>(std::stoul(line.substr(colonPos + 1)));
        } catch (...) {
            continue;
        }

        auto spacePos = line.find(' ', colonPos + 1);
        if (spacePos != std::string::npos) {
            std::string nodeIdHex = line.substr(spacePos + 1);
            if (nodeIdHex.size() >= 32) {
                try {
                    ep.nodeId.high = std::stoull(nodeIdHex.substr(0, 16), nullptr, 16);
                    ep.nodeId.low = std::stoull(nodeIdHex.substr(16, 16), nullptr, 16);
                } catch (...) {
                }
            }
        }

        endpoints_.push_back(std::move(ep));
    }

    return std::nullopt;
}

std::optional<ErrorCode> ManualConfigFallback::loadFromList(const std::vector<ManualEndpoint>& endpoints) noexcept {
    endpoints_ = endpoints;
    return std::nullopt;
}

std::optional<ErrorCode> ManualConfigFallback::start() noexcept {
    if (endpoints_.empty()) return ErrorCode::DiscManualFallback;
    running_ = true;
    return std::nullopt;
}

std::optional<ErrorCode> ManualConfigFallback::stop() noexcept {
    running_ = false;
    return std::nullopt;
}

std::optional<ManualEndpoint> ManualConfigFallback::findByNodeId(const NodeId& nodeId) const noexcept {
    for (const auto& ep : endpoints_) {
        if (ep.nodeId == nodeId) return ep;
    }
    return std::nullopt;
}

}  // namespace cfx