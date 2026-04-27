#pragma once

#include <cstdint>
#include <optional>
#include <functional>
#include <memory>

namespace BufferUtil {
    template<typename T>
    constexpr T alignTo(T size, T alignment);

    template <size_t ALIGNMENT>
    uint8_t *bufferAlloc(size_t bufferSize,
        std::optional<std::reference_wrapper<size_t>> alignedSize = std::nullopt);
        
    template <size_t ALIGNMENT>
    void bufferFree(uint8_t *ptr) noexcept;

    template <size_t ALIGNMENT>
    struct AlignedDeleter {
        void operator()(uint8_t *ptr) noexcept;
    };

    template <size_t ALIGNMENT>
    using AlignedBuffer = std::unique_ptr<
        uint8_t[],
        AlignedDeleter<ALIGNMENT>
    >;

    template <size_t ALIGNMENT>
    AlignedBuffer<ALIGNMENT> bufferAllocPtr(size_t bufferSize,
        std::optional<std::reference_wrapper<size_t>> alignedSize = std::nullopt);
}

#include "BufferUtil_impl.h"
