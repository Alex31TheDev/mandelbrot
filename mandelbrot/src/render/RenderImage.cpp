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
#elif defined(USE_VECTORS)
#include "../vector/VectorTypes.h"
#include "../vector/VectorRenderer.h"
#elif defined(USE_MPFR)
#include "../mpfr/MpfrRenderer.h"
#endif

#if defined(USE_SCALAR) || defined(USE_VECTORS)
#include "../scalar/ScalarCoords.h"
constexpr auto imagCenterCoord = getCenterImag;
#elif defined(USE_MPFR)
#include "../mpfr/MpfrCoords.h"
constexpr auto imagCenterCoord = getCenterImag_mp;
#endif

static void renderStrip(Image *image,
    int start_y, int end_y,
    RenderProgress *progress = nullptr) {
    int pos = start_y * width * Image::STRIDE;

    for (int y = start_y; y < end_y; y++) {
        const auto ci = imagCenterCoord(y);

#if defined(USE_SCALAR)
        for (int x = 0; x < width; x++) {
            ScalarRenderer::renderPixelScalar(image->pixels(), pos, x, ci);
        }
#elif defined(USE_VECTORS)
        for (int x = 0; x < width; x += SIMD_FULL_WIDTH) {
            int pixels_left = width - x;
            int simd_width = (pixels_left < SIMD_FULL_WIDTH ? pixels_left : SIMD_FULL_WIDTH);

            VectorRenderer::renderPixelSimd(image->pixels(), pos, simd_width, x, ci);
        }
#elif defined(USE_MPFR)
        for (int x = 0; x < width; x++) {
            MpfrRenderer::renderPixelMpfr(image->pixels(), pos, x, ci);
        }
#endif

        if (progress) progress->update();
    }
}

void renderImageSequential(Image *image) {
    RenderProgress progress(height);
    renderStrip(image, 0, height, &progress);
    progress.complete();
}

void renderImageParallel(Image *image) {
    int threadCount = std::thread::hardware_concurrency();

    if (threadCount == 0) {
        renderImageSequential(image);
        return;
    }

    RenderProgress progress(height);

    threadCount = std::min(threadCount, height);
    std::vector<std::thread> threads;

    int rowsPerThread = height / threadCount;
    int extraRows = height % threadCount;

    int start_y = 0;

    for (int i = 0; i < threadCount; i++) {
        int chunkRows = rowsPerThread + (i < extraRows ? 1 : 0);
        int end_y = start_y + chunkRows;

        threads.emplace_back([image, start_y, end_y, &progress]() {
            renderStrip(image, start_y, end_y, &progress); });

        start_y = end_y;
    }

    for (auto &thread : threads) {
        thread.join();
    }

    progress.complete();
}

void renderImage(Image *image) {
    if (useThreads) renderImageParallel(image);
    else renderImageSequential(image);
}