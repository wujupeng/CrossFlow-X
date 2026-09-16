#include <atomic>
#include <cstdint>
#include <string>

#include "common/atomic_payload.hpp"
#include "common/platform_ports.hpp"

namespace {

int testWordCount() {
    using namespace cfx;
    struct SmallPayload {
        u64 a;
    };
    struct MediumPayload {
        u64 a;
        u64 b;
        u32 c;
    };
    struct LargePayload {
        u64 a[5];
    };

    static_assert(AtomicPayload<SmallPayload>::N == 1);
    static_assert(AtomicPayload<MediumPayload>::N == 3);
    static_assert(AtomicPayload<LargePayload>::N == 5);
    static_assert(AtomicPayload<RawInputEvent>::N >= 1);

    if (AtomicPayload<SmallPayload>::wordCount() != 1) return 1;
    if (AtomicPayload<MediumPayload>::wordCount() != 3) return 1;
    if (AtomicPayload<LargePayload>::wordCount() != 5) return 1;
    return 0;
}

int testStoreLoadRoundtrip() {
    using namespace cfx;
    struct Payload {
        u64 a;
        u64 b;
        u32 c;
        u32 d;
    };

    AtomicPayload<Payload> ap;
    Payload src{0x1111111111111111ULL, 0x2222222222222222ULL, 0x33333333, 0x44444444};
    ap.store(src, std::memory_order_relaxed);

    Payload dst = ap.load(std::memory_order_relaxed);
    if (dst.a != src.a) return 1;
    if (dst.b != src.b) return 1;
    if (dst.c != src.c) return 1;
    if (dst.d != src.d) return 1;
    return 0;
}

int testRawInputEventRoundtrip() {
    using namespace cfx;
    AtomicPayload<RawInputEvent> ap;

    RawInputEvent src;
    src.platformTime = 0xDEADBEEFCAFEULL;
    src.kind = RawEventKind::KeyPress;
    src.payload = RawKeyPayload{65};

    ap.store(src, std::memory_order_release);
    RawInputEvent dst = ap.load(std::memory_order_acquire);

    if (dst.platformTime != src.platformTime) return 1;
    if (dst.kind != src.kind) return 1;
    if (!std::holds_alternative<RawKeyPayload>(dst.payload)) return 1;
    if (std::get<RawKeyPayload>(dst.payload).keyCode != 65) return 1;
    return 0;
}

int testTriviallyCopyable() {
    using namespace cfx;
    static_assert(std::is_trivially_copyable_v<RawInputEvent>);
    static_assert(std::is_trivially_copyable_v<u64>);
    static_assert(std::is_trivially_copyable_v<MouseButtonBitmap>);
    static_assert(std::is_trivially_copyable_v<KeyCodeBitmap>);
    return 0;
}

int testSingleWordPayload() {
    using namespace cfx;
    AtomicPayload<u64> ap;
    ap.store(0xABCDEF0123456789ULL, std::memory_order_relaxed);
    if (ap.load(std::memory_order_relaxed) != 0xABCDEF0123456789ULL) return 1;

    ap.store(0, std::memory_order_relaxed);
    if (ap.load(std::memory_order_relaxed) != 0) return 1;
    return 0;
}

}  // namespace

int main() {
    if (testWordCount()) return 1;
    if (testStoreLoadRoundtrip()) return 1;
    if (testRawInputEventRoundtrip()) return 1;
    if (testTriviallyCopyable()) return 1;
    if (testSingleWordPayload()) return 1;
    return 0;
}