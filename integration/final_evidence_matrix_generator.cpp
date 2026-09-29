#include "integration/final_evidence_matrix_generator.hpp"

#include <cstdio>
#include <set>

namespace cfx {

void FinalEvidenceMatrixGenerator::addScenarioResult(
    const std::string& scenario,
    EvidenceCategory cat,
    PlatformConfig cfg,
    bool pass,
    const std::string& evidenceRef,
    const std::string& failureReason) noexcept {
    MatrixCell cell{};
    cell.scenario = scenario;
    cell.evidenceCategory = cat;
    cell.platformConfig = cfg;
    cell.pass = pass;
    cell.evidenceRef = evidenceRef;
    cell.failureReason = failureReason;
    cells_.push_back(std::move(cell));
}

FinalEvidenceMatrix FinalEvidenceMatrixGenerator::generate() const noexcept {
    FinalEvidenceMatrix matrix{};
    matrix.cells = cells_;
    matrix.latencyReport = latencyReport_;

    std::set<std::string> scenarios;
    for (const auto& cell : cells_) {
        scenarios.insert(cell.scenario);
        if (cell.pass) {
            matrix.totalPassCells++;
        } else {
            matrix.totalFailCells++;
        }
    }
    matrix.totalScenarios = scenarios.size();
    matrix.allPass = (matrix.totalFailCells == 0 && matrix.totalPassCells > 0);

    return matrix;
}

static const char* evidenceCategoryName(EvidenceCategory cat) noexcept {
    switch (cat) {
        case EvidenceCategory::Capture: return "Capture";
        case EvidenceCategory::ReceiveInjection: return "Receive/Injection";
        case EvidenceCategory::Correlation: return "Correlation";
        case EvidenceCategory::DisconnectReconnect: return "Disconnect/Reconnect";
        default: return "Unknown";
    }
}

static const char* platformConfigName(PlatformConfig cfg) noexcept {
    switch (cfg) {
        case PlatformConfig::MacDebug: return "macOS-Debug";
        case PlatformConfig::MacRelease: return "macOS-Release";
        case PlatformConfig::WinDebug: return "Windows-Debug";
        case PlatformConfig::WinRelease: return "Windows-Release";
        default: return "Unknown";
    }
}

std::string FinalEvidenceMatrixGenerator::generateTextReport() const noexcept {
    std::string report;
    report += "=== Final Evidence Matrix ===\n\n";

    report += "Scenario | Evidence | Platform | Verdict | Evidence Ref\n";
    report += "---------|----------|----------|---------|------------\n";

    for (const auto& cell : cells_) {
        char line[512];
        std::snprintf(line, sizeof(line), "%s | %s | %s | %s | %s\n",
                      cell.scenario.c_str(),
                      evidenceCategoryName(cell.evidenceCategory),
                      platformConfigName(cell.platformConfig),
                      cell.pass ? "PASS" : "FAIL",
                      cell.evidenceRef.c_str());
        report += line;
    }

    report += "\n=== Summary ===\n";
    auto matrix = generate();
    char summary[256];
    std::snprintf(summary, sizeof(summary),
                  "Total Scenarios: %llu\nPass Cells: %llu\nFail Cells: %llu\nOverall: %s\n",
                  static_cast<unsigned long long>(matrix.totalScenarios),
                  static_cast<unsigned long long>(matrix.totalPassCells),
                  static_cast<unsigned long long>(matrix.totalFailCells),
                  matrix.allPass ? "ALL PASS" : "HAS FAILURES");
    report += summary;

    report += "\n=== Latency Report ===\n";
    char lat[512];
    std::snprintf(lat, sizeof(lat),
                  "Capture->Queue: min=%llu p50=%llu p99=%llu max=%llu\n"
                  "Queue->Transport: min=%llu p50=%llu p99=%llu max=%llu\n"
                  "Receive->Injection: min=%llu p50=%llu p99=%llu max=%llu\n"
                  "Total E2E: min=%llu p50=%llu p99=%llu max=%llu\n"
                  "Complete Records: %llu / %llu\n",
                  static_cast<unsigned long long>(matrix.latencyReport.captureToQueue.min),
                  static_cast<unsigned long long>(matrix.latencyReport.captureToQueue.p50),
                  static_cast<unsigned long long>(matrix.latencyReport.captureToQueue.p99),
                  static_cast<unsigned long long>(matrix.latencyReport.captureToQueue.max),
                  static_cast<unsigned long long>(matrix.latencyReport.queueToTransport.min),
                  static_cast<unsigned long long>(matrix.latencyReport.queueToTransport.p50),
                  static_cast<unsigned long long>(matrix.latencyReport.queueToTransport.p99),
                  static_cast<unsigned long long>(matrix.latencyReport.queueToTransport.max),
                  static_cast<unsigned long long>(matrix.latencyReport.receiveToInjection.min),
                  static_cast<unsigned long long>(matrix.latencyReport.receiveToInjection.p50),
                  static_cast<unsigned long long>(matrix.latencyReport.receiveToInjection.p99),
                  static_cast<unsigned long long>(matrix.latencyReport.receiveToInjection.max),
                  static_cast<unsigned long long>(matrix.latencyReport.totalE2E.min),
                  static_cast<unsigned long long>(matrix.latencyReport.totalE2E.p50),
                  static_cast<unsigned long long>(matrix.latencyReport.totalE2E.p99),
                  static_cast<unsigned long long>(matrix.latencyReport.totalE2E.max),
                  static_cast<unsigned long long>(matrix.latencyReport.completeRecords),
                  static_cast<unsigned long long>(matrix.latencyReport.totalRecords));
    report += lat;

    return report;
}

}  // namespace cfx