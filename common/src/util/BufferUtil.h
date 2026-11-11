#pragma once

#include <cstdint>
#include <optional>
#include <functional>
#include <memory>

namespace BufferUtil {
    template<typename T>
    constexpr T alignTo(T size, T alignment);

    template <typename T, size_t ALIGNMENT>
    T *bufferAlloc(size_t count,
        std::optional<std::reference_wrapper<size_t>> alignedSize = std::nullopt);

    template <typename T, size_t ALIGNMENT>
    void bufferFree(T *ptr) noexcept;

    template <typename T, size_t ALIGNMENT>
    struct AlignedDeleter {
        void operator()(T *ptr) noexcept;
    };

    template <typename T, size_t ALIGNMENT>
    using AlignedBuffer = std::unique_ptr<
        T[],
        AlignedDeleter<T, ALIGNMENT>
    >;

    template <typename T, size_t ALIGNMENT>
    AlignedBuffer<T, ALIGNMENT> bufferAllocPtr(
        size_t count,
        std::optional<std::reference_wrapper<size_t>> alignedSize = std::nullopt
    );
}

#include "BufferUtil_impl.h"
