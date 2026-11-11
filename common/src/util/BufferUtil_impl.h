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

    template <typename T, size_t ALIGNMENT>
    T *bufferAlloc(
        size_t count,
        std::optional<std::reference_wrapper<size_t>> alignedSize
    ) {
        static_assert(std::is_trivial_v<T>);
        static_assert(ALIGNMENT == 0 || ALIGNMENT >= alignof(T));

        size_t bufSize = count * sizeof(T);
        size_t newSize;
        void *ptr = nullptr;

        if constexpr (ALIGNMENT > 8) {
            newSize = bufSize % ALIGNMENT == 0
                ? bufSize : alignTo(bufSize, ALIGNMENT);
            if (alignedSize) alignedSize->get() = newSize;

#ifdef _WIN32
            ptr = _aligned_malloc(newSize, ALIGNMENT);
#else
            if (posix_memalign(&ptr, ALIGNMENT, newSize) != 0) ptr = nullptr;
#endif
        } else {
            newSize = bufSize;
            if (alignedSize) alignedSize->get() = newSize;

            ptr = malloc(bufSize);
        }

        if (ptr) memset(ptr, 0, newSize);
        return static_cast<T *>(ptr);
    }

    template <typename T, size_t ALIGNMENT>
    void bufferFree(T *ptr) noexcept {
#if defined(_WIN32)
        if constexpr (ALIGNMENT > 8) {
            _aligned_free(ptr);
        } else
#endif
            free(ptr);
    }

    template <typename T, size_t ALIGNMENT>
    void AlignedDeleter<T, ALIGNMENT>::operator()(T *ptr) noexcept {
        bufferFree<T, ALIGNMENT>(ptr);
    }

    template <typename T, size_t ALIGNMENT>
    AlignedBuffer<T, ALIGNMENT> bufferAllocPtr(
        size_t count,
        std::optional<std::reference_wrapper<size_t>> alignedSize
    ) {
        return AlignedBuffer<T, ALIGNMENT>(bufferAlloc<T, ALIGNMENT>(count, alignedSize));
    }
}
