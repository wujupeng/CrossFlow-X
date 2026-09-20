#pragma once

#include <cstdint>
#include <functional>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"

namespace cfx {

class MacEventInjector : public IInputInjector {
public:
    explicit MacEventInjector(NodeId sourceNodeId = {}) noexcept;
    ~MacEventInjector() noexcept override = default;

    MacEventInjector(const MacEventInjector&) = delete;
    MacEventInjector& operator=(const MacEventInjector&) = delete;

    InjectResult inject(const CanonicalInputEvent& event) noexcept override;
    InjectResult injectBatch(const std::vector<CanonicalInputEvent>& events) noexcept override;
    ReleaseResult releaseAllPressed(const PressedStateSnapshot& pressed) noexcept override;

    void setSourceNodeId(NodeId id) noexcept { sourceNodeId_ = id; }
    NodeId sourceNodeId() const noexcept { return sourceNodeId_; }

    void setControllerMode(bool isController) noexcept { isController_ = isController; }
    bool isControllerMode() const noexcept { return isController_; }

    uint64_t totalInjected() const noexcept { return totalInjected_.load(std::memory_order_relaxed); }
    uint64_t totalRejected() const noexcept { return totalRejected_.load(std::memory_order_relaxed); }

    enum class InjectionMethod : uint8_t {
        RelativeDelta,
        AbsolutePosition,
        LocationCompute,
    };

    void setInjectionMethod(InjectionMethod method) noexcept { injectionMethod_ = method; }
    InjectionMethod injectionMethod() const noexcept { return injectionMethod_; }

    void setScreenBoundary(ScreenBoundary sb) noexcept { screenBoundary_ = sb; }
    ScreenBoundary screenBoundary() const noexcept { return screenBoundary_; }

    bool syncModifiers(const ModifierState& sourceState) noexcept;

    ModifierState localModifierState() const noexcept { return localModifierState_; }

private:
#ifdef __APPLE__
    InjectResult injectMouseEvent(const CanonicalInputEvent& event) noexcept;
    InjectResult injectMouseButtonEvent(const CanonicalInputEvent& event) noexcept;
    InjectResult injectWheelEvent(const CanonicalInputEvent& event) noexcept;
    InjectResult injectKeyEvent(const CanonicalInputEvent& event) noexcept;
#else
    InjectResult injectMouseEvent(const CanonicalInputEvent& event) noexcept { (void)event; return {true, 0, 0}; }
    InjectResult injectMouseButtonEvent(const CanonicalInputEvent& event) noexcept { (void)event; return {true, 0, 0}; }
    InjectResult injectWheelEvent(const CanonicalInputEvent& event) noexcept { (void)event; return {true, 0, 0}; }
    InjectResult injectKeyEvent(const CanonicalInputEvent& event) noexcept { (void)event; return {true, 0, 0}; }
#endif

    bool validateSource(const CanonicalInputEvent& event) const noexcept;
    bool validateParams(const CanonicalInputEvent& event) const noexcept;

    NodeId sourceNodeId_{};
    bool isController_{false};
    InjectionMethod injectionMethod_{InjectionMethod::RelativeDelta};
    ScreenBoundary screenBoundary_{};
    ModifierState localModifierState_{false, false, false, false, false};
    std::atomic<uint64_t> totalInjected_{0};
    std::atomic<uint64_t> totalRejected_{0};
};

}  // namespace cfx