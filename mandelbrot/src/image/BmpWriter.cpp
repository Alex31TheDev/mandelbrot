#include "BmpWriter.h"

#include <cstdint>
#include <iostream>

#include "../util/BufferUtil.h"

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

static const uint8_t padding[BMP_ALIGNMENT - 1] = { 0 };

bool writeBmpStream(
    std::ostream &fout, const uint8_t *pixels,
    int32_t width, int32_t height,
    int32_t stride, int32_t strideWidth
) {
    if (!pixels || width <= 0 || height <= 0 || stride < 3) {
        return false;
    }

    if (strideWidth == 0) strideWidth = width * stride;
    else if (strideWidth < width * stride) return false;

    const size_t bmpStrideWidth = static_cast<size_t>(width) * BMP_STRIDE;

    const size_t rowSize = BufferUtil::alignTo(bmpStrideWidth,
        static_cast<size_t>(BMP_ALIGNMENT));
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
        .bitsPerPixel = 8 * BMP_STRIDE,
        .imageSize = static_cast<uint32_t>(dataSize)
    };

    fout.write(reinterpret_cast<const char *>(&bmpHeader), sizeof(BmpHeader));
    if (!fout) return false;

    fout.write(reinterpret_cast<const char *>(&dibHeader), sizeof(DibHeader));
    if (!fout) return false;

    const size_t paddingSize = rowSize - bmpStrideWidth;
    uint8_t bgrRow[MAX_BMP_CHUNK_SIZE * BMP_STRIDE] = { 0 };

    for (int32_t y = height - 1; y >= 0; y--) {
        const uint8_t *row = pixels + y * strideWidth;
        int32_t pixelsRemaining = width;

        while (pixelsRemaining > 0) {
            const size_t chunkSize = (pixelsRemaining > MAX_BMP_CHUNK_SIZE)
                ? MAX_BMP_CHUNK_SIZE : pixelsRemaining;
            const uint8_t *src = row + (width - pixelsRemaining) * stride;

            for (size_t i = 0; i < chunkSize; i++) {
                bgrRow[i * BMP_STRIDE + 0] = src[i * stride + 2];
                bgrRow[i * BMP_STRIDE + 1] = src[i * stride + 1];
                bgrRow[i * BMP_STRIDE + 2] = src[i * stride + 0];
            }

            fout.write(reinterpret_cast<const char *>(bgrRow),
                chunkSize * BMP_STRIDE);
            if (!fout) return false;

            pixelsRemaining -= static_cast<int32_t>(chunkSize);
        }

        if (paddingSize > 0) {
            fout.write(reinterpret_cast<const char *>(padding), paddingSize);
            if (!fout) return false;
        }
    }

    return !!fout.flush();
}