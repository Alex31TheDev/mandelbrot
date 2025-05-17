#include "Image.h"

#include <cstdio>
#include <cstdint>
#include <string>
#include <memory>
#include <exception>

#ifdef _WIN32
#include <cstdlib>
#else
#include <limits.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {
    std::string getAbsolutePath(const char *filename) {
#ifdef _WIN32
        char absPath[_MAX_PATH] = { 0 };

        if (_fullpath(absPath, filename, _MAX_PATH) != nullptr) {
            return absPath;
        }
#else
        char *resolved = realpath(filename, nullptr);

        if (resolved != nullptr) {
            std::string result(resolved);
            free(resolved);

            return result;
        }
#endif
        return filename;
    }
}

std::unique_ptr<Image> Image::create(int width, int height) {
    auto image = std::unique_ptr<Image>(new Image());

    if (!image->_allocate(width, height)) {
        return nullptr;
    }

    return image;
}

bool Image::saveToFile(const char *filename) const {
    if (!_pixels) {
        fprintf(stderr, "Cannot save image. No pixel data allocated.\n");
        return false;
    }

    const int result = stbi_write_png(
        filename,
        _width,
        _height,
        STRIDE,
        _pixels.get(),
        _width * STRIDE
    );

    const std::string absPath = getAbsolutePath(filename);

    if (!result) {
        fprintf(stderr, "Failed to write PNG file: %s\n", absPath.c_str());
        return false;
    }

    printf("Successfully wrote: %s (%dx%d)\n", absPath.c_str(), _width, _height);
    return true;
}

void Image::_setDimensions(int width, int height) {
    _width = width;
    _height = height;

    _aspect = (float)width / (float)height;
}

bool Image::_allocate(int width, int height) {
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid image dimensions. (%dx%d)\n", width, height);
        return false;
    }

    const size_t bufferSize = (size_t)width * height * STRIDE;

    try {
        _pixels = std::make_unique<uint8_t[]>(bufferSize);
    } catch (const std::bad_alloc &) {
        fprintf(stderr, "Failed to allocate pixel buffer. (%zu bytes)\n", bufferSize);
        return false;
    }

    _setDimensions(width, height);
    return true;
}