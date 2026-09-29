#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "integration/e2e_latency_probe.hpp"

namespace cfx {

enum class EvidenceCategory : u8 {
    Capture = 0,
    ReceiveInjection = 1,
    Correlation = 2,
    DisconnectReconnect = 3,
};

enum class PlatformConfig : u8 {
    MacDebug = 0,
    MacRelease = 1,
    WinDebug = 2,
    WinRelease = 3,
};

struct MatrixCell {
    std::string scenario;
    EvidenceCategory evidenceCategory;
    PlatformConfig platformConfig;
    bool pass{false};
    std::string evidenceRef;
    std::string failureReason;
};

struct RedLineCheckResult {
    std::string redLineId;
    std::string description;
    bool complied{false};
    std::string evidence;
};

struct FinalEvidenceMatrix {
    std::vector<MatrixCell> cells;
    std::vector<RedLineCheckResult> redLineChecks;
    SegmentedLatencyReport latencyReport;
    u64 totalScenarios{0};
    u64 totalPassCells{0};
    u64 totalFailCells{0};
    bool allPass{false};
};

class FinalEvidenceMatrixGenerator {
public:
    FinalEvidenceMatrixGenerator() noexcept = default;
    ~FinalEvidenceMatrixGenerator() noexcept = default;

    void addScenarioResult(const std::string& scenario,
                           EvidenceCategory cat,
                           PlatformConfig cfg,
                           bool pass,
                           const std::string& evidenceRef,
                           const std::string& failureReason = "") noexcept;

    void setLatencyReport(const SegmentedLatencyReport& report) noexcept { latencyReport_ = report; }

    FinalEvidenceMatrix generate() const noexcept;
    std::string generateTextReport() const noexcept;

private:
    std::vector<MatrixCell> cells_;
    SegmentedLatencyReport latencyReport_{};
};

}  // namespace cfx