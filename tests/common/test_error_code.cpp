#include <string>

#include "common/error_code.hpp"

namespace {

int testConstructionAndString() {
    using namespace cfx;
    CfxError e(ErrorCode::CapPerm);
    if (e.to_string() != std::string("CFX-E-CAP-PERM")) return 1;
    if (e.level() != ErrorLevel::Error) return 1;
    if (e.module() != ErrorModule::Cap) return 1;
    if (e.reason() != std::string_view("PERM")) return 1;
    return 0;
}

int testAllErrorCodes() {
    using namespace cfx;
    struct Case {
        ErrorCode code;
        ErrorLevel level;
        ErrorModule module;
        std::string_view str;
    };
    const Case cases[] = {
        {ErrorCode::CapPerm,           ErrorLevel::Error, ErrorModule::Cap,     "CFX-E-CAP-PERM"},
        {ErrorCode::CapApi,            ErrorLevel::Error, ErrorModule::Cap,     "CFX-E-CAP-API"},
        {ErrorCode::InjPerm,           ErrorLevel::Error, ErrorModule::Inj,     "CFX-E-INJ-PERM"},
        {ErrorCode::InjApi,            ErrorLevel::Error, ErrorModule::Inj,     "CFX-E-INJ-API"},
        {ErrorCode::NormUnknown,       ErrorLevel::Warn,  ErrorModule::Norm,    "CFX-W-NORM-UNKNOWN"},
        {ErrorCode::NormSeqback,       ErrorLevel::Warn,  ErrorModule::Norm,    "CFX-W-NORM-SEQBACK"},
        {ErrorCode::TopoNodeidDup,     ErrorLevel::Error, ErrorModule::Topo,    "CFX-E-TOPO-NODEID-DUP"},
        {ErrorCode::TopoNeighborOver,  ErrorLevel::Error, ErrorModule::Topo,    "CFX-E-TOPO-NEIGHBOR-OVER"},
        {ErrorCode::TopoNonlinear,     ErrorLevel::Error, ErrorModule::Topo,    "CFX-E-TOPO-NONLINEAR"},
        {ErrorCode::HandoffTimeout,    ErrorLevel::Error, ErrorModule::Handoff, "CFX-E-HANDOFF-TIMEOUT"},
        {ErrorCode::HandoffYield,      ErrorLevel::Info,  ErrorModule::Handoff, "CFX-I-HANDOFF-YIELD"},
        {ErrorCode::HandoffReject,     ErrorLevel::Info,  ErrorModule::Handoff, "CFX-I-HANDOFF-REJECT"},
        {ErrorCode::MapNoboundary,     ErrorLevel::Error, ErrorModule::Map,     "CFX-E-MAP-NOBOUNARY"},
        {ErrorCode::CpRetxOver,        ErrorLevel::Error, ErrorModule::Cp,      "CFX-E-CP-RETX-OVER"},
        {ErrorCode::IpBacklog,         ErrorLevel::Warn,  ErrorModule::Ip,      "CFX-W-IP-BACKLOG"},
        {ErrorCode::ProtoVer,          ErrorLevel::Error, ErrorModule::Proto,   "CFX-E-PROTO-VER"},
        {ErrorCode::LinkDown,          ErrorLevel::Error, ErrorModule::Link,    "CFX-E-LINK-DOWN"},
        {ErrorCode::PairUnpaired,      ErrorLevel::Error, ErrorModule::Pair,    "CFX-E-PAIR-UNPAIRED"},
    };
    for (const auto& c : cases) {
        if (levelOf(c.code) != c.level) return 1;
        if (moduleOf(c.code) != c.module) return 1;
        if (to_string(c.code) != std::string(c.str)) return 1;
    }
    return 0;
}

int testLevelString() {
    using namespace cfx;
    if (levelString(ErrorLevel::Error) != std::string_view("E")) return 1;
    if (levelString(ErrorLevel::Warn) != std::string_view("W")) return 1;
    if (levelString(ErrorLevel::Info) != std::string_view("I")) return 1;
    return 0;
}

int testModuleString() {
    using namespace cfx;
    if (moduleString(ErrorModule::Cap) != std::string_view("CAP")) return 1;
    if (moduleString(ErrorModule::Handoff) != std::string_view("HANDOFF")) return 1;
    if (moduleString(ErrorModule::Pair) != std::string_view("PAIR")) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testConstructionAndString()) return 1;
    if (testAllErrorCodes()) return 1;
    if (testLevelString()) return 1;
    if (testModuleString()) return 1;
    return 0;
}