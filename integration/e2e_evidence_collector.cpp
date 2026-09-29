#include "integration/e2e_evidence_collector.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

namespace cfx {

void E2EEvidenceCollector::addRecord(PhysicalEvidenceRecord record) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.push_back(std::move(record));
}

void E2EEvidenceCollector::recordCaptureEvidence(u64 eventId, u64 timestampUs, const std::string& payload) noexcept {
    PhysicalEvidenceRecord rec{};
    rec.type = EvidenceType::Capture;
    rec.evidenceSource = "physical";
    rec.timestampUs = timestampUs;
    rec.eventId = eventId;
    rec.payload = payload;
    addRecord(std::move(rec));
}

void E2EEvidenceCollector::recordReceiveEvidence(u64 eventId, u64 timestampUs, const std::string& payload) noexcept {
    PhysicalEvidenceRecord rec{};
    rec.type = EvidenceType::Receive;
    rec.evidenceSource = "physical";
    rec.timestampUs = timestampUs;
    rec.eventId = eventId;
    rec.payload = payload;
    addRecord(std::move(rec));
}

void E2EEvidenceCollector::recordInjectionEvidence(u64 eventId, u64 timestampUs, const std::string& payload) noexcept {
    PhysicalEvidenceRecord rec{};
    rec.type = EvidenceType::Injection;
    rec.evidenceSource = "physical";
    rec.timestampUs = timestampUs;
    rec.eventId = eventId;
    rec.payload = payload;
    addRecord(std::move(rec));
}

void E2EEvidenceCollector::recordDisconnectEvidence(u64 timestampUs, const std::string& description) noexcept {
    PhysicalEvidenceRecord rec{};
    rec.type = EvidenceType::Disconnect;
    rec.evidenceSource = "physical";
    rec.timestampUs = timestampUs;
    rec.eventId = 0;
    rec.payload = description;
    addRecord(std::move(rec));
}

void E2EEvidenceCollector::recordReconnectEvidence(u64 timestampUs, const std::string& description) noexcept {
    PhysicalEvidenceRecord rec{};
    rec.type = EvidenceType::Reconnect;
    rec.evidenceSource = "physical";
    rec.timestampUs = timestampUs;
    rec.eventId = 0;
    rec.payload = description;
    addRecord(std::move(rec));
}

CorrelationResult E2EEvidenceCollector::verifyCorrelation() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    CorrelationResult result{};

    std::map<u64, const PhysicalEvidenceRecord*> captureMap;
    std::map<u64, const PhysicalEvidenceRecord*> injectionMap;

    for (const auto& rec : records_) {
        if (rec.type == EvidenceType::Capture) {
            captureMap[rec.eventId] = &rec;
        } else if (rec.type == EvidenceType::Injection) {
            injectionMap[rec.eventId] = &rec;
        }
    }

    std::set<u64> captureEventIds;
    std::set<u64> injectionEventIds;

    for (const auto& [id, _] : captureMap) captureEventIds.insert(id);
    for (const auto& [id, _] : injectionMap) injectionEventIds.insert(id);

    for (const auto& [eventId, capRec] : captureMap) {
        auto it = injectionMap.find(eventId);
        if (it == injectionMap.end()) {
            result.unmatchedCaptureCount++;
            continue;
        }

        const auto* injRec = it->second;
        result.matchedCount++;

        if (capRec->payload != injRec->payload) {
            result.payloadMismatchCount++;
        }

        if (injRec->timestampUs <= capRec->timestampUs) {
            result.timestampNonMonotonicCount++;
        }
    }

    for (const auto& [eventId, _] : injectionMap) {
        if (captureMap.find(eventId) == captureMap.end()) {
            result.unmatchedInjectionCount++;
        }
    }

    if (!captureEventIds.empty() && !injectionEventIds.empty()) {
        u64 maxCaptureId = *captureEventIds.rbegin();
        u64 maxInjectionId = *injectionEventIds.rbegin();
        u64 minId = std::min(*captureEventIds.begin(), *injectionEventIds.begin());
        u64 maxId = std::max(maxCaptureId, maxInjectionId);
        for (u64 id = minId; id <= maxId; ++id) {
            if (captureEventIds.find(id) == captureEventIds.end() &&
                injectionEventIds.find(id) == injectionEventIds.end()) {
                result.missingEventIdCount++;
            }
        }
    }

    result.pass = (result.unmatchedCaptureCount == 0 &&
                   result.unmatchedInjectionCount == 0 &&
                   result.payloadMismatchCount == 0 &&
                   result.timestampNonMonotonicCount == 0 &&
                   result.missingEventIdCount == 0);

    if (!result.pass) {
        if (result.payloadMismatchCount > 0) {
            result.failureReason = "payload mismatch detected";
        } else if (result.timestampNonMonotonicCount > 0) {
            result.failureReason = "timestamp non-monotonic";
        } else if (result.unmatchedCaptureCount > 0 || result.unmatchedInjectionCount > 0) {
            result.failureReason = "unmatched evidence records";
        } else if (result.missingEventIdCount > 0) {
            result.failureReason = "missing eventId in sequence";
        }
    }

    return result;
}

AuthenticityCheckResult E2EEvidenceCollector::checkAuthenticity(bool tccGranted, bool isPhysicalGuiSession, bool isUnitTest) const noexcept {
    AuthenticityCheckResult result{};
    result.tccGranted = tccGranted;
    result.isPhysicalGuiSession = isPhysicalGuiSession;
    result.isRealPhysicalCapture = !isUnitTest;

    if (!tccGranted) {
        result.failureReason = "TCC not granted";
        return result;
    }

    if (!isPhysicalGuiSession) {
        result.failureReason = "not physical GUI session (root/SSH)";
        return result;
    }

    if (isUnitTest) {
        result.failureReason = "Unit Test source, not real physical capture";
        return result;
    }

    result.pass = true;
    return result;
}

std::vector<PhysicalEvidenceRecord> E2EEvidenceCollector::allRecords() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_;
}

u64 E2EEvidenceCollector::recordCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return records_.size();
}

void E2EEvidenceCollector::clear() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.clear();
}

}  // namespace cfx