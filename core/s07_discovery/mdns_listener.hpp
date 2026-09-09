#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "s07_discovery/mdns.hpp"

namespace cfx {

class MdnsListener : public IMdnsListener {
public:
    using DiscoveryCallback = std::function<void(
        const std::string& serviceName,
        const std::string& host,
        u16 port,
        const std::vector<std::pair<std::string, std::string>>& txtRecord)>;

    MdnsListener();
    ~MdnsListener();

    MdnsListener(const MdnsListener&) = delete;
    MdnsListener& operator=(const MdnsListener&) = delete;

    std::optional<ErrorCode> start(const std::string& serviceType) noexcept override;
    std::optional<ErrorCode> stop() noexcept override;
    bool isRunning() const noexcept override;

    void setDiscoveryCallback(DiscoveryCallback cb) noexcept { callback_ = std::move(cb); }
    std::optional<ErrorCode> poll(std::chrono::milliseconds timeout) noexcept;

private:
    bool running_{false};
    std::string serviceType_;
    DiscoveryCallback callback_;

#ifdef _WIN32
    void* browseCancel_{nullptr};
    void* doneEvent_{nullptr};
#elif defined(__APPLE__)
    void* serviceRef_{nullptr};
#endif

    static std::vector<std::pair<std::string, std::string>> parseTxt(const u8* data, u16 length) noexcept;
};

}  // namespace cfx