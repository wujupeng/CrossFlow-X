#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cfx {

struct FrozenRedLineResult {
    std::string id;
    std::string description;
    bool complied{false};
    std::string evidence;
};

class FrozenRedLineChecker {
public:
    FrozenRedLineChecker() noexcept = default;
    ~FrozenRedLineChecker() noexcept = default;

    void checkR31_01_noFrozenFileModified(const std::vector<std::string>& modifiedFiles,
                                           const std::vector<std::string>& frozenFiles) noexcept;
    void checkR31_02_noFrozenInterfaceChanged(bool interfacesUnchanged) noexcept;
    void checkR31_03_noTestModifiedForPass(bool testsUnmodified) noexcept;
    void checkR31_04_techDebtPreserved(bool techDebtUnchanged) noexcept;
    void checkR31_05_realPhysicalEvidence(bool evidenceIsPhysical) noexcept;
    void checkR31_06_noUnitTestAsE2E(bool noUnitTestSubstitution) noexcept;
    void checkR31_07_noTccBypass(bool tccNotBypassed) noexcept;
    void checkR31_08_noRootSshImpersonation(bool noRootSsh) noexcept;
    void checkR31_09_noUnrelatedFeatures(bool noUnrelatedCode) noexcept;
    void checkR31_10_noTask032Started(bool task032NotStarted) noexcept;

    const std::vector<FrozenRedLineResult>& results() const noexcept { return results_; }
    bool allRedLinesComplied() const noexcept;
    std::string generateReport() const noexcept;

private:
    void addResult(const std::string& id, const std::string& desc, bool ok, const std::string& evidence) noexcept;

    std::vector<FrozenRedLineResult> results_;
};

}  // namespace cfx