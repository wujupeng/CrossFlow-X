#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "common/domain.hpp"

namespace cfx {

enum class EvidenceType : u8 {
    Capture = 0,
    Receive = 1,
    Injection = 2,
    Correlation = 3,
    Disconnect = 4,
    Reconnect = 5,
};

struct PhysicalEvidenceRecord {
    EvidenceType type;
    std::string evidenceSource;
    u64 timestampUs{0};
    u64 eventId{0};
    std::string payload;
};

struct CorrelationResult {
    bool pass{false};
    u64 matchedCount{0};
    u64 unmatchedCaptureCount{0};
    u64 unmatchedInjectionCount{0};
    u64 payloadMismatchCount{0};
    u64 timestampNonMonotonicCount{0};
    u64 missingEventIdCount{0};
    std::string failureReason;
};

struct AuthenticityCheckResult {
    bool pass{false};
    bool tccGranted{false};
    bool isPhysicalGuiSession{false};
    bool isRealPhysicalCapture{false};
    std::string failureReason;
};

class E2EEvidenceCollector {
public:
    E2EEvidenceCollector() noexcept = default;
    ~E2EEvidenceCollector() noexcept = default;

    E2EEvidenceCollector(const E2EEvidenceCollector&) = delete;
    E2EEvidenceCollector& operator=(const E2EEvidenceCollector&) = delete;

    void recordCaptureEvidence(u64 eventId, u64 timestampUs, const std::string& payload) noexcept;
    void recordReceiveEvidence(u64 eventId, u64 timestampUs, const std::string& payload) noexcept;
    void recordInjectionEvidence(u64 eventId, u64 timestampUs, const std::string& payload) noexcept;
    void recordDisconnectEvidence(u64 timestampUs, const std::string& description) noexcept;
    void recordReconnectEvidence(u64 timestampUs, const std::string& description) noexcept;

    CorrelationResult verifyCorrelation() const noexcept;
    AuthenticityCheckResult checkAuthenticity(bool tccGranted, bool isPhysicalGuiSession, bool isUnitTest) const noexcept;

    std::vector<PhysicalEvidenceRecord> allRecords() const noexcept;
    u64 recordCount() const noexcept;
    void clear() noexcept;

private:
    void addRecord(PhysicalEvidenceRecord record) noexcept;

    mutable std::mutex mutex_;
    std::vector<PhysicalEvidenceRecord> records_;
};

}  // namespace cfx