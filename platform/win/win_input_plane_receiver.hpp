#pragma once

#include <functional>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "s05_transport/i_input_plane_channel.hpp"

namespace cfx {

class WinEventInjector;

class WinInputPlaneReceiver {
public:
    WinInputPlaneReceiver(WinEventInjector& injector, NodeId expectedSourceNodeId) noexcept;
    ~WinInputPlaneReceiver() noexcept = default;

    WinInputPlaneReceiver(const WinInputPlaneReceiver&) = delete;
    WinInputPlaneReceiver& operator=(const WinInputPlaneReceiver&) = delete;

    void onEvent(std::function<void(const CanonicalInputEvent&)> handler) noexcept;
    void handleReceivedEvent(const CanonicalInputEvent& event) noexcept;

    uint64_t totalReceived() const noexcept { return totalReceived_.load(std::memory_order_relaxed); }
    uint64_t totalRejected() const noexcept { return totalRejected_.load(std::memory_order_relaxed); }
    uint64_t totalInjected() const noexcept { return totalInjected_.load(std::memory_order_relaxed); }

private:
    WinEventInjector& injector_;
    NodeId expectedSourceNodeId_{};
    std::function<void(const CanonicalInputEvent&)> handler_;
    std::atomic<uint64_t> totalReceived_{0};
    std::atomic<uint64_t> totalRejected_{0};
    std::atomic<uint64_t> totalInjected_{0};
};

}  // namespace cfx