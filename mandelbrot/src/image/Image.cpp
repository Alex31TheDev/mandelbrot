#include "Image.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

#include <string>
#include <memory>

#include <iostream>
#include <fstream>

#include "../util/WarningUtil.h"

PUSH_DISABLE_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
POP_DISABLE_WARNINGS

#include "BmpWriter.h"

#include "../util/fnv1a.h"
#include "../util/BufferUtil.h"
#include "../util/PathUtil.h"
#include "../util/FormatUtil.h"

size_t Image::_calcBufferSize(
    int32_t width, int32_t height,
    int32_t *strideWidth, int tailBytes
) {
    int32_t wbytes = width * STRIDE + tailBytes;

    if (strideWidth) *strideWidth = wbytes;
    return static_cast<size_t>(wbytes) * height;
}

size_t Image::calcBufferSize(
    int32_t width, int32_t height,
    int32_t *strideWidth
) {
    return _calcBufferSize(width, height, strideWidth, TAIL_BYTES);
}

static void stbi_write_callback(void *context, void *data, int size) {
    std::ostream *fout = static_cast<std::ostream *>(context);

    fout->write(static_cast<const char *>(data), size);
    fout->flush();
}

std::unique_ptr<Image> Image::create(
    int32_t width, int32_t height,
    bool simdSafe
) {
    auto image = std::unique_ptr<Image>(new Image(simdSafe));

    if (image->_allocate(width, height)) return image;
    else return nullptr;
}

Image::Image(bool simdSafe) {
    if (simdSafe) _tailBytes = TAIL_BYTES;
}

void Image::clear() {
    if (_pixels) memset(pixels(), 0, _bufferSize);
}

bool Image::writeToStream(
    std::ostream &fout,
    const std::string &type
) const {
    if (!_pixels) {
        fprintf(stderr, "Cannot write image. No pixel data allocated.\n");
        return false;
    }

    const uint8_t *pixelsPtr = _pixels.get();
    bool result = false;

    STR_SWITCH(type) {
        STR_CASE("png") :
            result = stbi_write_png_to_func(
                stbi_write_callback,
                &fout,
                _width, _height,
                STRIDE, pixelsPtr,
                _strideWidth
            );
        break;
        STR_CASE("jpg") :
            result = stbi_write_jpg_to_func(
                stbi_write_callback,
                &fout,
                _width, _height,
                STRIDE, pixelsPtr,
                _strideWidth
            );
        break;
        STR_CASE("bmp") :
            result = writeBmpStream(
                fout,
                pixelsPtr,
                _width, _height,
                STRIDE, _strideWidth
            );
        break;
        default:
            fprintf(stderr, "Invalid image type: %s\n", type.c_str());
            return false;
    }

    if (!result) {
        fprintf(stderr, "Failed to write %s data to stream\n", type.c_str());
    }

    return result;
}

bool Image::saveToFile(
    const std::string &filePath, bool appendDate,
    const std::string &type
) const {
    if (!_pixels) {
        fprintf(stderr, "Cannot save image. No pixel data allocated.\n");
        return false;
    }

    const std::string outPath = appendDate ?
        PathUtil::appendIsoDate(filePath) : filePath;
    const std::string absPath = PathUtil::getAbsolutePath(outPath);

    std::ofstream fout(absPath, std::ios::binary);
    const bool result = writeToStream(fout, type);
    fout.close();

    if (result) {
        printf("Successfully saved: %s (%dx%d)\n", absPath.c_str(),
            _width, _height);
    } else {
        fprintf(stderr, "Failed to write %s file: %s\n", type.c_str(),
            absPath.c_str());
    }

    return result;
}

void Image::_setDimensions(int32_t width, int32_t height) {
    _width = width;
    _height = height;

    _aspect = static_cast<float>(width) / height;
}

bool Image::_allocate(int32_t width, int32_t height) {
    if (_pixels) return false;

    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid image dimensions. (%dx%d)\n", width, height);
        return false;
    }

    const size_t originalSize = _calcBufferSize(width, height,
        &_strideWidth, _tailBytes);

    printf("Memory required: %s\n",
        FormatUtil::formatBufferSize(originalSize).c_str());

    uint8_t *ptr = BufferUtil::bufferAlloc<ALIGNMENT>
        (originalSize, &_bufferSize);

    if (!ptr) {
        fprintf(stderr, "Failed to allocate pixel buffer. (%zu bytes)\n",
            _bufferSize);
        return false;
    }

    _pixels.reset(ptr);

    _setDimensions(width, height);
    return true;
}