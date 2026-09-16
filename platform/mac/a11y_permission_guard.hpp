#pragma once

#include "common/error_code.hpp"

namespace cfx {

enum class A11yPermissionStatus : uint8_t {
    Granted,
    AccessibilityDenied,
    InputMonitoringDenied,
    BothDenied,
};

class A11yPermissionGuard {
public:
    A11yPermissionGuard() noexcept = default;
    ~A11yPermissionGuard() noexcept = default;

    A11yPermissionGuard(const A11yPermissionGuard&) = delete;
    A11yPermissionGuard& operator=(const A11yPermissionGuard&) = delete;

    A11yPermissionStatus check() const noexcept;

    bool checkAccessibility() const noexcept;

    bool checkInputMonitoring() const noexcept;

    void promptAccessibility() const noexcept;

    void promptInputMonitoring() const noexcept;

    bool ensurePermissions() noexcept;

    ErrorCode lastError() const noexcept { return lastError_; }

private:
    ErrorCode lastError_{ErrorCode::CapA11yDenied};
};

}  // namespace cfx