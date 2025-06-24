#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace BufferUtil {
    template<typename T>
    constexpr T alignTo(T size, T alignment) {
        static_assert(std::is_integral_v<T>,
            "Alignment can only be performed on integral types");

        if (alignment <= 0) return size;
        return (size + alignment - 1) & ~(alignment - 1);
    }

    template <size_t ALIGNMENT>
    uint8_t *bufferAlloc(size_t bufferSize, size_t *alignedSize) {
        size_t newSize;
        void *ptr = nullptr;

        if constexpr (ALIGNMENT > 8) {
            newSize = bufferSize % ALIGNMENT == 0
                ? bufferSize : alignTo(bufferSize, ALIGNMENT);
            if (alignedSize) *alignedSize = newSize;

#ifdef _WIN32
            ptr = _aligned_malloc(newSize, ALIGNMENT);
#else
            if (posix_memalign(&ptr, ALIGNMENT, newSize) != 0) {
                ptr = nullptr;
            }
#endif
        } else {
            newSize = bufferSize;
            if (alignedSize) *alignedSize = newSize;

            ptr = malloc(bufferSize);
        }

        if (ptr) memset(ptr, 0, newSize);
        return static_cast<uint8_t *>(ptr);
    }

    template <size_t ALIGNMENT>
    void bufferFree(uint8_t *ptr) noexcept {
#if defined(_WIN32)
        if constexpr (ALIGNMENT > 8) {
            _aligned_free(ptr);
        } else
#endif
            free(ptr);
    }

    template <size_t ALIGNMENT>
    void AlignedDeleter<ALIGNMENT>::operator()(uint8_t *ptr) noexcept {
        bufferFree<ALIGNMENT>(ptr);
    }
}