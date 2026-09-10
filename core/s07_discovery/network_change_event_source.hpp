#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/error_code.hpp"

namespace cfx {

enum class NetworkChangeType : uint8_t {
    IpAddressAdded,
    IpAddressRemoved,
    InterfaceUp,
    InterfaceDown,
};

struct NetworkChangeEvent {
    NetworkChangeType type;
    std::string interfaceName;
    std::string ipAddress;
};

class INetworkChangeEventSource {
public:
    using NetworkChangeCallback = std::function<void(const NetworkChangeEvent&)>;

    virtual ~INetworkChangeEventSource() = default;
    virtual std::optional<ErrorCode> start() noexcept = 0;
    virtual std::optional<ErrorCode> stop() noexcept = 0;
    virtual bool isRunning() const noexcept = 0;
    virtual void setCallback(NetworkChangeCallback cb) noexcept = 0;
};

class NetworkChangeEventSource : public INetworkChangeEventSource {
public:
    NetworkChangeEventSource();
    ~NetworkChangeEventSource();

    NetworkChangeEventSource(const NetworkChangeEventSource&) = delete;
    NetworkChangeEventSource& operator=(const NetworkChangeEventSource&) = delete;

    std::optional<ErrorCode> start() noexcept override;
    std::optional<ErrorCode> stop() noexcept override;
    bool isRunning() const noexcept override { return running_.load(); }
    void setCallback(NetworkChangeCallback cb) noexcept override { callback_ = std::move(cb); }

    void simulateNetworkChange(const NetworkChangeEvent& event) noexcept;

private:
    std::atomic<bool> running_{false};
    NetworkChangeCallback callback_;

#ifdef _WIN32
    void* notifyHandle_{nullptr};
    void* waitThread_{nullptr};
    std::atomic<bool> stopFlag_{false};
#elif defined(__APPLE__)
    void* dynamicStore_{nullptr};
    void* runLoopSource_{nullptr};
    void* monitorThread_{nullptr};
    std::atomic<bool> stopFlag_{false};
#else
    void* monitorThread_{nullptr};
    std::atomic<bool> stopFlag_{false};
#endif
};

}  // namespace cfx