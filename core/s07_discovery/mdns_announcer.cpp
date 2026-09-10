#include "s07_discovery/mdns_announcer.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windns.h>
#pragma comment(lib, "dnsapi.lib")

namespace cfx {

namespace {

struct RegisterContext {
    HANDLE doneEvent{nullptr};
    DWORD status{ERROR_SUCCESS};
};

void WINAPI registerCompleteCallback(DWORD status, PVOID pContext, PDNS_SERVICE_INSTANCE) {
    auto* ctx = static_cast<RegisterContext*>(pContext);
    if (ctx) {
        ctx->status = status;
        if (ctx->doneEvent) SetEvent(ctx->doneEvent);
    }
}

}

MdnsAnnouncer::MdnsAnnouncer() = default;

MdnsAnnouncer::~MdnsAnnouncer() {
    stop();
}

std::optional<ErrorCode> MdnsAnnouncer::start(
    const std::string& serviceName,
    u16 port,
    const std::vector<std::pair<std::string, std::string>>& txtRecord) noexcept {

    if (running_) return std::nullopt;

    auto announceStart = std::chrono::steady_clock::now();

    serviceName_ = serviceName;
    port_ = port;
    currentTxt_ = txtRecord;

    auto* ctx = new RegisterContext{};
    ctx->doneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ctx->doneEvent) {
        delete ctx;
        return ErrorCode::DiscMdnsUnavailable;
    }
    doneEvent_ = ctx->doneEvent;
    registerContext_ = ctx;

    std::vector<std::wstring> wKeys, wValues;
    std::vector<PCWSTR> keyPtrs, valPtrs;
    for (const auto& [k, v] : txtRecord) {
        wKeys.emplace_back(k.begin(), k.end());
        wValues.emplace_back(v.begin(), v.end());
    }
    for (const auto& wk : wKeys) keyPtrs.push_back(wk.c_str());
    for (const auto& wv : wValues) valPtrs.push_back(wv.c_str());

    wchar_t hostName[256] = {0};
    DWORD hostLen = 256;
    GetComputerNameW(hostName, &hostLen);

    std::wstring wServiceName(serviceName.begin(), serviceName.end());

    PDNS_SERVICE_INSTANCE instance = DnsServiceConstructInstance(
        wServiceName.c_str(),
        hostName,
        nullptr, nullptr,
        port, 0, 0,
        static_cast<DWORD>(txtRecord.size()),
        keyPtrs.data(), valPtrs.data());

    if (!instance) {
        CloseHandle(ctx->doneEvent);
        delete ctx;
        registerContext_ = nullptr;
        doneEvent_ = nullptr;
        return ErrorCode::DiscMdnsUnavailable;
    }

    DNS_SERVICE_REGISTER_REQUEST request{};
    request.Version = DNS_QUERY_REQUEST_VERSION1;
    request.InterfaceIndex = 0;
    request.pServiceInstance = instance;
    request.pRegisterCompletionCallback = registerCompleteCallback;
    request.pQueryContext = ctx;
    request.hCredentials = nullptr;
    request.unicastEnabled = FALSE;

    auto* cancel = new DNS_SERVICE_CANCEL{};
    DWORD result = DnsServiceRegister(&request, cancel);
    if (result != ERROR_SUCCESS && result != DNS_REQUEST_PENDING) {
        DnsServiceFreeInstance(instance);
        CloseHandle(ctx->doneEvent);
        delete ctx;
        delete cancel;
        registerContext_ = nullptr;
        doneEvent_ = nullptr;
        return ErrorCode::DiscMdnsUnavailable;
    }

    DWORD waitResult = WaitForSingleObject(ctx->doneEvent, 5000);
    if (waitResult != WAIT_OBJECT_0 || ctx->status != ERROR_SUCCESS) {
        DnsServiceRegisterCancel(cancel);
        DnsServiceFreeInstance(instance);
        CloseHandle(ctx->doneEvent);
        delete ctx;
        delete cancel;
        registerContext_ = nullptr;
        doneEvent_ = nullptr;
        return ErrorCode::DiscMdnsUnavailable;
    }

    registerCancel_ = cancel;
    txtRecordData_ = instance;
    running_ = true;
    lastRefreshTime_ = std::chrono::steady_clock::now();
    lastAnnounceDuration_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - announceStart);
    return std::nullopt;
}

std::optional<ErrorCode> MdnsAnnouncer::stop() noexcept {
    if (!running_) return std::nullopt;

    stopAutoRefresh();
    unsubscribeFromNetworkChanges();
    unsubscribeFromTopologyChanges();

    if (registerCancel_) {
        DnsServiceRegisterCancel(static_cast<PDNS_SERVICE_CANCEL>(registerCancel_));
        delete static_cast<PDNS_SERVICE_CANCEL>(registerCancel_);
        registerCancel_ = nullptr;
    }

    if (txtRecordData_) {
        DnsServiceFreeInstance(static_cast<PDNS_SERVICE_INSTANCE>(txtRecordData_));
        txtRecordData_ = nullptr;
    }

    if (doneEvent_) {
        CloseHandle(static_cast<HANDLE>(doneEvent_));
        doneEvent_ = nullptr;
    }

    if (registerContext_) {
        delete static_cast<RegisterContext*>(registerContext_);
        registerContext_ = nullptr;
    }

    running_ = false;
    return std::nullopt;
}

std::optional<ErrorCode> MdnsAnnouncer::updateTxt(
    const std::vector<std::pair<std::string, std::string>>& txtRecord) noexcept {
    if (!running_) return ErrorCode::DiscAnnounceLost;
    auto savedService = serviceName_;
    auto savedPort = port_;
    stop();
    return start(savedService, savedPort, txtRecord);
}

std::optional<ErrorCode> MdnsAnnouncer::refresh() noexcept {
    if (!running_) return std::nullopt;
    auto savedService = serviceName_;
    auto savedPort = port_;
    auto savedTxt = currentTxt_;
    stop();
    auto err = start(savedService, savedPort, savedTxt);
    if (!err) {
        lastRefreshTime_ = std::chrono::steady_clock::now();
    }
    return err;
}

bool MdnsAnnouncer::isRunning() const noexcept {
    return running_;
}

std::vector<u8> MdnsAnnouncer::encodeTxt(
    const std::vector<std::pair<std::string, std::string>>& txt) const noexcept {
    std::vector<u8> data;
    for (const auto& [key, val] : txt) {
        std::string entry = key + "=" + val;
        if (entry.size() > 255) continue;
        data.push_back(static_cast<u8>(entry.size()));
        data.insert(data.end(), entry.begin(), entry.end());
    }
    if (data.empty()) data.push_back(0);
    return data;
}

std::optional<ErrorCode> MdnsAnnouncer::republish() noexcept {
    return refresh();
}

void MdnsAnnouncer::startAutoRefresh(std::chrono::seconds interval) noexcept {
    refreshEnabled_ = true;
    refreshThread_ = std::thread([this, interval]() {
        while (refreshEnabled_.load(std::memory_order_relaxed)) {
            for (int i = 0; i < 10 && refreshEnabled_.load(std::memory_order_relaxed); ++i) {
                std::this_thread::sleep_for(interval / 10);
            }
            if (!refreshEnabled_.load(std::memory_order_relaxed)) break;
            refresh();
        }
    });
}

void MdnsAnnouncer::stopAutoRefresh() noexcept {
    refreshEnabled_ = false;
    if (refreshThread_.joinable()) {
        refreshThread_.join();
    }
}

void MdnsAnnouncer::subscribeToNetworkChanges(INetworkChangeEventSource& source) noexcept {
    if (subscribedToNetwork_.load()) return;
    networkSource_ = &source;
    source.setCallback([this](const NetworkChangeEvent&) {
        if (running_ && subscribedToNetwork_.load()) {
            networkChangeRepublishCount_.fetch_add(1, std::memory_order_relaxed);
            republish();
        }
    });
    subscribedToNetwork_.store(true);
}

void MdnsAnnouncer::unsubscribeFromNetworkChanges() noexcept {
    if (!subscribedToNetwork_.load()) return;
    if (networkSource_) {
        networkSource_->setCallback(nullptr);
        networkSource_ = nullptr;
    }
    subscribedToNetwork_.store(false);
}

void MdnsAnnouncer::subscribeToTopologyChanges(ITopologyChangeEventSource& source) noexcept {
    if (subscribedToTopology_.load()) return;
    topologySource_ = &source;
    source.setCallback([this](const TopologyChangeEvent&) {
        if (running_ && subscribedToTopology_.load()) {
            topologyChangeRepublishCount_.fetch_add(1, std::memory_order_relaxed);
            republish();
        }
    });
    subscribedToTopology_.store(true);
}

void MdnsAnnouncer::unsubscribeFromTopologyChanges() noexcept {
    if (!subscribedToTopology_.load()) return;
    if (topologySource_) {
        topologySource_->setCallback(nullptr);
        topologySource_ = nullptr;
    }
    subscribedToTopology_.store(false);
}

}

#elif defined(__APPLE__)
#include <dns_sd.h>
#include <arpa/inet.h>

namespace cfx {

MdnsAnnouncer::MdnsAnnouncer() = default;

MdnsAnnouncer::~MdnsAnnouncer() {
    stop();
}

std::optional<ErrorCode> MdnsAnnouncer::start(
    const std::string& serviceName,
    u16 port,
    const std::vector<std::pair<std::string, std::string>>& txtRecord) noexcept {

    if (running_) return std::nullopt;

    auto announceStart = std::chrono::steady_clock::now();

    serviceName_ = serviceName;
    port_ = port;
    currentTxt_ = txtRecord;

    auto txtData = encodeTxt(txtRecord);

    DNSServiceRef ref = nullptr;
    DNSServiceErrorType err = DNSServiceRegister(
        &ref,
        0,
        0,
        serviceName.empty() ? nullptr : serviceName.c_str(),
        "_crossflow-x._tcp",
        nullptr,
        nullptr,
        htons(port),
        static_cast<u16>(txtData.size()),
        txtData.data(),
        nullptr,
        nullptr);

    if (err != kDNSServiceErr_NoError) {
        return ErrorCode::DiscMdnsUnavailable;
    }

    serviceRef_ = ref;
    running_ = true;
    lastRefreshTime_ = std::chrono::steady_clock::now();
    lastAnnounceDuration_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - announceStart);
    return std::nullopt;
}

std::optional<ErrorCode> MdnsAnnouncer::stop() noexcept {
    if (!running_) return std::nullopt;

    stopAutoRefresh();
    unsubscribeFromNetworkChanges();
    unsubscribeFromTopologyChanges();

    if (serviceRef_) {
        DNSServiceRefDeallocate(static_cast<DNSServiceRef>(serviceRef_));
        serviceRef_ = nullptr;
    }

    running_ = false;
    return std::nullopt;
}

std::optional<ErrorCode> MdnsAnnouncer::updateTxt(
    const std::vector<std::pair<std::string, std::string>>& txtRecord) noexcept {
    if (!running_) return ErrorCode::DiscAnnounceLost;
    auto savedService = serviceName_;
    auto savedPort = port_;
    stop();
    return start(savedService, savedPort, txtRecord);
}

std::optional<ErrorCode> MdnsAnnouncer::refresh() noexcept {
    if (!running_) return std::nullopt;
    auto savedService = serviceName_;
    auto savedPort = port_;
    auto savedTxt = currentTxt_;
    stop();
    auto err = start(savedService, savedPort, savedTxt);
    if (!err) {
        lastRefreshTime_ = std::chrono::steady_clock::now();
    }
    return err;
}

bool MdnsAnnouncer::isRunning() const noexcept {
    return running_;
}

std::vector<u8> MdnsAnnouncer::encodeTxt(
    const std::vector<std::pair<std::string, std::string>>& txt) const noexcept {
    std::vector<u8> data;
    for (const auto& [key, val] : txt) {
        std::string entry = key + "=" + val;
        if (entry.size() > 255) continue;
        data.push_back(static_cast<u8>(entry.size()));
        data.insert(data.end(), entry.begin(), entry.end());
    }
    if (data.empty()) data.push_back(0);
    return data;
}

std::optional<ErrorCode> MdnsAnnouncer::republish() noexcept {
    return refresh();
}

void MdnsAnnouncer::startAutoRefresh(std::chrono::seconds interval) noexcept {
    refreshEnabled_ = true;
    refreshThread_ = std::thread([this, interval]() {
        while (refreshEnabled_.load(std::memory_order_relaxed)) {
            for (int i = 0; i < 10 && refreshEnabled_.load(std::memory_order_relaxed); ++i) {
                std::this_thread::sleep_for(interval / 10);
            }
            if (!refreshEnabled_.load(std::memory_order_relaxed)) break;
            refresh();
        }
    });
}

void MdnsAnnouncer::stopAutoRefresh() noexcept {
    refreshEnabled_ = false;
    if (refreshThread_.joinable()) {
        refreshThread_.join();
    }
}

void MdnsAnnouncer::subscribeToNetworkChanges(INetworkChangeEventSource& source) noexcept {
    if (subscribedToNetwork_.load()) return;
    networkSource_ = &source;
    source.setCallback([this](const NetworkChangeEvent&) {
        if (running_ && subscribedToNetwork_.load()) {
            networkChangeRepublishCount_.fetch_add(1, std::memory_order_relaxed);
            republish();
        }
    });
    subscribedToNetwork_.store(true);
}

void MdnsAnnouncer::unsubscribeFromNetworkChanges() noexcept {
    if (!subscribedToNetwork_.load()) return;
    if (networkSource_) {
        networkSource_->setCallback(nullptr);
        networkSource_ = nullptr;
    }
    subscribedToNetwork_.store(false);
}

void MdnsAnnouncer::subscribeToTopologyChanges(ITopologyChangeEventSource& source) noexcept {
    if (subscribedToTopology_.load()) return;
    topologySource_ = &source;
    source.setCallback([this](const TopologyChangeEvent&) {
        if (running_ && subscribedToTopology_.load()) {
            topologyChangeRepublishCount_.fetch_add(1, std::memory_order_relaxed);
            republish();
        }
    });
    subscribedToTopology_.store(true);
}

void MdnsAnnouncer::unsubscribeFromTopologyChanges() noexcept {
    if (!subscribedToTopology_.load()) return;
    if (topologySource_) {
        topologySource_->setCallback(nullptr);
        topologySource_ = nullptr;
    }
    subscribedToTopology_.store(false);
}

}

#else

namespace cfx {

MdnsAnnouncer::MdnsAnnouncer() = default;
MdnsAnnouncer::~MdnsAnnouncer() { stop(); }

std::optional<ErrorCode> MdnsAnnouncer::start(
    const std::string&,
    u16,
    const std::vector<std::pair<std::string, std::string>>&) noexcept {
    return ErrorCode::DiscMdnsUnavailable;
}

std::optional<ErrorCode> MdnsAnnouncer::stop() noexcept {
    stopAutoRefresh();
    unsubscribeFromNetworkChanges();
    unsubscribeFromTopologyChanges();
    running_ = false;
    return std::nullopt;
}

std::optional<ErrorCode> MdnsAnnouncer::updateTxt(
    const std::vector<std::pair<std::string, std::string>>&) noexcept {
    return ErrorCode::DiscMdnsUnavailable;
}

std::optional<ErrorCode> MdnsAnnouncer::refresh() noexcept {
    return ErrorCode::DiscMdnsUnavailable;
}

bool MdnsAnnouncer::isRunning() const noexcept {
    return running_;
}

std::vector<u8> MdnsAnnouncer::encodeTxt(
    const std::vector<std::pair<std::string, std::string>>& txt) const noexcept {
    return {};
}

std::optional<ErrorCode> MdnsAnnouncer::republish() noexcept {
    return ErrorCode::DiscMdnsUnavailable;
}

void MdnsAnnouncer::startAutoRefresh(std::chrono::seconds) noexcept {}

void MdnsAnnouncer::stopAutoRefresh() noexcept {}

void MdnsAnnouncer::subscribeToNetworkChanges(INetworkChangeEventSource& source) noexcept {
    if (subscribedToNetwork_.load()) return;
    networkSource_ = &source;
    source.setCallback([this](const NetworkChangeEvent&) {
        if (running_ && subscribedToNetwork_.load()) {
            networkChangeRepublishCount_.fetch_add(1, std::memory_order_relaxed);
            republish();
        }
    });
    subscribedToNetwork_.store(true);
}

void MdnsAnnouncer::unsubscribeFromNetworkChanges() noexcept {
    if (!subscribedToNetwork_.load()) return;
    if (networkSource_) {
        networkSource_->setCallback(nullptr);
        networkSource_ = nullptr;
    }
    subscribedToNetwork_.store(false);
}

void MdnsAnnouncer::subscribeToTopologyChanges(ITopologyChangeEventSource& source) noexcept {
    if (subscribedToTopology_.load()) return;
    topologySource_ = &source;
    source.setCallback([this](const TopologyChangeEvent&) {
        if (running_ && subscribedToTopology_.load()) {
            topologyChangeRepublishCount_.fetch_add(1, std::memory_order_relaxed);
            republish();
        }
    });
    subscribedToTopology_.store(true);
}

void MdnsAnnouncer::unsubscribeFromTopologyChanges() noexcept {
    if (!subscribedToTopology_.load()) return;
    if (topologySource_) {
        topologySource_->setCallback(nullptr);
        topologySource_ = nullptr;
    }
    subscribedToTopology_.store(false);
}

}

#endif
