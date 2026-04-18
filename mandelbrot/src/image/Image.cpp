#include "Image.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>

#include <string>
#include <memory>

#include <iostream>
#include <fstream>

#include "util/WarningUtil.h"

PUSH_DISABLE_WARNINGS
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
POP_DISABLE_WARNINGS

#include "BmpWriter.h"

#include "util/fnv1a.h"
#include "util/BufferUtil.h"
#include "util/PathUtil.h"
#include "util/FormatUtil.h"

inline float clampf(float x, float a, float b) {
    return fmaxf(a, fminf(x, b));
}

static void boxDownscale(
    const uint8_t *src, int srcW, int srcH, int srcStrideW,
    uint8_t *dst, int dstW, int dstH, int dstStrideW
) {
    float scaleX = (float)srcW / dstW,
        scaleY = (float)srcH / dstH;

    for (int dy = 0; dy < dstH; dy++) {
        float y0 = dy * scaleY,
            y1 = (dy + 1) * scaleY;

        int sy0 = static_cast<int>(floorf(y0)),
            sy1 = static_cast<int>(ceilf(y1));

        for (int dx = 0; dx < dstW; dx++) {
            float x0 = dx * scaleX,
                x1 = (dx + 1) * scaleX;

            int sx0 = static_cast<int>(floorf(x0)),
                sx1 = static_cast<int>(ceilf(x1));

            float r, g, b,
                areaSum = 0;
            r = g = b = 0;

            for (int sy = sy0; sy < sy1; sy++) {
                if (sy < 0 || sy >= srcH) continue;

                float oy0 = fmaxf(y0, static_cast<float>(sy)),
                    oy1 = fminf(y1, static_cast<float>(sy) + 1.0f);

                float wy = oy1 - oy0;
                if (wy <= 0) continue;

                for (int sx = sx0; sx < sx1; sx++) {
                    if (sx < 0 || sx >= srcW) continue;

                    float ox0 = fmaxf(x0, static_cast<float>(sx)),
                        ox1 = fminf(x1, static_cast<float>(sx) + 1.0f);

                    float wx = ox1 - ox0;
                    if (wx <= 0) continue;

                    float w = wx * wy;
                    int si = sy * srcStrideW + sx * Image::STRIDE;

                    r += src[si] * w;
                    g += src[si + 1] * w;
                    b += src[si + 2] * w;

                    areaSum += w;
                }
            }

            int di = dy * dstStrideW + dx * Image::STRIDE;

            if (areaSum > 0) {
                float inv = 1.0f / areaSum;

                dst[di] = static_cast<uint8_t>(
                    clampf(r * inv + 0.5f, 0.0f, 255.0f)
                    );
                dst[di + 1] = static_cast<uint8_t>(
                    clampf(g * inv + 0.5f, 0.0f, 255.0f)
                    );
                dst[di + 2] = static_cast<uint8_t>(
                    clampf(b * inv + 0.5f, 0.0f, 255.0f)
                    );
            } else {
                dst[di] = dst[di + 1] = dst[di + 2] = 0;
            }
        }
    }
}

size_t Image::_calcBufferSize(
    int32_t width, int32_t height,
    std::optional<std::reference_wrapper<int32_t>> strideWidth,
    int tailBytes
) {
    int32_t wbytes = width * STRIDE + tailBytes;

    if (strideWidth) strideWidth->get() = wbytes;
    return static_cast<size_t>(wbytes) * height;
}

size_t Image::calcBufferSize(
    int32_t width, int32_t height,
    std::optional<std::reference_wrapper<int32_t>> strideWidth
) {
    return _calcBufferSize(width, height, strideWidth, TAIL_BYTES);
}

float Image::calcAAScale(int32_t aaPixels) {
    return clampf(sqrtf(static_cast<float>(aaPixels)), 1.0f, MAX_AA_SCALE);
}

std::tuple<int32_t, int32_t> Image::calcRenderDimensions(
    int32_t width, int32_t height, float aaScale
) {
    return {
        static_cast<int32_t>(
            fmaxf(1.0f, roundf(static_cast<float>(width) * aaScale))
        ),
        static_cast<int32_t>(
            fmaxf(1.0f, roundf(static_cast<float>(height) * aaScale))
        )
    };
}

static void stbi_write_callback(void *context, void *data, int size) {
    std::ostream *fout = static_cast<std::ostream *>(context);

    fout->write(static_cast<const char *>(data), size);
    fout->flush();
}

std::unique_ptr<Image> Image::create(
    int32_t width, int32_t height,
    bool simdSafe, int32_t aaPixels
) {
    auto image = std::unique_ptr<Image>(new Image(simdSafe, aaPixels));

    if (image->_allocate(width, height)) return image;
    else return nullptr;
}

Image::Image(bool simdSafe, int32_t aaPixels) {
    if (simdSafe) _tailBytes = TAIL_BYTES;
    _aaScale = calcAAScale(aaPixels);
}

const uint8_t *Image::outputPixels() const {
    if (!_resolved) resolve();
    return _getOutputBuffer();
}

void Image::clear() {
    if (_pixels) memset(_pixels.get(), 0, _bufferSize);
    if (_outputPixels) memset(_outputPixels.get(), 0, _outputSize);
    _resolved = !_downscaling;
}

void Image::resolve() const {
    if (!_downscaling || !_pixels) return;

    boxDownscale(
        _pixels.get(), _width, _height, _strideW,
        _outputPixels.get(), _outputW, _outputH, _outputStrideW
    );

    _resolved = true;
}

bool Image::writeToStream(
    std::ostream &fout,
    const std::string &type
) const {
    if (!_pixels) return false;

    bool result = false;

    STR_SWITCH(type) {
        STR_CASE("png") :
            result = stbi_write_png_to_func(
                stbi_write_callback,
                &fout,
                _outputW, _outputH,
                STRIDE, outputPixels(),
                _getOutputStrideW()
            );
        break;

        STR_CASE("jpg") :
            result = stbi_write_jpg_to_func(
                stbi_write_callback,
                &fout,
                _outputW, _outputH,
                STRIDE, outputPixels(), 100
            );
        break;

        STR_CASE("bmp") :
            result = writeBmpStream(
                fout,
                outputPixels(),
                _outputW, _outputH,
                STRIDE, _getOutputStrideW()
            );
        break;

        default:
            return false;
    }

    return result;
}

bool Image::saveToFile(
    const std::string &filePath, bool appendDate,
    const std::string &type
) const {
    if (!_pixels) return false;

    const std::string outPath = appendDate ?
        PathUtil::appendIsoDate(filePath) : filePath;
    const std::string absPath = PathUtil::getAbsolutePath(outPath);

    std::ofstream fout(absPath, std::ios::binary);
    const bool result = writeToStream(fout, type);
    fout.close();

    if (result) _emitImageEvent(Backend::ImageEventKind::saved, absPath.c_str());

    return result;
}

void Image::_emitImageEvent(Backend::ImageEventKind kind, const char *path) const {
    if (!_callbacks || !_callbacks->onImage) return;

    const Backend::ImageEvent event = {
        .kind = kind,
        .aaScale = _aaScale,
        .aspect = _aspect,
        .downscaling = _downscaling,
        .width = _width,
        .height = _height,
        .outputWidth = _outputW,
        .outputHeight = _outputH,
        .primaryBytes = _bufferSize,
        .secondaryBytes = _outputSize,
        .path = path
    };

    _callbacks->onImage(event);
}

void Image::_setDimensions(int32_t width, int32_t height) {
    _outputW = width;
    _outputH = height;

    std::tie(_width, _height) = calcRenderDimensions(width, height, _aaScale);

    _downscaling = (_width != _outputW) || (_height != _outputH);
    _aspect = static_cast<float>(width) / height;
}

bool Image::_allocate(int32_t width, int32_t height) {
    if (_pixels) return false;

    if (width <= 0 || height <= 0) return false;

    _setDimensions(width, height);
    _resolved = !_downscaling;

    const size_t originalSize = _calcBufferSize(_width, _height,
        _strideW, _tailBytes);

    if (_downscaling) {
        _outputSize = _calcBufferSize(_outputW, _outputH, _outputStrideW, 0);
    }

    uint8_t *ptr = BufferUtil::bufferAlloc<ALIGNMENT>
        (originalSize, _bufferSize);
    if (!ptr) return false;
    _pixels.reset(ptr);

    if (_downscaling) {
        _outputPixels = std::make_unique<uint8_t[]>(_outputSize);
    }

    _emitImageEvent(Backend::ImageEventKind::allocated);

    return true;
}