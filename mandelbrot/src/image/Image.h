#pragma once

#include <cstdint>

#include <string>
#include <memory>
#include <iostream>

#ifdef USE_VECTORS
#include "../vector/VectorTypes.h"
#else
static constexpr int SIMD_HALF_ALIGNMENT = 0;
#endif

#include "../util/BufferUtil.h"

class Image {
public:
    static constexpr int STRIDE = 3;
    static constexpr int ALIGNMENT = SIMD_HALF_ALIGNMENT;

    static size_t calcBufferSize(int32_t width, int32_t height);
    static std::unique_ptr<Image> create(int32_t width, int32_t height);

    int32_t width() const { return _width; }
    int32_t height() const { return _height; }
    float aspect() const { return _aspect; }

    uint8_t *pixels() { return _pixels.get(); }
    const uint8_t *pixels() const { return _pixels.get(); }

    void clear();

    bool writeToStream(std::ostream &output) const;
    bool saveToFile(const std::string &filename, bool appendDate = false) const;

    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    Image(Image &&) = default;
    Image &operator=(Image &&) = default;

private:
    Image() = default;

    int32_t _width = 0, _height = 0;
    float _aspect = 0;

    std::unique_ptr<
        uint8_t[],
        BufferUtil::AlignedDeleter<ALIGNMENT>
    > _pixels;
    size_t _bufferSize = 0;

    void _setDimensions(int32_t width, int32_t height);
    bool _allocate(int32_t width, int32_t height);
};