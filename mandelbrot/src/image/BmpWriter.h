#pragma once

#include <cstdint>
#include <iostream>

constexpr int BMP_STRIDE = 3;
constexpr int BMP_ALIGNMENT = 4;
constexpr size_t MAX_BMP_CHUNK_SIZE = 1024;

bool writeBmpStream(std::ostream &fout, const uint8_t *pixels,
    int32_t width, int32_t height,
    int32_t stride = 3, int32_t strideWidth = 0);
