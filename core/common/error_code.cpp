#include "common/error_code.hpp"

#include <array>

namespace cfx {

namespace {

struct ErrorMeta {
    ErrorLevel level;
    ErrorModule module;
    std::string_view reason;
};

constexpr std::array<ErrorMeta, 18> kErrorMeta = {{
    {ErrorLevel::Error,   ErrorModule::Cap,      "PERM"},
    {ErrorLevel::Error,   ErrorModule::Cap,      "API"},
    {ErrorLevel::Error,   ErrorModule::Inj,      "PERM"},
    {ErrorLevel::Error,   ErrorModule::Inj,      "API"},
    {ErrorLevel::Warn,    ErrorModule::Norm,     "UNKNOWN"},
    {ErrorLevel::Warn,    ErrorModule::Norm,     "SEQBACK"},
    {ErrorLevel::Error,   ErrorModule::Topo,     "NODEID-DUP"},
    {ErrorLevel::Error,   ErrorModule::Topo,     "NEIGHBOR-OVER"},
    {ErrorLevel::Error,   ErrorModule::Topo,     "NONLINEAR"},
    {ErrorLevel::Error,   ErrorModule::Handoff,  "TIMEOUT"},
    {ErrorLevel::Info,    ErrorModule::Handoff,  "YIELD"},
    {ErrorLevel::Info,    ErrorModule::Handoff,  "REJECT"},
    {ErrorLevel::Error,   ErrorModule::Map,      "NOBOUNARY"},
    {ErrorLevel::Error,   ErrorModule::Cp,       "RETX-OVER"},
    {ErrorLevel::Warn,    ErrorModule::Ip,       "BACKLOG"},
    {ErrorLevel::Error,   ErrorModule::Proto,    "VER"},
    {ErrorLevel::Error,   ErrorModule::Link,     "DOWN"},
    {ErrorLevel::Error,   ErrorModule::Pair,     "UNPAIRED"},
}};

constexpr std::size_t errorCodeIndex(ErrorCode code) noexcept {
    return static_cast<std::size_t>(code);
}

}  // namespace

std::string_view levelString(ErrorLevel level) noexcept {
    switch (level) {
        case ErrorLevel::Error: return "E";
        case ErrorLevel::Warn:  return "W";
        case ErrorLevel::Info:  return "I";
    }
    return "E";
}

std::string_view moduleString(ErrorModule mod) noexcept {
    switch (mod) {
        case ErrorModule::Cap:      return "CAP";
        case ErrorModule::Inj:      return "INJ";
        case ErrorModule::Norm:     return "NORM";
        case ErrorModule::Topo:     return "TOPO";
        case ErrorModule::Handoff:  return "HANDOFF";
        case ErrorModule::Map:      return "MAP";
        case ErrorModule::Cp:       return "CP";
        case ErrorModule::Ip:       return "IP";
        case ErrorModule::Proto:    return "PROTO";
        case ErrorModule::Link:     return "LINK";
        case ErrorModule::Pair:     return "PAIR";
    }
    return "UNKNOWN";
}

ErrorLevel levelOf(ErrorCode code) noexcept {
    return kErrorMeta[errorCodeIndex(code)].level;
}

ErrorModule moduleOf(ErrorCode code) noexcept {
    return kErrorMeta[errorCodeIndex(code)].module;
}

std::string_view reasonOf(ErrorCode code) noexcept {
    return kErrorMeta[errorCodeIndex(code)].reason;
}

std::string to_string(ErrorCode code) {
    const auto& meta = kErrorMeta[errorCodeIndex(code)];
    std::string result;
    result.reserve(24);
    result.append("CFX-");
    result.append(levelString(meta.level));
    result.append("-");
    result.append(moduleString(meta.module));
    result.append("-");
    result.append(meta.reason);
    return result;
}

CfxError::CfxError(ErrorCode code) noexcept : code_(code) {}

ErrorLevel CfxError::level() const noexcept {
    return levelOf(code_);
}

ErrorModule CfxError::module() const noexcept {
    return moduleOf(code_);
}

std::string_view CfxError::reason() const noexcept {
    return reasonOf(code_);
}

std::string CfxError::to_string() const {
    return cfx::to_string(code_);
}

}  // namespace cfx