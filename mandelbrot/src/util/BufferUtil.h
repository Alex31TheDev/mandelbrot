#pragma once

#include <cstdint>

namespace BufferUtil {
    constexpr size_t alignTo(size_t size, size_t alignment);

    template <size_t ALIGNMENT>
    uint8_t *bufferAlloc(size_t bufferSize, size_t *alignedSizeOut = nullptr);

    template <size_t ALIGNMENT>
    void bufferFree(uint8_t *ptr) noexcept;

    template <size_t ALIGNMENT>
    struct AlignedDeleter {
        void operator()(uint8_t *ptr) noexcept;
    };
}

#include "BufferUtil_impl.h"
