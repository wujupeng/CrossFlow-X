#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include "common/error_code.hpp"

namespace cfx {

enum class LogLevel : uint8_t {
    Info,
    Warn,
    Error,
};

class Logger;

class LogEntry {
public:
    LogEntry();
    ~LogEntry() = default;

    LogEntry(const LogEntry&) = delete;
    LogEntry& operator=(const LogEntry&) = delete;
    LogEntry(LogEntry&&) = default;
    LogEntry& operator=(LogEntry&&) = default;

    LogEntry& nodeId(std::string_view value);
    LogEntry& traceId(uint64_t value);
    LogEntry& eventId(uint64_t value);
    LogEntry& linkState(std::string_view value);
    LogEntry& code(ErrorCode value);
    LogEntry& extra(std::string_view key, std::string_view value);
    LogEntry& extra(std::string_view key, uint64_t value);

    void info(std::string_view msg);
    void warn(std::string_view msg);
    void error(std::string_view msg);

private:
    void submit(LogLevel level, std::string_view msg);
    void appendKey(std::string_view key);
    void appendStringValue(std::string_view value);
    void appendU64Value(uint64_t value);

    std::string json_;
    bool hasFields_;
};

class Logger {
public:
    using Sink = std::function<void(const std::string&)>;

    static Logger& instance();

    void start(Sink sink = {});
    void stop();
    void submit(std::string jsonLine);
    void flush();
    std::size_t droppedCount() const noexcept;

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void workerLoop();

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    std::atomic<bool> running_;
    std::thread worker_;
    Sink sink_;
    std::atomic<std::size_t> dropped_;
    static constexpr std::size_t kMaxQueue = 4096;
};

}  // namespace cfx

#define CFX_LOG ::cfx::LogEntry()