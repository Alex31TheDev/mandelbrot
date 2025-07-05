#pragma once

#include <cstdint>

#include <string>
#include <memory>
#include <iostream>

#ifdef USE_VECTORS
#include "../vector/VectorTypes.h"
#endif

#include "../util/BufferUtil.h"

class Image {
public:
    static constexpr int STRIDE = 3;

#ifdef USE_VECTORS
    static constexpr int ALIGNMENT = SIMD_HALF_ALIGNMENT;
    static constexpr int TAIL_BYTES = SIMD_HALF_WIDTH;
#else
    static constexpr int ALIGNMENT = 0;
    static constexpr int TAIL_BYTES = 0;
#endif

    static size_t calcBufferSize(int32_t width, int32_t height,
        int32_t *strideWidth = nullptr);
    static std::unique_ptr<Image> create(int32_t width, int32_t height,
        bool simdSafe = true);

    [[nodiscard]] int32_t width() const { return _width; }
    [[nodiscard]] int32_t height() const { return _height; }
    [[nodiscard]] float aspect() const { return _aspect; }

    [[nodiscard]] int32_t tailBytes() const { return _tailBytes; }
    [[nodiscard]] int32_t strideWidth() const { return _strideWidth; }

    [[nodiscard]] size_t size() const { return _bufferSize; }
    [[nodiscard]] uint8_t *pixels() { return _pixels.get(); }
    [[nodiscard]] const uint8_t *pixels() const { return _pixels.get(); }

    void clear();

    bool writeToStream(std::ostream &fout,
        const std::string &type = "png") const;
    bool saveToFile(const std::string &filePath, bool appendDate = false,
        const std::string &type = "png") const;

    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    Image(Image &&) = default;
    Image &operator=(Image &&) = default;

private:
    static size_t _calcBufferSize(int32_t width, int32_t height,
        int32_t *strideWidth, int32_t tailBytes);

    explicit Image(bool simdSafe);

    int32_t _width = 0, _height = 0;
    float _aspect = 0;

    int32_t _tailBytes = 0, _strideWidth = 0;
    size_t _bufferSize = 0;

    std::unique_ptr<
        uint8_t[],
        BufferUtil::AlignedDeleter<ALIGNMENT>
    > _pixels;

    void _setDimensions(int32_t width, int32_t height);
    bool _allocate(int32_t width, int32_t height);
};