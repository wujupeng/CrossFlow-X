#include "s07_discovery/mdns_listener.hpp"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windns.h>
#pragma comment(lib, "dnsapi.lib")

namespace cfx {

namespace {

struct BrowseContext {
    MdnsListener::DiscoveryCallback callback;
};

void WINAPI browseCompleteCallback(DWORD status, PVOID pContext, PDNS_RECORD pRecord) {
    auto* ctx = static_cast<BrowseContext*>(pContext);
    if (!ctx || status != ERROR_SUCCESS) return;

    for (PDNS_RECORDA rec = reinterpret_cast<PDNS_RECORDA>(pRecord); rec; rec = rec->pNext) {
        if (rec->wType != DNS_TYPE_TEXT) continue;

        std::string serviceName;
        if (rec->pName) {
            serviceName = rec->pName;
        }

        std::vector<std::pair<std::string, std::string>> txt;
        auto& txtData = rec->Data.TXT;
        for (DWORD i = 0; i < txtData.dwStringCount; ++i) {
            std::string entry = txtData.pStringArray[i];
            auto eq = entry.find('=');
            if (eq != std::string::npos) {
                txt.emplace_back(entry.substr(0, eq), entry.substr(eq + 1));
            }
        }

        if (ctx->callback) {
            ctx->callback(serviceName, "", 0, txt);
        }
    }
}

}

MdnsListener::MdnsListener() = default;

MdnsListener::~MdnsListener() {
    stop();
}

std::optional<ErrorCode> MdnsListener::start(const std::string& serviceType) noexcept {
    if (running_) return std::nullopt;
    serviceType_ = serviceType.empty() ? "_crossflow-x._tcp" : serviceType;

    auto* ctx = new BrowseContext{};
    ctx->callback = callback_;

    DNS_SERVICE_BROWSE_REQUEST request{};
    request.Version = DNS_QUERY_REQUEST_VERSION1;
    request.InterfaceIndex = 0;
    request.QueryName = L"_crossflow-x._tcp.local";
    request.pBrowseCallback = browseCompleteCallback;
    request.pQueryContext = ctx;

    DNS_SERVICE_CANCEL cancel{};
    PDNS_SERVICE_CANCEL pCancel = &cancel;
    DWORD result = DnsServiceBrowse(&request, pCancel);
    if (result != ERROR_SUCCESS && result != DNS_REQUEST_PENDING) {
        delete ctx;
        return ErrorCode::DiscMdnsUnavailable;
    }

    browseCancel_ = pCancel;
    running_ = true;
    return std::nullopt;
}

std::optional<ErrorCode> MdnsListener::stop() noexcept {
    if (!running_) return std::nullopt;

    if (browseCancel_) {
        DnsServiceBrowseCancel(static_cast<PDNS_SERVICE_CANCEL>(browseCancel_));
        browseCancel_ = nullptr;
    }

    running_ = false;
    return std::nullopt;
}

bool MdnsListener::isRunning() const noexcept {
    return running_;
}

std::optional<ErrorCode> MdnsListener::poll(std::chrono::milliseconds) noexcept {
    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> MdnsListener::parseTxt(const u8* data, u16 length) noexcept {
    std::vector<std::pair<std::string, std::string>> result;
    u16 pos = 0;
    while (pos < length) {
        u8 entryLen = data[pos++];
        if (pos + entryLen > length) break;
        std::string entry(reinterpret_cast<const char*>(data + pos), entryLen);
        pos += entryLen;

        auto eq = entry.find('=');
        if (eq != std::string::npos) {
            result.emplace_back(entry.substr(0, eq), entry.substr(eq + 1));
        }
    }
    return result;
}

}

#elif defined(__APPLE__)
#include <dns_sd.h>
#include <arpa/inet.h>
#include <sys/select.h>

namespace cfx {

namespace {

struct BrowseContext {
    MdnsListener::DiscoveryCallback callback;
};

void DNSSD_API browseCallback(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType,
                              const char* serviceName, const char* regtype,
                              const char* domain, void* context) {
    auto* ctx = static_cast<BrowseContext*>(context);
    if (!ctx || !ctx->callback) return;
    ctx->callback(serviceName ? serviceName : "", domain ? domain : "", 0, {});
}

}

MdnsListener::MdnsListener() = default;

MdnsListener::~MdnsListener() {
    stop();
}

std::optional<ErrorCode> MdnsListener::start(const std::string& serviceType) noexcept {
    if (running_) return std::nullopt;
    serviceType_ = serviceType.empty() ? "_crossflow-x._tcp" : serviceType;

    auto* ctx = new BrowseContext{};
    ctx->callback = callback_;

    DNSServiceRef ref = nullptr;
    DNSServiceErrorType err = DNSServiceBrowse(
        &ref, 0, 0, "_crossflow-x._tcp", "local.",
        browseCallback, ctx);

    if (err != kDNSServiceErr_NoError) {
        delete ctx;
        return ErrorCode::DiscMdnsUnavailable;
    }

    serviceRef_ = ref;
    running_ = true;
    return std::nullopt;
}

std::optional<ErrorCode> MdnsListener::stop() noexcept {
    if (!running_) return std::nullopt;

    if (serviceRef_) {
        DNSServiceRefDeallocate(static_cast<DNSServiceRef>(serviceRef_));
        serviceRef_ = nullptr;
    }

    running_ = false;
    return std::nullopt;
}

bool MdnsListener::isRunning() const noexcept {
    return running_;
}

std::optional<ErrorCode> MdnsListener::poll(std::chrono::milliseconds timeout) noexcept {
    if (!running_ || !serviceRef_) return std::nullopt;

    DNSServiceRef ref = static_cast<DNSServiceRef>(serviceRef_);
    int fd = DNSServiceRefSockFD(ref);
    if (fd < 0) return std::nullopt;

    struct timeval tv;
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    int result = select(fd + 1, &readfds, nullptr, nullptr, &tv);
    if (result <= 0) return std::nullopt;

    DNSServiceErrorType err = DNSServiceProcessResult(ref);
    if (err != kDNSServiceErr_NoError) return ErrorCode::DiscMdnsUnavailable;

    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> MdnsListener::parseTxt(const u8* data, u16 length) noexcept {
    std::vector<std::pair<std::string, std::string>> result;
    u16 pos = 0;
    while (pos < length) {
        u8 entryLen = data[pos++];
        if (pos + entryLen > length) break;
        std::string entry(reinterpret_cast<const char*>(data + pos), entryLen);
        pos += entryLen;

        auto eq = entry.find('=');
        if (eq != std::string::npos) {
            result.emplace_back(entry.substr(0, eq), entry.substr(eq + 1));
        }
    }
    return result;
}

}

#else

namespace cfx {

MdnsListener::MdnsListener() = default;
MdnsListener::~MdnsListener() { stop(); }

std::optional<ErrorCode> MdnsListener::start(const std::string&) noexcept {
    return ErrorCode::DiscMdnsUnavailable;
}

std::optional<ErrorCode> MdnsListener::stop() noexcept {
    running_ = false;
    return std::nullopt;
}

bool MdnsListener::isRunning() const noexcept {
    return running_;
}

std::optional<ErrorCode> MdnsListener::poll(std::chrono::milliseconds) noexcept {
    return std::nullopt;
}

std::vector<std::pair<std::string, std::string>> MdnsListener::parseTxt(const u8*, u16) noexcept {
    return {};
}

}

#endif
