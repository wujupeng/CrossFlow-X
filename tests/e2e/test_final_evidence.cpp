#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "integration/final_evidence_matrix_generator.hpp"
#include "integration/frozen_red_line_checker.hpp"
#include "integration/e2e_latency_probe.hpp"

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED: %s:%d: %s\n", __FILE__, __LINE__, #cond); std::exit(1); } } while(0)

namespace cfx {

static void test_evidence_matrix_basic() {
    printf("[TEST] test_evidence_matrix_basic\n");

    FinalEvidenceMatrixGenerator generator;

    generator.addScenarioResult("E2E-01", EvidenceCategory::Capture, PlatformConfig::MacRelease, true, "evidence/e2e_01_mac_capture.json");
    generator.addScenarioResult("E2E-01", EvidenceCategory::ReceiveInjection, PlatformConfig::WinRelease, true, "evidence/e2e_01_win_injection.json");
    generator.addScenarioResult("E2E-01", EvidenceCategory::Correlation, PlatformConfig::MacRelease, true, "evidence/e2e_01_correlation.json");

    auto matrix = generator.generate();
    CHECK(matrix.totalScenarios == 1);
    CHECK(matrix.totalPassCells == 3);
    CHECK(matrix.totalFailCells == 0);
    CHECK(matrix.allPass);
}

static void test_evidence_matrix_with_failure() {
    printf("[TEST] test_evidence_matrix_with_failure\n");

    FinalEvidenceMatrixGenerator generator;

    generator.addScenarioResult("E2E-02", EvidenceCategory::Capture, PlatformConfig::MacRelease, true, "ref1");
    generator.addScenarioResult("E2E-02", EvidenceCategory::Correlation, PlatformConfig::WinRelease, false, "", "payload mismatch");

    auto matrix = generator.generate();
    CHECK(matrix.totalScenarios == 1);
    CHECK(matrix.totalPassCells == 1);
    CHECK(matrix.totalFailCells == 1);
    CHECK(!matrix.allPass);
}

static void test_evidence_matrix_full_coverage() {
    printf("[TEST] test_evidence_matrix_full_coverage\n");

    FinalEvidenceMatrixGenerator generator;

    const char* scenarios[] = {"E2E-01", "E2E-02", "E2E-03", "E2E-04", "E2E-05", "E2E-06"};
    EvidenceCategory cats[] = {EvidenceCategory::Capture, EvidenceCategory::ReceiveInjection, EvidenceCategory::Correlation, EvidenceCategory::DisconnectReconnect};
    PlatformConfig cfgs[] = {PlatformConfig::MacDebug, PlatformConfig::MacRelease, PlatformConfig::WinDebug, PlatformConfig::WinRelease};

    for (auto s : scenarios) {
        for (auto c : cats) {
            for (auto cfg : cfgs) {
                generator.addScenarioResult(s, c, cfg, true, "evidence_ref");
            }
        }
    }

    auto matrix = generator.generate();
    CHECK(matrix.totalScenarios == 6);
    CHECK(matrix.totalPassCells == 96);
    CHECK(matrix.totalFailCells == 0);
    CHECK(matrix.allPass);
}

static void test_evidence_matrix_text_report() {
    printf("[TEST] test_evidence_matrix_text_report\n");

    FinalEvidenceMatrixGenerator generator;
    generator.addScenarioResult("E2E-01", EvidenceCategory::Capture, PlatformConfig::MacRelease, true, "ref1");

    auto report = generator.generateTextReport();
    CHECK(report.find("Final Evidence Matrix") != std::string::npos);
    CHECK(report.find("E2E-01") != std::string::npos);
    CHECK(report.find("PASS") != std::string::npos);
}

static void test_evidence_matrix_with_latency() {
    printf("[TEST] test_evidence_matrix_with_latency\n");

    E2ELatencyProbe probe;
    for (u64 i = 1; i <= 5; ++i) {
        probe.recordCapture(i, i * 1000);
        probe.recordQueueProcessing(i, i * 1000 + 50);
        probe.recordTransportReceive(i, i * 1000 + 100);
        probe.recordInjection(i, i * 1000 + 150);
    }

    FinalEvidenceMatrixGenerator generator;
    generator.setLatencyReport(probe.generateReport());
    generator.addScenarioResult("E2E-01", EvidenceCategory::Capture, PlatformConfig::MacRelease, true, "ref1");

    auto report = generator.generateTextReport();
    CHECK(report.find("Latency Report") != std::string::npos);
    CHECK(report.find("Capture->Queue") != std::string::npos);
}

static void test_red_line_checker_all_complied() {
    printf("[TEST] test_red_line_checker_all_complied\n");

    FrozenRedLineChecker checker;

    checker.checkR31_01_noFrozenFileModified({"integration/test.cpp"}, {"platform/mac/mac_event_tap.hpp"});
    checker.checkR31_02_noFrozenInterfaceChanged(true);
    checker.checkR31_03_noTestModifiedForPass(true);
    checker.checkR31_04_techDebtPreserved(true);
    checker.checkR31_05_realPhysicalEvidence(true);
    checker.checkR31_06_noUnitTestAsE2E(true);
    checker.checkR31_07_noTccBypass(true);
    checker.checkR31_08_noRootSshImpersonation(true);
    checker.checkR31_09_noUnrelatedFeatures(true);
    checker.checkR31_10_noTask032Started(true);

    CHECK(checker.results().size() == 10);
    CHECK(checker.allRedLinesComplied());
}

static void test_red_line_checker_with_violation() {
    printf("[TEST] test_red_line_checker_with_violation\n");

    FrozenRedLineChecker checker;

    checker.checkR31_01_noFrozenFileModified(
        {"platform/mac/mac_event_tap.hpp"},
        {"platform/mac/mac_event_tap.hpp"}
    );

    CHECK(!checker.allRedLinesComplied());
    CHECK(checker.results()[0].id == "R31-01");
    CHECK(!checker.results()[0].complied);
}

static void test_red_line_checker_report() {
    printf("[TEST] test_red_line_checker_report\n");

    FrozenRedLineChecker checker;
    checker.checkR31_01_noFrozenFileModified({}, {"platform/mac/mac_event_tap.hpp"});
    checker.checkR31_10_noTask032Started(true);

    auto report = checker.generateReport();
    CHECK(report.find("Frozen Red Line Check Report") != std::string::npos);
    CHECK(report.find("R31-01") != std::string::npos);
    CHECK(report.find("R31-10") != std::string::npos);
    CHECK(report.find("COMPLIED") != std::string::npos);
}

}  // namespace cfx

int main() {
    printf("=== test_final_evidence ===\n");

    cfx::test_evidence_matrix_basic();
    cfx::test_evidence_matrix_with_failure();
    cfx::test_evidence_matrix_full_coverage();
    cfx::test_evidence_matrix_text_report();
    cfx::test_evidence_matrix_with_latency();
    cfx::test_red_line_checker_all_complied();
    cfx::test_red_line_checker_with_violation();
    cfx::test_red_line_checker_report();

    printf("=== ALL PASS ===\n");
    return 0;
}