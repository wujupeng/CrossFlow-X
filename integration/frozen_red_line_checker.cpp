#include "integration/frozen_red_line_checker.hpp"

#include <algorithm>
#include <cstdio>

namespace cfx {

void FrozenRedLineChecker::addResult(const std::string& id, const std::string& desc, bool ok, const std::string& evidence) noexcept {
    FrozenRedLineResult result{};
    result.id = id;
    result.description = desc;
    result.complied = ok;
    result.evidence = evidence;
    results_.push_back(std::move(result));
}

void FrozenRedLineChecker::checkR31_01_noFrozenFileModified(
    const std::vector<std::string>& modifiedFiles,
    const std::vector<std::string>& frozenFiles) noexcept {
    std::vector<std::string> violations;
    for (const auto& modified : modifiedFiles) {
        for (const auto& frozen : frozenFiles) {
            if (modified == frozen) {
                violations.push_back(modified);
            }
        }
    }
    addResult("R31-01", "No Frozen file modified",
              violations.empty(),
              violations.empty() ? "No Frozen files in modified list" : "Modified: " + violations[0]);
}

void FrozenRedLineChecker::checkR31_02_noFrozenInterfaceChanged(bool interfacesUnchanged) noexcept {
    addResult("R31-02", "No Frozen interface signature changed",
              interfacesUnchanged,
              interfacesUnchanged ? "All Frozen interfaces unchanged" : "Frozen interface modified");
}

void FrozenRedLineChecker::checkR31_03_noTestModifiedForPass(bool testsUnmodified) noexcept {
    addResult("R31-03", "No test modified only for PASS",
              testsUnmodified,
              testsUnmodified ? "No test modifications for PASS" : "Test modified for PASS");
}

void FrozenRedLineChecker::checkR31_04_techDebtPreserved(bool techDebtUnchanged) noexcept {
    addResult("R31-04", "Technical Debt preserved",
              techDebtUnchanged,
              techDebtUnchanged ? "Tech Debt unchanged" : "Tech Debt altered");
}

void FrozenRedLineChecker::checkR31_05_realPhysicalEvidence(bool evidenceIsPhysical) noexcept {
    addResult("R31-05", "Real physical evidence (not fabricated)",
              evidenceIsPhysical,
              evidenceIsPhysical ? "Evidence is physical" : "Evidence may be fabricated");
}

void FrozenRedLineChecker::checkR31_06_noUnitTestAsE2E(bool noUnitTestSubstitution) noexcept {
    addResult("R31-06", "No Unit Test as E2E Evidence",
              noUnitTestSubstitution,
              noUnitTestSubstitution ? "No Unit Test substitution" : "Unit Test used as E2E Evidence");
}

void FrozenRedLineChecker::checkR31_07_noTccBypass(bool tccNotBypassed) noexcept {
    addResult("R31-07", "No macOS TCC bypass",
              tccNotBypassed,
              tccNotBypassed ? "TCC not bypassed" : "TCC bypassed");
}

void FrozenRedLineChecker::checkR31_08_noRootSshImpersonation(bool noRootSsh) noexcept {
    addResult("R31-08", "No root/SSH impersonation",
              noRootSsh,
              noRootSsh ? "No root/SSH impersonation" : "root/SSH impersonation detected");
}

void FrozenRedLineChecker::checkR31_09_noUnrelatedFeatures(bool noUnrelatedCode) noexcept {
    addResult("R31-09", "No unrelated new features",
              noUnrelatedCode,
              noUnrelatedCode ? "No unrelated code" : "Unrelated code found");
}

void FrozenRedLineChecker::checkR31_10_noTask032Started(bool task032NotStarted) noexcept {
    addResult("R31-10", "TASK-032 not started",
              task032NotStarted,
              task032NotStarted ? "TASK-032 not started" : "TASK-032 started");
}

bool FrozenRedLineChecker::allRedLinesComplied() const noexcept {
    for (const auto& r : results_) {
        if (!r.complied) return false;
    }
    return true;
}

std::string FrozenRedLineChecker::generateReport() const noexcept {
    std::string report;
    report += "=== Frozen Red Line Check Report ===\n\n";
    report += "Red Line | Description | Verdict | Evidence\n";
    report += "---------|-------------|---------|----------\n";

    for (const auto& r : results_) {
        char line[512];
        std::snprintf(line, sizeof(line), "%s | %s | %s | %s\n",
                      r.id.c_str(), r.description.c_str(),
                      r.complied ? "COMPLIED" : "VIOLATED",
                      r.evidence.c_str());
        report += line;
    }

    report += "\n=== Summary ===\n";
    char summary[128];
    std::snprintf(summary, sizeof(summary), "All Red Lines: %s\n",
                  allRedLinesComplied() ? "ALL COMPLIED" : "HAS VIOLATIONS");
    report += summary;

    return report;
}

}  // namespace cfx