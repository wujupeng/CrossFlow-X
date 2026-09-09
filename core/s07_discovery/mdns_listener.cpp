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

    std::string serviceName;
    std::string host;
    u16 port = 0;
    std::vector<std::pair<std::string, std::string>> txt;

    for (PDNS_RECORDA rec = reinterpret_cast<PDNS_RECORDA>(pRecord); rec; rec = rec->pNext) {
        if (rec->wType == DNS_TYPE_SRV) {
            if (rec->pName) serviceName = rec->pName;
            if (rec->Data.SRV.pNameTarget) host = rec->Data.SRV.pNameTarget;
            port = rec->Data.SRV.wPort;
        } else if (rec->wType == DNS_TYPE_TEXT) {
            if (rec->pName && serviceName.empty()) serviceName = rec->pName;
            auto& txtData = rec->Data.TXT;
            for (DWORD i = 0; i < txtData.dwStringCount; ++i) {
                std::string entry = txtData.pStringArray[i];
                auto eq = entry.find('=');
                if (eq != std::string::npos) {
                    txt.emplace_back(entry.substr(0, eq), entry.substr(eq + 1));
                }
            }
        }
    }

    if (ctx->callback) {
        ctx->callback(serviceName, host, port, txt);
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
    browseContext_ = ctx;

    DNS_SERVICE_BROWSE_REQUEST request{};
    request.Version = DNS_QUERY_REQUEST_VERSION1;
    request.InterfaceIndex = 0;
    request.QueryName = L"_crossflow-x._tcp.local";
    request.pBrowseCallback = browseCompleteCallback;
    request.pQueryContext = ctx;

    auto* cancel = new DNS_SERVICE_CANCEL{};
    DWORD result = DnsServiceBrowse(&request, cancel);
    if (result != ERROR_SUCCESS && result != DNS_REQUEST_PENDING) {
        delete ctx;
        delete cancel;
        browseContext_ = nullptr;
        return ErrorCode::DiscMdnsUnavailable;
    }

    browseCancel_ = cancel;
    running_ = true;
    return std::nullopt;
}

std::optional<ErrorCode> MdnsListener::stop() noexcept {
    if (!running_) return std::nullopt;

    if (browseCancel_) {
        DnsServiceBrowseCancel(static_cast<PDNS_SERVICE_CANCEL>(browseCancel_));
        delete static_cast<PDNS_SERVICE_CANCEL>(browseCancel_);
        browseCancel_ = nullptr;
    }

    if (browseContext_) {
        delete static_cast<BrowseContext*>(browseContext_);
        browseContext_ = nullptr;
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

struct ResolveContext {
    MdnsListener::DiscoveryCallback callback;
    std::string serviceName;
};

void DNSSD_API resolveCallback(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType errorCode,
                                const char*, const char* hosttarget, uint16_t port,
                                uint16_t txtLen, const unsigned char* txtRecord, void* context) {
    auto* ctx = static_cast<ResolveContext*>(context);
    if (!ctx || !ctx->callback || errorCode != kDNSServiceErr_NoError) return;

    auto txt = MdnsListener::parseTxt(txtRecord, txtLen);
    u16 networkPort = ntohs(port);
    ctx->callback(ctx->serviceName, hosttarget ? hosttarget : "", networkPort, txt);
}

void DNSSD_API browseCallback(DNSServiceRef, DNSServiceFlags, uint32_t interfaceIndex, DNSServiceErrorType errorCode,
                              const char* serviceName, const char* regtype,
                              const char* domain, void* context) {
    auto* ctx = static_cast<BrowseContext*>(context);
    if (!ctx || !ctx->callback || errorCode != kDNSServiceErr_NoError || !serviceName) return;

    auto* resolveCtx = new ResolveContext{};
    resolveCtx->callback = ctx->callback;
    resolveCtx->serviceName = serviceName;

    DNSServiceRef resolveRef = nullptr;
    DNSServiceErrorType err = DNSServiceResolve(
        &resolveRef, 0, interfaceIndex, serviceName, regtype, domain,
        resolveCallback, resolveCtx);

    if (err == kDNSServiceErr_NoError) {
        DNSServiceProcessResult(resolveRef);
        DNSServiceRefDeallocate(resolveRef);
    }
    delete resolveCtx;
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
    browseContext_ = ctx;

    DNSServiceRef ref = nullptr;
    DNSServiceErrorType err = DNSServiceBrowse(
        &ref, 0, 0, "_crossflow-x._tcp", "local.",
        browseCallback, ctx);

    if (err != kDNSServiceErr_NoError) {
        delete ctx;
        browseContext_ = nullptr;
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

    if (browseContext_) {
        delete static_cast<BrowseContext*>(browseContext_);
        browseContext_ = nullptr;
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
