#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

namespace {

const std::vector<std::string> kForbiddenPatterns = {
    "CGEvent.h",
    "CGEventTap.h",
    "CGEventType",
    "CGEventRef",
    "NSScreen.h",
    "CGDisplay.h",
    "ApplicationServices",
    "CoreGraphics",
    "CoreFoundation",
    "Carbon/Carbon.h",
    "#ifdef __APPLE__",
    "#if defined(__APPLE__)",
    "kCGEvent",
    "CGEventTapCreate",
    "AXIsProcessTrusted",
};

bool fileContainsForbiddenPattern(const std::filesystem::path& filepath,
                                  std::string& matchedPattern) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        for (const auto& pattern : kForbiddenPatterns) {
            if (line.find(pattern) != std::string::npos) {
                matchedPattern = pattern;
                return true;
            }
        }
    }
    return false;
}

}  // namespace

int main() {
    printf("=== Platform Isolation Architecture Test ===\n\n");

    const char* repoRootEnv = std::getenv("CFX_REPO_ROOT");
    std::filesystem::path coreDir;
    if (repoRootEnv != nullptr) {
        coreDir = std::filesystem::path(repoRootEnv) / "core";
    } else {
        coreDir = std::filesystem::current_path() / "core";
    }

    if (!std::filesystem::exists(coreDir)) {
        printf("SKIP: core/ directory not found at %s\n", coreDir.string().c_str());
        return 0;
    }

    int violations = 0;
    int filesChecked = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(coreDir)) {
        if (!entry.is_regular_file()) continue;
        const auto& path = entry.path();
        auto ext = path.extension().string();
        if (ext != ".hpp" && ext != ".cpp" && ext != ".h") continue;

        ++filesChecked;
        std::string matched;
        if (fileContainsForbiddenPattern(path, matched)) {
            printf("VIOLATION: %s\n  Pattern: %s\n  File: %s\n",
                   matched.c_str(), matched.c_str(), path.string().c_str());
            ++violations;
        }
    }

    printf("\nFiles checked: %d\n", filesChecked);
    printf("Violations: %d\n", violations);

    if (violations > 0) {
        printf("\nFAIL: Platform isolation violated\n");
        return 1;
    }

    printf("\nPASS: Core layer contains no platform-specific headers\n");
    return 0;
}