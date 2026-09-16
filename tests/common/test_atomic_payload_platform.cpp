#include <atomic>
#include <cstdint>

#include "common/atomic_payload.hpp"
#include "common/error_code.hpp"

namespace {

int testPlatformSupported() {
    if (!cfx::AtomicPayload<uint64_t>::isPlatformSupported()) return 1;
    if (!cfx::AtomicPayload<uint64_t>::runtimeLockFreeCheck()) return 1;
    return 0;
}

int testAtomicU64IsLockFree() {
    std::atomic<uint64_t> a{0};
    if (!a.is_lock_free()) return 1;
    return 0;
}

int testArchUnsupportedErrorCode() {
    using namespace cfx;
    auto str = to_string(ErrorCode::CapArchUnsupported);
    if (str.find("CAP") == std::string_view::npos) return 1;
    if (str.find("ARCH-UNSUPPORTED") == std::string_view::npos) return 1;
    return 0;
}

int testNoMutexFallback() {
    struct Payload {
        uint64_t a;
        uint64_t b;
        uint64_t c;
    };

    cfx::AtomicPayload<Payload> ap;
    Payload src{1, 2, 3};
    ap.store(src);

    Payload dst = ap.load();
    if (dst.a != 1) return 1;
    if (dst.b != 2) return 1;
    if (dst.c != 3) return 1;

    if (!cfx::AtomicPayload<Payload>::isPlatformSupported()) return 1;
    return 0;
}

int testWordCountMatchesSize() {
    struct Small {
        uint64_t a;
    };
    struct Medium {
        uint64_t a;
        uint64_t b;
    };
    struct Large {
        uint64_t a;
        uint64_t b;
        uint64_t c;
    };

    if (cfx::AtomicPayload<Small>::wordCount() != 1) return 1;
    if (cfx::AtomicPayload<Medium>::wordCount() != 2) return 1;
    if (cfx::AtomicPayload<Large>::wordCount() != 3) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testPlatformSupported()) return 1;
    if (testAtomicU64IsLockFree()) return 1;
    if (testArchUnsupportedErrorCode()) return 1;
    if (testNoMutexFallback()) return 1;
    if (testWordCountMatchesSize()) return 1;
    return 0;
}