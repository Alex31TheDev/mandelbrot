#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <tuple>
#include <optional>
#include <functional>
#include <iostream>

#include "BackendAPI.h"

#ifdef USE_VECTORS
#include "vector/VectorTypes.h"
#endif

#include "util/BufferUtil.h"

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

    static constexpr float MAX_AA_SCALE = 8.0f;

    static size_t calcBufferSize(
        int32_t width, int32_t height,
        std::optional<std::reference_wrapper<int32_t>> strideWidth = std::nullopt
    );

    static float calcAAScale(int32_t aaPixels);
    static std::tuple<int32_t, int32_t> calcRenderDimensions(
        int32_t width, int32_t height, float aaScale
    );

    static std::unique_ptr<Image> create(int32_t width, int32_t height,
        bool simdSafe = true, int32_t aaPixels = 1);

    void setCallbacks(const Backend::Callbacks *callbacks) {
        _callbacks = callbacks;

        if (_callbacks && _pixels) {
            _emitImageEvent(Backend::ImageEventKind::allocated);
        }
    }

    [[nodiscard]] int32_t width() const { return _width; }
    [[nodiscard]] int32_t height() const { return _height; }
    [[nodiscard]] float aspect() const { return _aspect; }

    [[nodiscard]] float aaScale() const { return _aaScale; }
    [[nodiscard]] bool downscaling() const { return _downscaling; }

    [[nodiscard]] int32_t outputWidth() const { return _outputW; }
    [[nodiscard]] int32_t outputHeight() const { return _outputH; }

    [[nodiscard]] int32_t tailBytes() const { return _tailBytes; }
    [[nodiscard]] int32_t strideWidth() const { return _strideW; }

    [[nodiscard]] size_t size() const { return _bufferSize; }
    [[nodiscard]] uint8_t *pixels() { return _pixels.get(); }
    [[nodiscard]] const uint8_t *pixels() const { return _pixels.get(); }
    [[nodiscard]] const uint8_t *outputPixels() const;

    void resolve() const;
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
        std::optional<std::reference_wrapper<int32_t>> strideWidth,
        int32_t tailBytes);

    explicit Image(bool simdSafe, int32_t aaPixels);
    const Backend::Callbacks *_callbacks = nullptr;

    bool _downscaling = false;
    mutable bool _resolved = true;
    float _aaScale = 1.0f;

    int32_t _width = 0, _height = 0;
    int32_t _outputW = 0, _outputH = 0;
    float _aspect = 0;

    int32_t _tailBytes = 0, _strideW = 0;
    size_t _bufferSize = 0;

    int32_t _outputStrideW = 0;
    size_t _outputSize = 0;

    BufferUtil::AlignedBuffer<ALIGNMENT> _pixels;
    std::unique_ptr<uint8_t[]> _outputPixels;

    [[nodiscard]] int32_t _getOutputStrideW() const {
        return _downscaling ? _outputStrideW : _strideW;
    }
    [[nodiscard]] const uint8_t *_getOutputBuffer() const {
        return _downscaling ? _outputPixels.get() : _pixels.get();
    }

    void _emitImageEvent(Backend::ImageEventKind kind,
        const char *path = nullptr) const;

    void _setDimensions(int32_t width, int32_t height);
    bool _allocate(int32_t width, int32_t height);
};
