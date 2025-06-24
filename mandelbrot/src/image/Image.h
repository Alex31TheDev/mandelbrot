#pragma once

#include <cstdint>

#include <string>
#include <memory>
#include <iostream>

#ifdef USE_VECTORS
#include "../vector/VectorTypes.h"
#else
constexpr int SIMD_HALF_WIDTH = 0;
constexpr int SIMD_HALF_ALIGNMENT = 0;
#endif

#include "../util/BufferUtil.h"

class Image {
public:
    static constexpr int STRIDE = 3;
    static constexpr int ALIGNMENT = SIMD_HALF_ALIGNMENT;

    static size_t calcBufferSize(int32_t width, int32_t height,
        int32_t *strideWidth = nullptr);
    static std::unique_ptr<Image> create(int32_t width, int32_t height);

    friend std::unique_ptr<Image> std::make_unique<Image>();

    [[nodiscard]] int32_t width() const noexcept { return _width; }
    [[nodiscard]] int32_t height() const noexcept { return _height; }
    [[nodiscard]] float aspect() const noexcept { return _aspect; }

    [[nodiscard]] int32_t strideWidth() const noexcept { return _strideWidth; }
    [[nodiscard]] size_t size() const noexcept { return _bufferSize; }
    [[nodiscard]] uint8_t *pixels() noexcept { return _pixels.get(); }
    [[nodiscard]] const uint8_t *
        pixels() const noexcept { return _pixels.get(); }

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
    Image() = default;

    int32_t _width = 0, _height = 0;
    float _aspect = 0;

    int32_t _strideWidth = 0;
    size_t _bufferSize = 0;

    std::unique_ptr<
        uint8_t[],
        BufferUtil::AlignedDeleter<ALIGNMENT>
    > _pixels;

    void _setDimensions(int32_t width, int32_t height);
    bool _allocate(int32_t width, int32_t height);
};