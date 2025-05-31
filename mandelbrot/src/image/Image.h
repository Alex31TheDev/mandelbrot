#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>

#ifdef USE_VECTORS
#include "../vector/VectorTypes.h"
#else
static constexpr int SIMD_HALF_ALIGNMENT = 0;
#endif

class Image {
public:
    static constexpr int STRIDE = 3;
    static constexpr int ALIGNMENT = SIMD_HALF_ALIGNMENT;

    static std::unique_ptr<Image> create(int32_t width, int32_t height);

    int32_t width() const { return _width; }
    int32_t height() const { return _height; }
    float aspect() const { return _aspect; }

    uint8_t *pixels() { return _pixels.get(); }
    const uint8_t *pixels() const { return _pixels.get(); }

    bool saveToFile(const char *filename) const;

    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    Image(Image &&) = default;
    Image &operator=(Image &&) = default;

private:
    Image() = default;

    int32_t _width = 0, _height = 0;
    float _aspect = 0;

    struct _AlignedDeleter {
        void operator()(uint8_t *ptr) const;
    };

    using _storage_t = std::conditional_t<
        (ALIGNMENT > 8),
        std::unique_ptr<uint8_t[], _AlignedDeleter>,
        std::unique_ptr<uint8_t[]>>;
    _storage_t _pixels;

    void _setDimensions(int32_t width, int32_t height);
    bool _allocate(int32_t width, int32_t height);
};