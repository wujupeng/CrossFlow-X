#include "s07_discovery/network_change_event_source.hpp"

#include <thread>

namespace cfx {

NetworkChangeEventSource::NetworkChangeEventSource() = default;

NetworkChangeEventSource::~NetworkChangeEventSource() {
    stop();
}

std::optional<ErrorCode> NetworkChangeEventSource::start() noexcept {
    if (running_.load()) {
        return ErrorCode::DiscAnnounceLost;
    }
    running_.store(true);
    stopFlag_.store(false);

#ifdef _WIN32
    // Windows: NotifyAddrChange would be used here for real implementation.
    // For now, we set up the source as ready; real IP change monitoring
    // requires NotifyAddrChange/NotifyRouteChange with OVERLAPPED I/O.
    notifyHandle_ = nullptr;
    waitThread_ = nullptr;
#elif defined(__APPLE__)
    // macOS: SCDynamicStore would be used here for real implementation.
    // Requires SCDynamicStoreCreate + SCDynamicStoreNotifyValue + CFRunLoopAddSource.
    dynamicStore_ = nullptr;
    runLoopSource_ = nullptr;
#else
    monitorThread_ = nullptr;
#endif

    return std::nullopt;
}

std::optional<ErrorCode> NetworkChangeEventSource::stop() noexcept {
    if (!running_.load()) {
        return std::nullopt;
    }
    stopFlag_.store(true);
    running_.store(false);

#ifdef _WIN32
    if (notifyHandle_ != nullptr) {
        // Cancel notification
        notifyHandle_ = nullptr;
    }
    waitThread_ = nullptr;
#elif defined(__APPLE__)
    if (runLoopSource_ != nullptr) {
        runLoopSource_ = nullptr;
    }
    dynamicStore_ = nullptr;
#else
    monitorThread_ = nullptr;
#endif

    return std::nullopt;
}

void NetworkChangeEventSource::simulateNetworkChange(const NetworkChangeEvent& event) noexcept {
    if (running_.load() && callback_) {
        callback_(event);
    }
}

}  // namespace cfx