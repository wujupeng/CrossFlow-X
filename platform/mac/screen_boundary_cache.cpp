#include "mac/screen_boundary_cache.hpp"

#include <memory>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

namespace cfx {

ScreenBoundaryCache::ScreenBoundaryCache()
    : snapshot_(nullptr)
    , reconfigPending_(false) {}

ScreenBoundaryCache::~ScreenBoundaryCache() = default;

std::shared_ptr<const ScreenBoundary> ScreenBoundaryCache::getSnapshot() const {
    return std::atomic_load_explicit(&snapshot_, std::memory_order_acquire);
}

void ScreenBoundaryCache::publishSnapshot(std::shared_ptr<const ScreenBoundary> newSnapshot) {
    std::atomic_store_explicit(&snapshot_, std::move(newSnapshot), std::memory_order_release);
}

void ScreenBoundaryCache::setReconfigPending() {
    reconfigPending_.store(true, std::memory_order_release);
}

bool ScreenBoundaryCache::consumeReconfigPending() {
    return reconfigPending_.exchange(false, std::memory_order_acq_rel);
}

bool ScreenBoundaryCache::isValid() const {
    auto snap = std::atomic_load_explicit(&snapshot_, std::memory_order_acquire);
    return snap != nullptr && snap->isValid();
}

}  // namespace cfx

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
