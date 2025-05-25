#include "RenderImage.h"

#include <vector>
#include <algorithm>
#include <functional>
#include <thread>

#include "../image/Image.h"
#include "RenderProgress.h"

#ifdef __AVX2__
#include "../vector/VectorRenderer.h"
#include "../vector/VectorGlobals.h"

using namespace VectorRenderer;
using namespace VectorGlobals;
#else
#include "../scalar/ScalarRenderer.h"
using namespace ScalarRenderer;
#endif
#include "../scalar/ScalarGlobals.h"
using namespace ScalarGlobals;

#include "../scalar/ScalarCoords.h"

static void renderStrip(Image &image, int start_y, int end_y, RenderProgress *progress = nullptr) {
    int pos = start_y * width * Image::STRIDE;

    for (int y = start_y; y < end_y; y++) {
        double ci = getCenterImag(y);

#ifdef __AVX2__
        for (int x = 0; x < width; x += SIMD_WIDTH) {
            int pixels_left = width - x;
            int simd_width = (pixels_left < SIMD_WIDTH ? pixels_left : SIMD_WIDTH);

            renderPixelSimd(image.pixels(), pos, simd_width, x, ci);
        }
#else
        for (int x = 0; x < width; x++) {
            renderPixelScalar(image.pixels(), pos, x, ci);
        }
#endif

        if (progress) progress->update();
    }
}

void renderImage(Image &image) {
    RenderProgress progress(height);
    renderStrip(image, 0, height, &progress);
    progress.complete();
}

void renderImageParallel(Image &image) {
    int threadCount = std::thread::hardware_concurrency();

    if (threadCount == 0) {
        renderImage(image);
        return;
    }

    RenderProgress progress(height);

    threadCount = std::min(threadCount, height);
    std::vector<std::thread> threads;

    int rowsPerThread = height / threadCount;
    int extraRows = height % threadCount;

    int start_y = 0;

    for (int i = 0; i < threadCount; ++i) {
        int chunkRows = rowsPerThread + (i < extraRows ? 1 : 0);
        int end_y = start_y + chunkRows;

        threads.emplace_back([&image, start_y, end_y, &progress]() {
            renderStrip(image, start_y, end_y, &progress); });

        start_y = end_y;
    }

    for (auto &t : threads) t.join();
    progress.complete();
}