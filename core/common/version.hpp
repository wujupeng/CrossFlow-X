#pragma once

#include <string_view>

namespace cfx::version {

constexpr int kMajor = 0;
constexpr int kMinor = 1;
constexpr int kPatch = 0;

inline std::string_view string() {
    return "0.1.0";
}

}  // namespace cfx::version