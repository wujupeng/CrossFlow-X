#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>

#include "common/domain.hpp"
#include "s05_transport/transport_impl.hpp"

using namespace cfx;

static void printUsage() {
    fprintf(stderr, "Usage: cross_end_test <mode> [host]\n");
    fprintf(stderr, "  mode: server | client\n");
    fprintf(stderr, "  host: remote host IP (client mode only, default 127.0.0.1)\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    bool isServer = (std::strcmp(argv[1], "server") == 0);
    bool isClient = (std::strcmp(argv[1], "client") == 0);

    if (!isServer && !isClient) {
        printUsage();
        return 1;
    }

    const char* remoteHost = (argc >= 3) ? argv[2] : "127.0.0.1";
    uint16_t inputPort = 11301;
    uint16_t controlPort = 11302;

    TransportConfig config{};
    config.remoteHost = remoteHost;
    config.inputPlanePort = inputPort;
    config.controlPlanePort = controlPort;
    config.isServer = isServer;

    TransportImpl transport(config);

    std::atomic<int> receivedCount{0};
    std::atomic<uint64_t> firstReceivedTs{0};

    transport.onEvent([&](const CanonicalInputEvent& event) {
        int count = receivedCount.fetch_add(1) + 1;
        if (count == 1) {
            firstReceivedTs.store(static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count()));
        }
        int dx = 0, dy = 0;
        if (auto* p = std::get_if<MouseMovePayload>(&event.payload)) {
            dx = p->deltaX; dy = p->deltaY;
        }
        fprintf(stderr, "[RECV] event #%d: type=%u dx=%d dy=%d\n",
                count, static_cast<unsigned>(event.eventType), dx, dy);
    });

    fprintf(stderr, "[INFO] %s mode, connecting to %s:%u/%u\n",
            isServer ? "server" : "client", remoteHost, inputPort, controlPort);

    if (!transport.connect()) {
        fprintf(stderr, "[FAIL] connect failed\n");
        return 2;
    }

    fprintf(stderr, "[OK] connected, isConnected=%d\n", transport.isConnected());

    if (isServer) {
        fprintf(stderr, "[INFO] server waiting for events (timeout 15s)...\n");
        for (int i = 0; i < 1500 && receivedCount.load() < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        int count = receivedCount.load();
        fprintf(stderr, "[RESULT] server received %d events\n", count);
        if (count >= 10) {
            fprintf(stderr, "[PASS] received all 10 events\n");
            transport.disconnect();
            return 0;
        } else {
            fprintf(stderr, "[FAIL] expected 10, got %d\n", count);
            transport.disconnect();
            return 3;
        }
    } else {
        fprintf(stderr, "[INFO] client sending 10 events...\n");
        for (int i = 1; i <= 10; ++i) {
            CanonicalInputEvent event{};
            event.eventId = i;
            event.eventType = EventType::MouseMove;
            event.payload = MouseMovePayload{i * 10, 0};
            event.modifierState = {false, false, false, false, false};
            transport.send(event);
            fprintf(stderr, "[SEND] event #%d: dx=%d\n", i, i * 10);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        fprintf(stderr, "[RESULT] client sent %llu, server received %d\n",
                static_cast<unsigned long long>(transport.totalSent()),
                receivedCount.load());
        fprintf(stderr, "[PASS] client completed\n");
        transport.disconnect();
        return 0;
    }
}