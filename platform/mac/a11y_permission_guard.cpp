#include "mac/a11y_permission_guard.hpp"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace cfx {

#ifdef __APPLE__

A11yPermissionStatus A11yPermissionGuard::check() const noexcept {
    const bool a11y = checkAccessibility();
    const bool inputMon = checkInputMonitoring();

    if (a11y && inputMon) {
        return A11yPermissionStatus::Granted;
    }
    if (!a11y && !inputMon) {
        return A11yPermissionStatus::BothDenied;
    }
    if (!a11y) {
        return A11yPermissionStatus::AccessibilityDenied;
    }
    return A11yPermissionStatus::InputMonitoringDenied;
}

bool A11yPermissionGuard::checkAccessibility() const noexcept {
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        (const void**)(const void*[]){kAXTrustedCheckOptionPrompt},
        (const void**)(const void*[]){kCFBooleanTrue},
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    const bool trusted = AXIsProcessTrustedWithOptions(options);
    if (options) {
        CFRelease(options);
    }
    return trusted;
}

bool A11yPermissionGuard::checkInputMonitoring() const noexcept {
    return CGPreflightSessionEventAccess();
}

void A11yPermissionGuard::promptAccessibility() const noexcept {
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        (const void**)(const void*[]){kAXTrustedCheckOptionPrompt},
        (const void**)(const void*[]){kCFBooleanTrue},
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    (void)AXIsProcessTrustedWithOptions(options);
    if (options) {
        CFRelease(options);
    }
}

void A11yPermissionGuard::promptInputMonitoring() const noexcept {
    (void)CGRequestSessionEventAccess();
}

bool A11yPermissionGuard::ensurePermissions() noexcept {
    const A11yPermissionStatus status = check();
    if (status == A11yPermissionStatus::Granted) {
        lastError_ = ErrorCode{};
        return true;
    }

    lastError_ = ErrorCode::CapA11yDenied;

    if (status == A11yPermissionStatus::AccessibilityDenied ||
        status == A11yPermissionStatus::BothDenied) {
        promptAccessibility();
    }
    if (status == A11yPermissionStatus::InputMonitoringDenied ||
        status == A11yPermissionStatus::BothDenied) {
        promptInputMonitoring();
    }
    return false;
}

#else

A11yPermissionStatus A11yPermissionGuard::check() const noexcept {
    return A11yPermissionStatus::Granted;
}

bool A11yPermissionGuard::checkAccessibility() const noexcept {
    return true;
}

bool A11yPermissionGuard::checkInputMonitoring() const noexcept {
    return true;
}

void A11yPermissionGuard::promptAccessibility() const noexcept {}
void A11yPermissionGuard::promptInputMonitoring() const noexcept {}

bool A11yPermissionGuard::ensurePermissions() noexcept {
    lastError_ = ErrorCode{};
    return true;
}

#endif

}  // namespace cfx