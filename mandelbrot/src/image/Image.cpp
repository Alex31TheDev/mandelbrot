#include "Image.h"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include <string>
#include <memory>

#ifdef _WIN32
#include <malloc.h>
#else
#include <limits.h>
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

static size_t alignTo(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

static uint8_t *bufferAlloc(size_t bufferSize) {
    void *ptr = nullptr;

    if constexpr (Image::ALIGNMENT > 8) {
        size_t alignedSize = alignTo(bufferSize, Image::ALIGNMENT);

#ifdef _WIN32
        ptr = _aligned_malloc(alignedSize, Image::ALIGNMENT);
#else
        if (posix_memalign(&ptr, Image::ALIGNMENT, alignedSize) != 0) {
            ptr = nullptr;
        }
#endif
    } else {
        ptr = malloc(bufferSize);
    }

    if (ptr != nullptr) memset(ptr, 0, bufferSize);
    return static_cast<uint8_t *>(ptr);
}

#ifdef USE_VECTORS
void Image::_AlignedDeleter::operator()(uint8_t *ptr) const {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}
#endif

std::unique_ptr<Image> Image::create(int32_t width, int32_t height) {
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
        STRIDE, _pixels.get(),
        _width * STRIDE);

    const std::string absPath = getAbsolutePath(filename);

    if (!result) {
        fprintf(stderr, "Failed to write PNG file: %s\n", absPath.c_str());
        return false;
    }

    printf("Successfully wrote: %s (%dx%d)\n", absPath.c_str(), _width, _height);
    return true;
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

    size_t bufferSize = static_cast<size_t>(width) * height * STRIDE;
    printf("Memoty required: %zu bytes\n", bufferSize);

    uint8_t *ptr = bufferAlloc(bufferSize);

    if (ptr == nullptr) {
        fprintf(stderr, "Failed to allocate pixel buffer. (%zu bytes)\n", bufferSize);
        return false;
    }

    _pixels.reset(ptr);

    _setDimensions(width, height);
    return true;
}