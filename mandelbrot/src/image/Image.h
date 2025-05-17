#pragma once

#include <cstdint>
#include <memory>

class Image {
public:
    static constexpr int STRIDE = 3;

    static std::unique_ptr<Image> create(int width, int height);

    int width() const { return _width; }
    int height() const { return _height; }
    float aspect() const { return _aspect; }

    uint8_t *pixels() { return _pixels.get(); }
    const uint8_t *pixels() const { return _pixels.get(); }

    bool saveToFile(const char *filename) const;

    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    Image(Image &&) = default;
    Image &operator=(Image &&) = default;

private:
    Image() = default;

    int _width = 0;
    int _height = 0;
    float _aspect = 0;

    std::unique_ptr<uint8_t[]> _pixels;

    void _setDimensions(int width, int height);
    bool _allocate(int width, int height);
};