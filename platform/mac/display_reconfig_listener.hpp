#pragma once

#include "mac/screen_boundary_cache.hpp"

namespace cfx {

class DisplayReconfigListener {
public:
    explicit DisplayReconfigListener(ScreenBoundaryCache& cache) noexcept;
    ~DisplayReconfigListener();

    DisplayReconfigListener(const DisplayReconfigListener&) = delete;
    DisplayReconfigListener& operator=(const DisplayReconfigListener&) = delete;

    bool registerCallback() noexcept;
    void unregisterCallback() noexcept;
    bool isRegistered() const noexcept;

private:
    ScreenBoundaryCache& cache_;
    bool registered_{false};

};

}  // namespace cfx