#include "mac/screen_boundary_cache.hpp"

namespace cfx {

ScreenBoundaryCache::ScreenBoundaryCache()
    : snapshot_(nullptr)
    , reconfigPending_(false) {}

ScreenBoundaryCache::~ScreenBoundaryCache() = default;

std::shared_ptr<const ScreenBoundary> ScreenBoundaryCache::getSnapshot() const {
    return snapshot_.load(std::memory_order_acquire);
}

void ScreenBoundaryCache::publishSnapshot(std::shared_ptr<const ScreenBoundary> newSnapshot) {
    snapshot_.store(std::move(newSnapshot), std::memory_order_release);
}

void ScreenBoundaryCache::setReconfigPending() {
    reconfigPending_.store(true, std::memory_order_release);
}

bool ScreenBoundaryCache::consumeReconfigPending() {
    return reconfigPending_.exchange(false, std::memory_order_acq_rel);
}

bool ScreenBoundaryCache::isValid() const {
    auto snap = snapshot_.load(std::memory_order_acquire);
    return snap != nullptr && snap->isValid();
}

}  // namespace cfx