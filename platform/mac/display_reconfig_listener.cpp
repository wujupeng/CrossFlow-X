#include "mac/display_reconfig_listener.hpp"

#include <cstdio>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace cfx {

DisplayReconfigListener::DisplayReconfigListener(ScreenBoundaryCache& cache) noexcept
    : cache_(cache) {}

DisplayReconfigListener::~DisplayReconfigListener() {
    unregisterCallback();
}

#ifdef __APPLE__
void DisplayReconfigListener::reconfigCallback(CGDirectDisplayID /*display*/,
                                                CGDisplayChangeSummaryFlags /*flags*/,
                                                void* userInfo) {
    if (userInfo) {
        auto* cache = static_cast<ScreenBoundaryCache*>(userInfo);
        cache->setReconfigPending();
    }
}
#endif

bool DisplayReconfigListener::registerCallback() noexcept {
#ifdef __APPLE__
    if (registered_) {
        return true;
    }
    CGError err = CGDisplayRegisterReconfigurationCallback(reconfigCallback, &cache_);
    if (err != kCGErrorSuccess) {
        fprintf(stderr, "[CFX-E-CAP-RECONFIG-REGISTER-FAIL] CGDisplayRegisterReconfigurationCallback failed: %d\n", static_cast<int>(err));
        return false;
    }
    registered_ = true;
    return true;
#else
    registered_ = true;
    return true;
#endif
}

void DisplayReconfigListener::unregisterCallback() noexcept {
#ifdef __APPLE__
    if (!registered_) {
        return;
    }
    CGDisplayRemoveReconfigurationCallback(reconfigCallback, &cache_);
    registered_ = false;
#else
    registered_ = false;
#endif
}

bool DisplayReconfigListener::isRegistered() const noexcept {
    return registered_;
}

}  // namespace cfx