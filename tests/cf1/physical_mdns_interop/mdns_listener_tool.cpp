#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "common/domain.hpp"
#include "common/error_code.hpp"
#include "s07_discovery/mdns_announcer.hpp"
#include "s07_discovery/mdns_listener.hpp"
#include "s07_discovery/discovery_table.hpp"

int main(int argc, char* argv[]) {
    using namespace cfx;

    int timeoutSeconds = 5;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) timeoutSeconds = std::stoi(argv[++i]);
    }

    std::printf("=== CrossFlow-X Physical mDNS Listener ===\n");
    std::printf("Service type: _crossflow-x._tcp\n");
    std::printf("Timeout: %d seconds\n", timeoutSeconds);
    std::printf("Platform: %s\n",
#ifdef _WIN32
        "Windows"
#elif defined(__APPLE__)
        "macOS"
#else
        "Unknown"
#endif
    );

    auto listenStart = std::chrono::steady_clock::now();

    MdnsListener listener;
    DiscoveryTable table;

    std::atomic<bool> discovered{false};
    std::atomic<int> discoveryCount{0};

    listener.setDiscoveryCallback([&](const std::string& serviceName,
                                       const std::string& host,
                                       u16 port,
                                       const std::vector<std::pair<std::string, std::string>>& txtRecord) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - listenStart);
        int count = discoveryCount.fetch_add(1) + 1;

        std::printf("\n--- Discovery #%d (latency: %lld ms) ---\n", count, static_cast<long long>(elapsed.count()));
        std::printf("Service: %s\n", serviceName.c_str());
        std::printf("Host: %s\n", host.c_str());
        std::printf("Port: %u\n", port);
        std::printf("TXT Record:\n");
        for (const auto& [k, v] : txtRecord) {
            std::printf("  %s = %s\n", k.c_str(), v.c_str());
        }

        DiscoveryRecord record{};
        record.nodeId.high = 0;
        record.nodeId.low = 0;
        for (const auto& [k, v] : txtRecord) {
            if (k == "node-h") record.nodeId.high = std::stoull(v);
            else if (k == "node-l") record.nodeId.low = std::stoull(v);
            else if (k == "topo") record.topologyId = v;
        }
        record.platform =
#ifdef _WIN32
            Platform::Win
#elif defined(__APPLE__)
            Platform::Mac
#else
            Platform::Win
#endif
        ;
        record.sessionEpoch.value = 1;
        record.protocolVersion = 1;

        auto [upsertErr, result] = table.upsertWithResult(record);
        std::printf("DiscoveryTable upsert result: %d\n", static_cast<int>(result));
        std::printf("Table size: %u\n", table.size());
        std::printf("Duplicates: %u / Stale: %u / Conflicts: %u\n",
            table.duplicateCount(), table.staleCount(), table.conflictCount());

        discovered.store(true);
    });

    auto err = listener.start("_crossflow-x._tcp");
    if (err) {
        std::printf("FAILED: listener.start() error\n");
        return 1;
    }

    std::printf("Listening for mDNS services...\n");

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    while (std::chrono::steady_clock::now() < deadline) {
        listener.poll(std::chrono::milliseconds(100));
    }

    listener.stop();

    auto listenEnd = std::chrono::steady_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(listenEnd - listenStart);

    std::printf("\n=== Summary ===\n");
    std::printf("Total time: %lld ms\n", static_cast<long long>(totalDuration.count()));
    std::printf("Discoveries: %d\n", discoveryCount.load());
    std::printf("Table size: %u\n", table.size());
    std::printf("Duplicates: %u\n", table.duplicateCount());
    std::printf("Stale: %u\n", table.staleCount());
    std::printf("Conflicts: %u\n", table.conflictCount());
    std::printf("Discovered: %s\n", discovered.load() ? "YES" : "NO");

    if (discovered.load()) {
        std::printf("RESULT: SUCCESS\n");
        return 0;
    } else {
        std::printf("RESULT: TIMEOUT (no discovery within %d seconds)\n", timeoutSeconds);
        return 2;
    }
}