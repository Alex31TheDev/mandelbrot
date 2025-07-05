#pragma once

#include <cstdint>

namespace BufferUtil {
    template<typename T>
    constexpr T alignTo(T size, T alignment);

    template <size_t ALIGNMENT>
    uint8_t *bufferAlloc(size_t bufferSize, size_t *alignedSize = nullptr);

    template <size_t ALIGNMENT>
    void bufferFree(uint8_t *ptr) noexcept;

    template <size_t ALIGNMENT>
    struct AlignedDeleter {
        void operator()(uint8_t *ptr) noexcept;
    };
}

#include "BufferUtil_impl.h"
