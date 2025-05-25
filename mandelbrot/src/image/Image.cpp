#include "Image.h"

#include <cstdlib>
#include <cstdio>
#include <cstdint>

#include <string>
#include <memory>

#ifdef _WIN32
#include <malloc.h>
#else
#include <limits.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static size_t alignTo(size_t size, size_t alignment) {
    return (size + alignment - 1) / alignment * alignment;
}

static std::string getAbsolutePath(const char *filename) {
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
        _width, _height,
        STRIDE, _pixels.get(), _width * STRIDE
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

    _aspect = static_cast<float>(width) / height;
}

bool Image::_allocate(int width, int height) {
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "Invalid image dimensions. (%dx%d)\n", width, height);
        return false;
    }

    size_t bufferSize = static_cast<size_t>(width) * height * STRIDE;
    size_t alignedSize = alignTo(bufferSize, ALIGNMENT);

    void *ptr = nullptr;

#ifdef _WIN32
    ptr = _aligned_malloc(alignedSize, ALIGNMENT);
#else
    ptr = std::aligned_alloc(ALIGNMENT, alignedSize);
#endif

    if (!ptr) {
        fprintf(stderr, "Failed to allocate pixel buffer. (%zu bytes)\n", bufferSize);
        return false;
    }

    _pixels.reset(reinterpret_cast<uint8_t *>(ptr));

    _setDimensions(width, height);
    return true;
}

void Image::_AlignedDeleter::operator()(uint8_t *ptr) const {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}