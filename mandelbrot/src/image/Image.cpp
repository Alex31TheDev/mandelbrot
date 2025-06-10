#include "Image.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

#include <string>
#include <memory>

#include <iostream>
#include <fstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "../util/BufferUtil.h"
#include "../util/PathUtil.h"

size_t Image::calcBufferSize(int32_t width, int32_t height) {
    return static_cast<size_t>(width) * height * STRIDE;
}

static void stbi_write_callback(void *context, void *data, int size) {
    std::ostream *fout = static_cast<std::ostream *>(context);

    fout->write(static_cast<const char *>(data), size);
    fout->flush();
}

std::unique_ptr<Image> Image::create(int32_t width, int32_t height) {
    auto image = std::unique_ptr<Image>(new Image());

    if (image->_allocate(width, height)) {
        return image;
    } else {
        return nullptr;
    }
}

void Image::clear() {
    if (_pixels) memset(pixels(), 0, _bufferSize);
}

bool Image::writeToStream(std::ostream &fout) const {
    if (!_pixels) {
        fprintf(stderr, "Cannot write image. No pixel data allocated.\n");
        return false;
    }

    int result = stbi_write_png_to_func(
        stbi_write_callback,
        &fout,
        _width, _height,
        STRIDE, _pixels.get(),
        _width * STRIDE
    );

    if (result) {
        return true;
    } else {
        fprintf(stderr, "Failed to write PNG data to stream\n");
        return false;
    }
}

bool Image::saveToFile(const std::string &filename, bool appendDate) const {
    if (!_pixels) {
        fprintf(stderr, "Cannot save image. No pixel data allocated.\n");
        return false;
    }

    std::string absPath = PathUtil::getAbsolutePath(
        appendDate ? PathUtil::appendIsoDate(filename) : filename
    );

    std::ofstream fout(absPath, std::ios::binary);

    bool result = writeToStream(fout);

    if (result) {
        printf("Successfully saved: %s (%dx%d)\n", absPath.c_str(), _width, _height);
    } else {
        fprintf(stderr, "Failed to write PNG file: %s\n", absPath.c_str());
    }

    fout.close();
    return result;
}

void Image::_setDimensions(int32_t width, int32_t height) {
    _width = width;
    _height = height;

    _aspect = static_cast<float>(width) / height;
}

bool Image::_allocate(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid image dimensions. (%dx%d)\n", width, height);
        return false;
    }

    size_t originalSize = calcBufferSize(width, height);
    printf("Memoty required: %zu bytes\n", originalSize);

    uint8_t *ptr = BufferUtil::bufferAlloc<ALIGNMENT>(originalSize, &_bufferSize);

    if (!ptr) {
        fprintf(stderr, "Failed to allocate pixel buffer. (%zu bytes)\n", _bufferSize);
        return false;
    }

    _pixels.reset(ptr);

    _setDimensions(width, height);
    return true;
}