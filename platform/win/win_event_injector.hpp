#pragma once

#include <atomic>
#include <cstdint>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"

namespace cfx {

class WinEventInjector : public IInputInjector {
public:
    explicit WinEventInjector(NodeId sourceNodeId = {}) noexcept;
    ~WinEventInjector() noexcept override = default;

    WinEventInjector(const WinEventInjector&) = delete;
    WinEventInjector& operator=(const WinEventInjector&) = delete;

    InjectResult inject(const CanonicalInputEvent& event) noexcept override;
    InjectResult injectBatch(const std::vector<CanonicalInputEvent>& events) noexcept override;
    ReleaseResult releaseAllPressed(const PressedStateSnapshot& pressed) noexcept override;

    void setSourceNodeId(NodeId id) noexcept { sourceNodeId_ = id; }
    NodeId sourceNodeId() const noexcept { return sourceNodeId_; }

    uint64_t totalInjected() const noexcept { return totalInjected_.load(std::memory_order_relaxed); }
    uint64_t totalRejected() const noexcept { return totalRejected_.load(std::memory_order_relaxed); }

    bool syncModifiers(const ModifierState& sourceState) noexcept;
    ModifierState localModifierState() const noexcept { return localModifierState_; }

private:
    InjectResult injectMouseEvent(const CanonicalInputEvent& event) noexcept;
    InjectResult injectMouseButtonEvent(const CanonicalInputEvent& event) noexcept;
    InjectResult injectWheelEvent(const CanonicalInputEvent& event) noexcept;
    InjectResult injectKeyEvent(const CanonicalInputEvent& event) noexcept;

    bool validateSource(const CanonicalInputEvent& event) const noexcept;

    NodeId sourceNodeId_{};
    ModifierState localModifierState_{false, false, false, false, false};
    std::atomic<uint64_t> totalInjected_{0};
    std::atomic<uint64_t> totalRejected_{0};
};

}  // namespace cfx