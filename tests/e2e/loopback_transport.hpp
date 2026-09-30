#pragma once

#include <thread>
#include <atomic>
#include <chrono>

#include "s05_transport/transport_impl.hpp"

namespace cfx {

struct LoopbackTransportPair {
    TransportImpl server;
    TransportImpl client;
    std::thread serverThread;
    std::atomic<bool> serverReady{false};
    std::atomic<bool> connected{false};

    LoopbackTransportPair(uint16_t inputPort, uint16_t controlPort)
        : server(makeServerConfig(inputPort, controlPort))
        , client(makeClientConfig(inputPort, controlPort))
    {
        serverThread = std::thread([this]() {
            serverReady.store(true);
            server.connect();
            connected.store(true);
        });

        while (!serverReady.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        client.connect();

        for (int i = 0; i < 1000; ++i) {
            if (client.isConnected() && connected.load()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    ~LoopbackTransportPair() {
        client.disconnect();
        server.disconnect();
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }

    LoopbackTransportPair(const LoopbackTransportPair&) = delete;
    LoopbackTransportPair& operator=(const LoopbackTransportPair&) = delete;

private:
    static TransportConfig makeServerConfig(uint16_t inputPort, uint16_t controlPort) {
        TransportConfig cfg{};
        cfg.isServer = true;
        cfg.inputPlanePort = inputPort;
        cfg.controlPlanePort = controlPort;
        return cfg;
    }
    static TransportConfig makeClientConfig(uint16_t inputPort, uint16_t controlPort) {
        TransportConfig cfg{};
        cfg.remoteHost = "127.0.0.1";
        cfg.inputPlanePort = inputPort;
        cfg.controlPlanePort = controlPort;
        return cfg;
    }
};

}  // namespace cfx::test