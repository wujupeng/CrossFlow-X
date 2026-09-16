#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if !defined(__x86_64__) && !defined(__aarch64__) && !defined(_M_X64) && !defined(_M_ARM64)
#error "CFX-E-CAP-ARCH-UNSUPPORTED: CF2 requires x86_64 or arm64 for word-atomic payload"
#endif

namespace cfx {

static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "CFX-E-CAP-ARCH-UNSUPPORTED: std::atomic<uint64_t> must be always lock-free on CF2 supported platforms");

using ap_u64 = uint64_t;

template <typename T>
class AtomicPayload {
    static_assert(std::is_trivially_copyable_v<T>,
                  "AtomicPayload<T> requires T to be trivially copyable");

public:
    static constexpr std::size_t N = (sizeof(T) + 7) / 8;

    void store(const T& value, std::memory_order mo = std::memory_order_relaxed) noexcept {
        alignas(T) unsigned char src[sizeof(T)];
        std::memcpy(src, &value, sizeof(T));
        for (std::size_t k = 0; k < N; ++k) {
            ap_u64 word = 0;
            const std::size_t byteOffset = k * 8;
            const std::size_t bytesToCopy = (byteOffset + 8 <= sizeof(T)) ? 8 : (sizeof(T) - byteOffset);
            std::memcpy(&word, src + byteOffset, bytesToCopy);
            words_[k].store(word, mo);
        }
    }

    T load(std::memory_order mo = std::memory_order_relaxed) const noexcept {
        alignas(T) unsigned char dst[sizeof(T)] = {};
        for (std::size_t k = 0; k < N; ++k) {
            const ap_u64 word = words_[k].load(mo);
            const std::size_t byteOffset = k * 8;
            const std::size_t bytesToCopy = (byteOffset + 8 <= sizeof(T)) ? 8 : (sizeof(T) - byteOffset);
            std::memcpy(dst + byteOffset, &word, bytesToCopy);
        }
        T result;
        std::memcpy(&result, dst, sizeof(T));
        return result;
    }

    static constexpr std::size_t wordCount() noexcept { return N; }

    static constexpr bool isPlatformSupported() noexcept {
        return std::atomic<ap_u64>::is_always_lock_free;
    }

    static bool runtimeLockFreeCheck() noexcept {
        std::atomic<ap_u64> a{0};
        return a.is_lock_free();
    }

private:
    std::atomic<ap_u64> words_[N]{};
};

}  // namespace cfx