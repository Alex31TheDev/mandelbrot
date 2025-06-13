#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

#ifdef _WIN32
#include <malloc.h>
#endif

template<typename T>
constexpr T BufferUtil::alignTo(T size, T alignment) {
    static_assert(std::is_integral_v<T>, "Alignment must be performed on integral types");
    return (size + alignment - 1) & ~(alignment - 1);
}

template <size_t ALIGNMENT>
uint8_t *BufferUtil::bufferAlloc(size_t bufferSize, size_t *alignedSizeOut) {
    void *ptr = nullptr;

    if constexpr (ALIGNMENT > 8) {
        size_t alignedSize = alignTo(bufferSize, ALIGNMENT);
        if (alignedSizeOut)  *alignedSizeOut = alignedSize;

#ifdef _WIN32
        ptr = _aligned_malloc(alignedSize, ALIGNMENT);
#else
        if (posix_memalign(&ptr, ALIGNMENT, alignedSize) != 0) {
            ptr = nullptr;
        }
#endif
    } else {
        if (alignedSizeOut)  *alignedSizeOut = bufferSize;
        ptr = malloc(bufferSize);
    }

    if (ptr) memset(ptr, 0, bufferSize);
    return static_cast<uint8_t *>(ptr);
}

template <size_t ALIGNMENT>
void BufferUtil::bufferFree(uint8_t *ptr) noexcept {
#if defined(_WIN32)
    if constexpr (ALIGNMENT > 8) {
        _aligned_free(ptr);
    } else
#endif
        free(ptr);
}

template <size_t ALIGNMENT>
void BufferUtil::AlignedDeleter<ALIGNMENT>::operator()(uint8_t *ptr) noexcept {
    BufferUtil::bufferFree<ALIGNMENT>(ptr);
}
