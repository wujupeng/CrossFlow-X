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

    std::string serviceName = "CrossFlow-X-PHY-Test";
    u16 port = 5353;
    std::string topologyId = "phy-test-topo";
    std::string nodeIdHigh = "1";
    std::string nodeIdLow = "100";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--service") == 0 && i + 1 < argc) serviceName = argv[++i];
        else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = static_cast<u16>(std::stoi(argv[++i]));
        else if (std::strcmp(argv[i], "--topology") == 0 && i + 1 < argc) topologyId = argv[++i];
        else if (std::strcmp(argv[i], "--node-high") == 0 && i + 1 < argc) nodeIdHigh = argv[++i];
        else if (std::strcmp(argv[i], "--node-low") == 0 && i + 1 < argc) nodeIdLow = argv[++i];
    }

    std::printf("=== CrossFlow-X Physical mDNS Announcer ===\n");
    std::printf("Service: %s\n", serviceName.c_str());
    std::printf("Port: %u\n", port);
    std::printf("Topology: %s\n", topologyId.c_str());
    std::printf("NodeID: %s:%s\n", nodeIdHigh.c_str(), nodeIdLow.c_str());
    std::printf("Platform: %s\n",
#ifdef _WIN32
        "Windows"
#elif defined(__APPLE__)
        "macOS"
#else
        "Unknown"
#endif
    );
    std::printf("Timestamp: %lld\n", static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count()));

    MdnsAnnouncer announcer;

    std::vector<std::pair<std::string, std::string>> txt;
    txt.push_back({"node-h", nodeIdHigh});
    txt.push_back({"node-l", nodeIdLow});
    txt.push_back({"topo", topologyId});
    txt.push_back({"proto", "1"});
    txt.push_back({"epoch", "1"});

    auto announceStart = std::chrono::steady_clock::now();
    auto err = announcer.start(serviceName, port, txt);
    auto announceEnd = std::chrono::steady_clock::now();

    if (err) {
        std::printf("FAILED: announcer.start() error\n");
        return 1;
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(announceEnd - announceStart);
    std::printf("Announce duration: %lld ms\n", static_cast<long long>(duration.count()));
    std::printf("Status: ANNOUNCING\n");
    std::printf("Waiting for discovery... (press Ctrl+C to stop)\n");

    std::this_thread::sleep_for(std::chrono::seconds(10));

    announcer.stop();
    std::printf("Stopped.\n");
    return 0;
}