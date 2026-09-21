#pragma once

#include "common/domain.hpp"

#include <atomic>
#include <memory>

namespace cfx {

class ScreenBoundaryCache {
public:
    ScreenBoundaryCache();
    ~ScreenBoundaryCache();

    ScreenBoundaryCache(const ScreenBoundaryCache&) = delete;
    ScreenBoundaryCache& operator=(const ScreenBoundaryCache&) = delete;

    std::shared_ptr<const ScreenBoundary> getSnapshot() const;

    void publishSnapshot(std::shared_ptr<const ScreenBoundary> newSnapshot);

    void setReconfigPending();

    bool consumeReconfigPending();

    bool isValid() const;

private:
    mutable std::shared_ptr<const ScreenBoundary> snapshot_;
    std::atomic<bool> reconfigPending_{false};
};

}  // namespace cfx