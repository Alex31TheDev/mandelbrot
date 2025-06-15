#include "BmpWriter.h"

#include <cstdint>
#include <iostream>

#include "../util/BufferUtil.h"

constexpr int STRIDE = 3;
constexpr int ALIGNMENT = 4;
constexpr size_t MAX_CHUNK_SIZE = 1024;

#pragma pack(push, 1)
struct BmpHeader {
    uint8_t type[2];
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
};

struct DibHeader {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t imageSize;
    int32_t xPixelsPerMeter;
    int32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t importantColors;
};
#pragma pack(pop)

static_assert(sizeof(BmpHeader) == 14, "Invalid BmpHeader size");
static_assert(sizeof(DibHeader) == 40, "Invalid DibHeader size");

bool writeBmpStream(std::ostream &fout, const uint8_t *pixels,
    int32_t width, int32_t height) {
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    const size_t rowSize = BufferUtil::alignTo(
        static_cast<size_t>(width) * STRIDE, static_cast<size_t>(ALIGNMENT)
    );

    const size_t dataSize = rowSize * height;

    const BmpHeader bmpHeader = {
       .type = { 'B', 'M' },
       .size = static_cast <uint32_t>(
           sizeof(BmpHeader) + sizeof(DibHeader) + dataSize
           ),
       .offset = static_cast<uint32_t>(
           sizeof(BmpHeader) + sizeof(DibHeader)
           )
    };

    const DibHeader dibHeader = {
        .size = sizeof(DibHeader),
        .width = width,
        .height = height,
        .planes = 1,
        .bitsPerPixel = 8 * STRIDE,
        .imageSize = static_cast<uint32_t>(dataSize)
    };

    fout.write(reinterpret_cast<const char *>(&bmpHeader), sizeof(BmpHeader));
    if (!fout) return false;
    fout.write(reinterpret_cast<const char *>(&dibHeader), sizeof(DibHeader));
    if (!fout) return false;

    const uint8_t padding[STRIDE] = { 0 };
    const size_t paddingSize = rowSize - (static_cast<size_t>(width) * STRIDE);

    uint8_t bgrRow[MAX_CHUNK_SIZE * STRIDE] = { 0 };

    for (int32_t y = height - 1; y >= 0; y--) {
        const uint8_t *row = pixels + y * width * STRIDE;
        int32_t pixelsRemaining = width;

        while (pixelsRemaining > 0) {
            const size_t chunkSize = (pixelsRemaining > MAX_CHUNK_SIZE)
                ? MAX_CHUNK_SIZE : pixelsRemaining;
            const uint8_t *src = row + (width - pixelsRemaining) * STRIDE;

            for (size_t i = 0; i < chunkSize; i++) {
                bgrRow[i * STRIDE + 0] = src[i * STRIDE + 2];
                bgrRow[i * STRIDE + 1] = src[i * STRIDE + 1];
                bgrRow[i * STRIDE + 2] = src[i * STRIDE + 0];
            }

            fout.write(reinterpret_cast<const char *>(bgrRow),
                chunkSize * STRIDE);
            if (!fout) return false;

            pixelsRemaining -= static_cast<int32_t>(chunkSize);
        }

        if (paddingSize > 0) {
            fout.write(reinterpret_cast<const char *>(padding), paddingSize);
            if (!fout) return false;
        }
    }

    fout.flush();
    return true;
}