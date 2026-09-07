#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace cfx {

enum class ErrorLevel : uint8_t {
    Error,
    Warn,
    Info,
};

enum class ErrorModule : uint8_t {
    Cap,
    Inj,
    Norm,
    Topo,
    Handoff,
    Map,
    Cp,
    Ip,
    Proto,
    Link,
    Pair,
};

enum class ErrorCode : uint16_t {
    CapPerm,
    CapApi,
    InjPerm,
    InjApi,
    NormUnknown,
    NormSeqback,
    TopoNodeidDup,
    TopoNeighborOver,
    TopoNonlinear,
    HandoffTimeout,
    HandoffYield,
    HandoffReject,
    MapNoboundary,
    CpRetxOver,
    IpBacklog,
    ProtoVer,
    LinkDown,
    PairUnpaired,
};

std::string_view levelString(ErrorLevel level) noexcept;
std::string_view moduleString(ErrorModule mod) noexcept;

ErrorLevel levelOf(ErrorCode code) noexcept;
ErrorModule moduleOf(ErrorCode code) noexcept;
std::string_view reasonOf(ErrorCode code) noexcept;
std::string to_string(ErrorCode code);

class CfxError {
public:
    explicit CfxError(ErrorCode code) noexcept;

    ErrorCode code() const noexcept { return code_; }
    ErrorLevel level() const noexcept;
    ErrorModule module() const noexcept;
    std::string_view reason() const noexcept;
    std::string to_string() const;

private:
    ErrorCode code_;
};

}  // namespace cfx