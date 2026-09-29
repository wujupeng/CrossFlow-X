#include "win/win_input_plane_receiver.hpp"
#include "win/win_event_injector.hpp"

#include <cstdio>

namespace cfx {

WinInputPlaneReceiver::WinInputPlaneReceiver(WinEventInjector& injector, NodeId expectedSourceNodeId) noexcept
    : injector_(injector)
    , expectedSourceNodeId_(expectedSourceNodeId) {}

void WinInputPlaneReceiver::onEvent(std::function<void(const CanonicalInputEvent&)> handler) noexcept {
    handler_ = std::move(handler);
}

void WinInputPlaneReceiver::handleReceivedEvent(const CanonicalInputEvent& event) noexcept {
    totalReceived_.fetch_add(1, std::memory_order_relaxed);

    if (event.sourceNodeId != expectedSourceNodeId_) {
        fprintf(stderr, "[CFX-E-INJ-UNAUTHORIZED-SOURCE] WinInputPlaneReceiver rejected non-controller event\n");
        totalRejected_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    if (handler_) {
        handler_(event);
    }

    auto result = injector_.inject(event);
    if (result.ok) {
        totalInjected_.fetch_add(1, std::memory_order_relaxed);
    } else {
        totalRejected_.fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace cfx