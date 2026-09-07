#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "common/error_code.hpp"
#include "common/logger.hpp"

namespace {

struct CapturedLines {
    std::mutex mutex;
    std::vector<std::string> lines;
};

bool containsKey(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    return json.find(needle) != std::string::npos;
}

bool isValidJsonLine(const std::string& line) {
    if (line.size() < 2) return false;
    if (line.front() != '{') return false;
    if (line.back() != '}') return false;
    return true;
}

int testBasicLogging() {
    CapturedLines captured;
    cfx::Logger::instance().start([&](const std::string& line) {
        std::lock_guard<std::mutex> lock(captured.mutex);
        captured.lines.push_back(line);
    });

    CFX_LOG.info("hello world");
    CFX_LOG.warn("warning text");
    CFX_LOG.error("error occurred");

    cfx::Logger::instance().stop();

    std::lock_guard<std::mutex> lock(captured.mutex);
    if (captured.lines.size() != 3) return 1;
    for (const auto& line : captured.lines) {
        if (!isValidJsonLine(line)) return 1;
        if (!containsKey(line, "ts")) return 1;
        if (!containsKey(line, "level")) return 1;
        if (!containsKey(line, "msg")) return 1;
    }
    if (!containsKey(captured.lines[0], "\"msg\":\"hello world\"")) {
        if (captured.lines[0].find("hello world") == std::string::npos) return 1;
    }
    return 0;
}

int testFieldInjection() {
    CapturedLines captured;
    cfx::Logger::instance().start([&](const std::string& line) {
        std::lock_guard<std::mutex> lock(captured.mutex);
        captured.lines.push_back(line);
    });

    CFX_LOG
        .nodeId("a1b2c3d4-e5f6-7890-abcd-ef1234567890")
        .traceId(12345)
        .eventId(67890)
        .linkState("up")
        .code(cfx::ErrorCode::HandoffTimeout)
        .info("handoff initiated");

    cfx::Logger::instance().stop();

    std::lock_guard<std::mutex> lock(captured.mutex);
    if (captured.lines.size() != 1) return 1;
    const auto& line = captured.lines[0];
    if (!isValidJsonLine(line)) return 1;
    if (!containsKey(line, "nodeId")) return 1;
    if (!containsKey(line, "traceId")) return 1;
    if (!containsKey(line, "eventId")) return 1;
    if (!containsKey(line, "linkState")) return 1;
    if (!containsKey(line, "code")) return 1;
    if (line.find("CFX-E-HANDOFF-TIMEOUT") == std::string::npos) return 1;
    return 0;
}

int testHotPathLatency() {
    CapturedLines captured;
    cfx::Logger::instance().start([&](const std::string& line) {
        std::lock_guard<std::mutex> lock(captured.mutex);
        captured.lines.push_back(line);
    });

    constexpr int kIterations = 10000;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        CFX_LOG.eventId(static_cast<uint64_t>(i)).info("hot path test");
    }
    const auto end = std::chrono::steady_clock::now();
    cfx::Logger::instance().stop();

    const auto totalUs = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
    const auto avgUs = totalUs / kIterations;
    if (avgUs > 50.0) return 1;

    std::lock_guard<std::mutex> lock(captured.mutex);
    if (captured.lines.empty()) return 1;
    return 0;
}

int testEscaping() {
    CapturedLines captured;
    cfx::Logger::instance().start([&](const std::string& line) {
        std::lock_guard<std::mutex> lock(captured.mutex);
        captured.lines.push_back(line);
    });

    CFX_LOG.info("text with \"quotes\" and \\backslash and \n newline");

    cfx::Logger::instance().stop();

    std::lock_guard<std::mutex> lock(captured.mutex);
    if (captured.lines.size() != 1) return 1;
    if (!isValidJsonLine(captured.lines[0])) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testBasicLogging()) return 1;
    if (testFieldInjection()) return 1;
    if (testHotPathLatency()) return 1;
    if (testEscaping()) return 1;
    return 0;
}