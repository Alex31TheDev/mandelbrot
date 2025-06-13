#include "RenderImage.h"

#include <vector>
#include <algorithm>
#include <thread>

#include "../image/Image.h"
#include "RenderProgress.h"

#include "RenderGlobals.h"
using namespace RenderGlobals;

#if defined(USE_SCALAR)
#include "../scalar/ScalarRenderer.h"
constexpr auto renderPixel = &ScalarRenderer::renderPixelScalar;
#elif defined(USE_VECTORS)
#include "../vector/VectorTypes.h"
#include "../vector/VectorRenderer.h"
constexpr auto renderPixel = &VectorRenderer::renderPixelSimd;
#elif defined(USE_MPFR)
#include "../mpfr/MpfrRenderer.h"
constexpr auto renderPixel = &MpfrRenderer::renderPixelMpfr;
#else
#error "No renderer implementation selected. (define USE_SCALAR, USE_VECTORS, or USE_MPFR)"
#endif

#if defined(USE_SCALAR) || defined(USE_VECTORS)
#include "../scalar/ScalarCoords.h"
constexpr auto imagCenterCoord = &getCenterImag;
#elif defined(USE_MPFR)
#include "../mpfr/MpfrCoords.h"
constexpr auto imagCenterCoord = &getCenterImag_mp;
#endif

static void renderStrip(Image *image,
    int start_y, int end_y,
    RenderProgress *progress = nullptr) {
    if (image == nullptr) return;
    int pos = start_y * width * Image::STRIDE;

    for (int y = start_y; y < end_y; y++) {
        const auto ci = imagCenterCoord(y);

#if defined(USE_SCALAR) || defined(USE_MPFR)
        for (int x = 0; x < width; x++) {
            renderPixel(image->pixels(), pos, x, ci);
        }
#elif defined(USE_VECTORS)
        for (int x = 0; x < width; x += SIMD_FULL_WIDTH) {
            int pixels_left = width - x;
            int simd_width = (pixels_left < SIMD_FULL_WIDTH ? pixels_left : SIMD_FULL_WIDTH);

            renderPixel(image->pixels(), pos, simd_width, x, ci);
        }
#endif

        if (progress) progress->update();
    }
}

void renderImageSequential(Image *image) {
    if (image == nullptr) return;

    RenderProgress progress(height);
    renderStrip(image, 0, height, &progress);
    progress.complete(true);
}

void renderImageParallel(Image *image) {
    if (image == nullptr) return;
    int threadCount = std::thread::hardware_concurrency();

    if (threadCount == 0) {
        renderImageSequential(image);
        return;
    }

    RenderProgress progress(height);

    threadCount = std::min(threadCount, height);
    std::vector<std::thread> threads;

    const int rowsPerThread = height / threadCount;
    const int extraRows = height % threadCount;

    int start_y = 0;

    for (int i = 0; i < threadCount; i++) {
        const int chunkRows = rowsPerThread + (i < extraRows ? 1 : 0);
        const int end_y = start_y + chunkRows;

        threads.emplace_back([image, start_y, end_y, &progress]() {
            renderStrip(image, start_y, end_y, &progress); });

        start_y = end_y;
    }

    for (auto &thread : threads) {
        thread.join();
    }

    progress.complete(true);
}

void renderImage(Image *image) {
    if (useThreads) renderImageParallel(image);
    else renderImageSequential(image);
}