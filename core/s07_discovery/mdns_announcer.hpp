#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "s07_discovery/mdns.hpp"

namespace cfx {

class MdnsAnnouncer : public IMdnsAnnouncer {
public:
    MdnsAnnouncer();
    ~MdnsAnnouncer();

    MdnsAnnouncer(const MdnsAnnouncer&) = delete;
    MdnsAnnouncer& operator=(const MdnsAnnouncer&) = delete;

    std::optional<ErrorCode> start(const std::string& serviceName,
                                   u16 port,
                                   const std::vector<std::pair<std::string, std::string>>& txtRecord) noexcept override;
    std::optional<ErrorCode> stop() noexcept override;
    std::optional<ErrorCode> updateTxt(const std::vector<std::pair<std::string, std::string>>& txtRecord) noexcept override;
    bool isRunning() const noexcept override;

    static constexpr const char* kServiceType = "_crossflow-x._tcp";

private:
    bool running_{false};
    std::string serviceName_;
    u16 port_{0};
    std::vector<std::pair<std::string, std::string>> currentTxt_;

#ifdef _WIN32
    void* registerCancel_{nullptr};
    void* doneEvent_{nullptr};
    void* txtRecordData_{nullptr};
#elif defined(__APPLE__)
    void* serviceRef_{nullptr};
    void* txtRecordRef_{nullptr};
#endif

    std::vector<u8> encodeTxt(const std::vector<std::pair<std::string, std::string>>& txt) const noexcept;
};

}  // namespace cfx