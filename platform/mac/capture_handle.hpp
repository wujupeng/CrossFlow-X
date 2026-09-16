#pragma once

#include <atomic>
#include <cstdint>

#include "common/domain.hpp"
#include "common/platform_ports.hpp"

namespace cfx {

class CaptureHandleManager {
public:
    CaptureHandleManager() noexcept = default;

    CaptureHandleManager(const CaptureHandleManager&) = delete;
    CaptureHandleManager& operator=(const CaptureHandleManager&) = delete;

    CaptureHandle acquire() noexcept {
        const u32 id = nextId_.fetch_add(1, std::memory_order_relaxed);
        CaptureHandle h;
        h.id = id;
        h.active = true;
        activeCount_.fetch_add(1, std::memory_order_relaxed);
        return h;
    }

    void release(CaptureHandle& handle) noexcept {
        if (handle.id == 0 || !handle.active) {
            return;
        }
        handle.active = false;
        activeCount_.fetch_sub(1, std::memory_order_relaxed);
    }

    bool isValid(const CaptureHandle& handle) const noexcept {
        return handle.id != 0 && handle.active;
    }

    u32 activeCount() const noexcept {
        return activeCount_.load(std::memory_order_acquire);
    }

private:
    std::atomic<u32> nextId_{1};
    std::atomic<u32> activeCount_{0};
};

class ScopedCaptureHandle {
public:
    ScopedCaptureHandle() noexcept = default;

    explicit ScopedCaptureHandle(CaptureHandleManager& mgr) noexcept : mgr_(&mgr) {
        handle_ = mgr_->acquire();
    }

    ScopedCaptureHandle(const ScopedCaptureHandle&) = delete;
    ScopedCaptureHandle& operator=(const ScopedCaptureHandle&) = delete;

    ScopedCaptureHandle(ScopedCaptureHandle&& other) noexcept
        : mgr_(other.mgr_), handle_(other.handle_) {
        other.mgr_ = nullptr;
        other.handle_ = {};
    }

    ScopedCaptureHandle& operator=(ScopedCaptureHandle&& other) noexcept {
        if (this != &other) {
            release();
            mgr_ = other.mgr_;
            handle_ = other.handle_;
            other.mgr_ = nullptr;
            other.handle_ = {};
        }
        return *this;
    }

    ~ScopedCaptureHandle() noexcept {
        release();
    }

    const CaptureHandle& get() const noexcept { return handle_; }
    bool isValid() const noexcept { return mgr_ != nullptr && handle_.active; }

private:
    void release() noexcept {
        if (mgr_ && handle_.active) {
            mgr_->release(handle_);
        }
    }

    CaptureHandleManager* mgr_{nullptr};
    CaptureHandle handle_{};
};

}  // namespace cfx