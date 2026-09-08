#include "common/error_code.hpp"

#include <array>

namespace cfx {

namespace {

struct ErrorMeta {
    ErrorLevel level;
    ErrorModule module;
    std::string_view reason;
};

constexpr std::array<ErrorMeta, 49> kErrorMeta = {{
    {ErrorLevel::Error,     ErrorModule::Cap,         "PERM"},
    {ErrorLevel::Error,     ErrorModule::Cap,         "API"},
    {ErrorLevel::Error,     ErrorModule::Inj,         "PERM"},
    {ErrorLevel::Error,     ErrorModule::Inj,         "API"},
    {ErrorLevel::Warn,      ErrorModule::Norm,        "UNKNOWN"},
    {ErrorLevel::Warn,      ErrorModule::Norm,        "SEQBACK"},
    {ErrorLevel::Error,     ErrorModule::Topo,        "NODEID-DUP"},
    {ErrorLevel::Error,     ErrorModule::Topo,        "NEIGHBOR-OVER"},
    {ErrorLevel::Error,     ErrorModule::Topo,        "NONLINEAR"},
    {ErrorLevel::Error,     ErrorModule::Handoff,     "TIMEOUT"},
    {ErrorLevel::Info,      ErrorModule::Handoff,     "YIELD"},
    {ErrorLevel::Info,      ErrorModule::Handoff,     "REJECT"},
    {ErrorLevel::Error,     ErrorModule::Map,         "NOBOUNARY"},
    {ErrorLevel::Error,     ErrorModule::Cp,          "RETX-OVER"},
    {ErrorLevel::Warn,      ErrorModule::Ip,          "BACKLOG"},
    {ErrorLevel::Error,     ErrorModule::Proto,       "VER"},
    {ErrorLevel::Error,     ErrorModule::Link,        "DOWN"},
    {ErrorLevel::Error,     ErrorModule::Pair,        "UNPAIRED"},
    {ErrorLevel::Error,     ErrorModule::Session,     "UUID-GEN-FAIL"},
    {ErrorLevel::Warn,      ErrorModule::Session,     "PERSIST-CORRUPT"},
    {ErrorLevel::Error,     ErrorModule::Session,     "NODEID-FORGE"},
    {ErrorLevel::Warn,      ErrorModule::Session,     "STALE-EPOCH"},
    {ErrorLevel::Error,     ErrorModule::Session,     "INSTANCE-CONFLICT"},
    {ErrorLevel::Warn,      ErrorModule::Session,     "UNKNOWN-NODE"},
    {ErrorLevel::Warn,      ErrorModule::Discovery,   "MDNS-UNAVAILABLE"},
    {ErrorLevel::Warn,      ErrorModule::Discovery,   "CROSS-SUBNET"},
    {ErrorLevel::Warn,      ErrorModule::Discovery,   "MANUAL-FALLBACK"},
    {ErrorLevel::Warn,      ErrorModule::Discovery,   "ANNOUNCE-LOST"},
    {ErrorLevel::Error,     ErrorModule::Pair,        "CODE-MISMATCH"},
    {ErrorLevel::Error,     ErrorModule::Pair,        "ILLEGAL-TRANS"},
    {ErrorLevel::Error,     ErrorModule::Pair,        "CODE-BROADCAST"},
    {ErrorLevel::Error,     ErrorModule::Pair,        "TRUST-PERSIST-FAIL"},
    {ErrorLevel::Warn,      ErrorModule::Pair,        "TRUST-CORRUPT"},
    {ErrorLevel::Error,     ErrorModule::Registration,"UNDISCOVERED"},
    {ErrorLevel::Error,     ErrorModule::Registration,"CAP-INCOMPAT"},
    {ErrorLevel::Error,     ErrorModule::Registration,"TOPOID-MISMATCH"},
    {ErrorLevel::Error,     ErrorModule::Registration,"LINK-BROKEN"},
    {ErrorLevel::Warn,      ErrorModule::Topo,        "STALE-VERSION"},
    {ErrorLevel::Warn,      ErrorModule::Topo,        "VERSION-CONFLICT"},
    {ErrorLevel::Error,     ErrorModule::Topo,        "VALIDATION-FAIL"},
    {ErrorLevel::Error,     ErrorModule::Topo,        "COMMIT-FAIL"},
    {ErrorLevel::Error,     ErrorModule::Topo,        "MULTIPATH"},
    {ErrorLevel::Warn,      ErrorModule::Topo,        "DEGRADED-NON-CIRCULAR"},
    {ErrorLevel::Warn,      ErrorModule::Topo,        "DEGRADED-SINGLE"},
    {ErrorLevel::Error,     ErrorModule::Topo,        "COORD-DUPLICATE-CLAIM"},
    {ErrorLevel::Error,     ErrorModule::Recovery,    "UNTRUSTED"},
    {ErrorLevel::Error,     ErrorModule::Recovery,    "UNREACHABLE"},
    {ErrorLevel::Error,     ErrorModule::Recovery,    "TIMEOUT"},
    {ErrorLevel::Warn,      ErrorModule::Handoff,     "UNREGISTERED-PEER"},
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
        case ErrorModule::Cap:         return "CAP";
        case ErrorModule::Inj:         return "INJ";
        case ErrorModule::Norm:        return "NORM";
        case ErrorModule::Topo:        return "TOPO";
        case ErrorModule::Handoff:     return "HANDOFF";
        case ErrorModule::Map:         return "MAP";
        case ErrorModule::Cp:          return "CP";
        case ErrorModule::Ip:          return "IP";
        case ErrorModule::Proto:       return "PROTO";
        case ErrorModule::Link:        return "LINK";
        case ErrorModule::Pair:        return "PAIR";
        case ErrorModule::Session:     return "SESS";
        case ErrorModule::Discovery:   return "DISC";
        case ErrorModule::Registration:return "REG";
        case ErrorModule::Recovery:    return "RECOVERY";
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
    result.reserve(32);
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
