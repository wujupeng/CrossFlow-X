#include "common/logger.hpp"

#include <chrono>
#include <cstdio>

namespace cfx {

namespace {

uint64_t currentMicros() noexcept {
    const auto now = std::chrono::system_clock::now();
    const auto dur = now.time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(dur).count());
}

std::string_view logLevelString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Info:  return "I";
        case LogLevel::Warn:  return "W";
        case LogLevel::Error: return "E";
    }
    return "I";
}

void escapeJsonString(std::string& out, std::string_view value) {
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<int>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
}

}  // namespace

LogEntry::LogEntry() : json_("{"), hasFields_(false) {}

void LogEntry::appendKey(std::string_view key) {
    if (hasFields_) json_ += ',';
    json_ += '"';
    json_ += key;
    json_ += "\":";
    hasFields_ = true;
}

void LogEntry::appendStringValue(std::string_view value) {
    json_ += '"';
    escapeJsonString(json_, value);
    json_ += '"';
}

void LogEntry::appendU64Value(uint64_t value) {
    char buf[24];
    const int n = std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(value));
    json_.append(buf, static_cast<std::size_t>(n));
}

LogEntry& LogEntry::nodeId(std::string_view value) {
    appendKey("nodeId");
    appendStringValue(value);
    return *this;
}

LogEntry& LogEntry::traceId(uint64_t value) {
    appendKey("traceId");
    appendU64Value(value);
    return *this;
}

LogEntry& LogEntry::eventId(uint64_t value) {
    appendKey("eventId");
    appendU64Value(value);
    return *this;
}

LogEntry& LogEntry::linkState(std::string_view value) {
    appendKey("linkState");
    appendStringValue(value);
    return *this;
}

LogEntry& LogEntry::code(ErrorCode value) {
    appendKey("code");
    appendStringValue(cfx::to_string(value));
    return *this;
}

LogEntry& LogEntry::extra(std::string_view key, std::string_view value) {
    appendKey(key);
    appendStringValue(value);
    return *this;
}

LogEntry& LogEntry::extra(std::string_view key, uint64_t value) {
    appendKey(key);
    appendU64Value(value);
    return *this;
}

void LogEntry::submit(LogLevel level, std::string_view msg) {
    appendKey("ts");
    appendU64Value(currentMicros());
    appendKey("level");
    appendStringValue(logLevelString(level));
    appendKey("msg");
    appendStringValue(msg);
    json_ += '}';
    Logger::instance().submit(std::move(json_));
}

void LogEntry::info(std::string_view msg)  { submit(LogLevel::Info, msg); }
void LogEntry::warn(std::string_view msg)  { submit(LogLevel::Warn, msg); }
void LogEntry::error(std::string_view msg) { submit(LogLevel::Error, msg); }

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() : running_(false), sink_(nullptr), dropped_(0) {}

Logger::~Logger() {
    stop();
}

void Logger::start(Sink sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return;
    if (sink) {
        sink_ = std::move(sink);
    } else {
        sink_ = [](const std::string& line) {
            std::fputs(line.c_str(), stderr);
            std::fputc('\n', stderr);
        };
    }
    running_ = true;
    worker_ = std::thread(&Logger::workerLoop, this);
}

void Logger::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        running_ = false;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    flush();
    std::lock_guard<std::mutex> lock(mutex_);
    sink_ = nullptr;
}

void Logger::submit(std::string jsonLine) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || queue_.size() >= kMaxQueue) {
        if (queue_.size() >= kMaxQueue) dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    queue_.push_back(std::move(jsonLine));
    cv_.notify_one();
}

void Logger::flush() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return queue_.empty() || !running_; });
}

std::size_t Logger::droppedCount() const noexcept {
    return dropped_.load(std::memory_order_relaxed);
}

void Logger::workerLoop() {
    while (true) {
        std::string line;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
            if (!running_ && queue_.empty()) return;
            if (!queue_.empty()) {
                line = std::move(queue_.front());
                queue_.pop_front();
            }
        }
        if (!line.empty() && sink_) {
            sink_(line);
        }
    }
}

}  // namespace cfx